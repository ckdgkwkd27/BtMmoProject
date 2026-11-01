#include <Pch.h>
#include <NetworkCore/Session.h>
#include <NetworkCore/CoreRuntime.h>
#include <NetworkCore/Service.h>
#include <NetworkCore/SocketUtils.h>
#include <NetworkCore/Packet.h>

Session::Session(CoreRuntime& runtime, SessionOptions _options)
    : iocpCore(runtime.GetIocpCore()), jobs(runtime.CreateJobQueue()),
      options(_options), recvBuffer(static_cast<int32>(MAX_PACKET_SIZE))
{
	if (!runtime.IsValid() || _options.maxQueuedBytes < MAX_PACKET_SIZE || _options.idleTimeoutMs == 0 || _options.connectTimeoutMs == 0)
	{
		return;
	}

	socket = SocketUtils::CreateSocket();
}

Session::~Session()
{
	SocketUtils::Close(socket);
}

SessionPtr Session::GetSessionPtr()
{
	return std::static_pointer_cast<Session>(shared_from_this());
}

HANDLE Session::GetHandle() const
{
	return reinterpret_cast<HANDLE>(socket);
}

boost::future<DWORD> Session::Connect()
{
	auto promise = std::make_shared<boost::promise<DWORD>>();
	auto future = promise->get_future();
	jobs->DoAsync([self = GetSessionPtr(), promise]
	{
		auto _service = self->GetService();
		if (self->socket == INVALID_SOCKET || self->state != State::CREATED || !_service || _service->GetServiceType() != ServiceType::CLIENT)
		{
			promise->set_value(ERROR_INVALID_STATE);
			return;
		}

		self->connectPromise = promise;
		if (_service->IsClosing() || self->disconnectRequested)
		{
			self->BeginDisconnect(ERROR_OPERATION_ABORTED);
			return;
		}

		self->netAddress = _service->GetNetAddress();
		self->state = State::CONNECTING;
		self->deadline = self->jobs->DoTimer(self->options.connectTimeoutMs, [weak = std::weak_ptr(self)]
		{
			if (auto session = weak.lock())
			{
				session->BeginDisconnect(WSAETIMEDOUT);
			}
		});

		self->RegisterConnect();
	});

	return future;
}

void Session::AcceptConnected(NetAddress address)
{
	jobs->DoAsync([self = GetSessionPtr(), address]
	{
		self->netAddress = address;
		self->ProcessConnect(ERROR_SUCCESS);
	});
}

bool Session::Send(SendBufferPtr buffer)
{
	if (!buffer || buffer->WriteSize() <= 0 || !IsConnected())
	{
		return false;
	}

	const auto size = static_cast<std::size_t>(buffer->WriteSize());
	auto queued = queuedBytes.load();
	do
	{
		if (size > options.maxQueuedBytes - queued)
		{
			Disconnect(WSAENOBUFS);
			return false;
		}
	}
	while (!queuedBytes.compare_exchange_weak(queued, queued + size));
	jobs->DoAsync([self = GetSessionPtr(), buffer = std::move(buffer), size]
	{
		if (!self->IsConnected())
		{
			self->queuedBytes.fetch_sub(size);
			return;
		}

		self->sendQueue.Push(buffer);
		if (!self->sendRegistered)
		{
			self->RegisterSend();
		}
	});

	return true;
}

void Session::Disconnect(DWORD reason)
{
	isConnected = false;
	if (disconnectRequested.exchange(true))
	{
		return;
	}

	jobs->DoAsync([self = GetSessionPtr(), reason]
	{
		self->BeginDisconnect(reason);
	});
}

void Session::BeginDisconnect(DWORD error)
{
	if (state == State::DISCONNECTING || state == State::CLOSED)
	{
		return;
	}

	const bool wasConnected = state == State::CONNECTED;
	state = State::DISCONNECTING;
	disconnectRequested = true;
	isConnected = false;
	disconnectReason = error;
	deadline.Cancel();
	queuedBytes.fetch_sub(sendQueue.ClearPending());
	::CancelIoEx(GetHandle(), nullptr);
	if (wasConnected)
	{
		RegisterDisconnect();
	}
	else
	{
		SocketUtils::Close(socket);
	}

	ProcessDisconnect();
}

void Session::ProcessDisconnect()
{
	if (state != State::DISCONNECTING || pendingIo != 0)
	{
		return;
	}

	state = State::CLOSED;
	SocketUtils::Close(socket);
	queuedBytes.fetch_sub(sendQueue.ClearActive());
	CompleteConnect(disconnectReason);
	if (auto _service = GetService())
	{
		_service->ReleaseSession(GetSessionPtr());
	}

	OnDisconnected(disconnectReason);
}

void Session::CompleteConnect(DWORD error)
{
	if (auto promise = std::exchange(connectPromise, {}))
	{
		promise->set_value(error);
	}
}

void Session::ScheduleIdleCheck(uint64 delay)
{
	deadline = jobs->DoTimer(delay, [weak = std::weak_ptr(GetSessionPtr())]
	{
		if (auto session = weak.lock())
		{
			session->CheckIdle();
		}
	});
}

void Session::CheckIdle()
{
	if (state != State::CONNECTED)
	{
		return;
	}

	const uint64 elapsed = ::GetTickCount64() - lastActivity;
	if (elapsed >= options.idleTimeoutMs)
	{
		BeginDisconnect(WSAETIMEDOUT);
	}
	else
	{
		ScheduleIdleCheck(options.idleTimeoutMs - elapsed);
	}
}
