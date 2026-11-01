#include <Pch.h>
#include <NetworkCore/Listener.h>
#include <NetworkCore/ServerService.h>
#include <NetworkCore/CoreRuntime.h>
#include <NetworkCore/Session.h>
#include <NetworkCore/SocketUtils.h>

Listener::Listener(CoreRuntime& runtime)
    : iocpCore(runtime.GetIocpCore()), jobs(runtime.CreateJobQueue())
{
}

Listener::~Listener()
{
	SocketUtils::Close(socket);
}

HANDLE Listener::GetHandle() const
{
	return reinterpret_cast<HANDLE>(socket);
}

std::shared_ptr<Listener> Listener::GetListenerPtr()
{
	return std::static_pointer_cast<Listener>(shared_from_this());
}

Result Listener::StartAccept(const ServerServicePtr& _service)
{
	if (!_service || !_service->GetNetAddress().IsValid())
	{
		return std::unexpected("Invalid listener service/address");
	}

	service = _service;
	socket = SocketUtils::CreateSocket();
	const BOOL exclusive = TRUE;
	if (socket == INVALID_SOCKET || !iocpCore->Register(shared_from_this()) ||
	    ::setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0 ||
	    !SocketUtils::Bind(socket, _service->GetNetAddress()) || !SocketUtils::Listen(socket))
	{
		return std::unexpected("Starting listener failed: " + std::to_string(::WSAGetLastError()));
	}

	SOCKADDR_IN local{};
	int length = sizeof(local);
	if (::getsockname(socket, reinterpret_cast<SOCKADDR*>(&local), &length) != 0)
	{
		return std::unexpected("getsockname failed");
	}

	address = NetAddress(local);
	const auto count = std::min<std::size_t>(32, _service->GetMaxSessionCount());
	for (std::size_t i = 0; i < count; ++i)
	{
		acceptEvents.push_back(std::make_unique<AcceptEvent>());
		if (!RegisterAccept(acceptEvents.back().get()))
		{
			return std::unexpected("Creating or registering accept session failed");
		}
	}

	return {};
}

void Listener::CloseSocket()
{
	if (closing.exchange(true))
	{
		return;
	}

	jobs->DoAsync([self = GetListenerPtr()]
	{
		::CancelIoEx(self->GetHandle(), nullptr);
		SocketUtils::Close(self->socket);
		for (const auto& event : self->acceptEvents)
		{
			if (event->session)
			{
				event->session->Disconnect();
			}
		}
	});
}

void Listener::Dispatch(IocpEvent* event, uint32, DWORD error)
{
	jobs->DoAsync([self = GetListenerPtr(), event, error]
	{
		self->ProcessAccept(static_cast<AcceptEvent*>(event), error);
	});
}

bool Listener::RegisterAccept(AcceptEvent* event)
{
	auto _service = service.lock();
	if (closing || !_service || _service->IsClosing())
	{
		return false;
	}

	event->session = _service->CreateSession();
	if (!event->session)
	{
		_service->GetRuntime().GetGlobalQueue().RecordFailure("Creating or registering accept session failed");
		return false;
	}

	event->Init();
	event->owner = shared_from_this();
	iocpCore->OperationStarted();
	DWORD bytes = 0;
	if (!SocketUtils::AcceptEx(socket, event->session->GetSocket(), event->addresses.data(), 0,
	                           sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, &bytes, event))
	{
		const DWORD error = ::WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			event->owner.reset();
			event->session.reset();
			iocpCore->OperationCompleted();
			RetryAccept(event);
		}
	}

	return true;
}

void Listener::ProcessAccept(AcceptEvent* event, DWORD error)
{
	auto session = std::move(event->session);
	auto _service = service.lock();
	if (closing || !_service || _service->IsClosing())
	{
		if (session)
		{
			session->Disconnect();
		}

		return;
	}

	if (error)
	{
		RetryAccept(event);
		return;
	}

	SOCKADDR_IN remote{};
	int length = sizeof(remote);
	if (!SocketUtils::SetUpdateAcceptSocket(session->GetSocket(), socket) ||
	    ::getpeername(session->GetSocket(), reinterpret_cast<SOCKADDR*>(&remote), &length) != 0)
	{
		RetryAccept(event);
		return;
	}

	if (_service->AddSession(session))
	{
		session->AcceptConnected(NetAddress(remote));
	}
	else
	{
		session->Disconnect(WSAENOBUFS);
	}

	RegisterAccept(event);
}

void Listener::RetryAccept(AcceptEvent* event)
{
	jobs->DoTimer(100, [weak = std::weak_ptr(GetListenerPtr()), event]
	{
		if (auto self = weak.lock())
		{
			self->RegisterAccept(event);
		}
	});
}
