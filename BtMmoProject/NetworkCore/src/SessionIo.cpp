#include <Pch.h>
#include <NetworkCore/Session.h>
#include <NetworkCore/Service.h>
#include <NetworkCore/SocketUtils.h>

void Session::Dispatch(IocpEvent* event, uint32 bytes, DWORD error)
{
	jobs->DoAsync([self = GetSessionPtr(), event, bytes, error]
	{
		self->DispatchCompletion(event, bytes, error);
	});
}

void Session::DispatchCompletion(IocpEvent* event, uint32 bytes, DWORD error)
{
	--pendingIo;
	switch (event->eventType)
	{
	case EventType::CONNECT:
		ProcessConnect(error);
		break;
	case EventType::DISCONNECT:
		SocketUtils::Close(socket);
		break;
	case EventType::RECV:
		ProcessRecv(bytes, error);
		break;
	case EventType::SEND:
		ProcessSend(bytes, error);
		break;
	default:
		BeginDisconnect(ERROR_INVALID_DATA);
		break;
	}

	ProcessDisconnect();
}

void Session::BeginOperation(IocpEvent& event)
{
	event.Init();
	event.owner = shared_from_this();
	++pendingIo;
	iocpCore->OperationStarted();
}

void Session::AbandonOperation(IocpEvent& event)
{
	event.owner.reset();
	--pendingIo;
	iocpCore->OperationCompleted();
}

void Session::RegisterConnect()
{
	if (!SocketUtils::BindAnyAddress(socket, 0))
	{
		BeginDisconnect(::WSAGetLastError());
		return;
	}

	BeginOperation(connectEvent);
	DWORD bytes = 0;
	const auto& address = netAddress.GetSockAddr();
	if (!SocketUtils::ConnectEx(socket, reinterpret_cast<const SOCKADDR*>(&address), sizeof(address),
	                            nullptr, 0, &bytes, &connectEvent))
	{
		const DWORD error = ::WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			AbandonOperation(connectEvent);
			BeginDisconnect(error);
		}
	}
}

void Session::RegisterDisconnect()
{
	BeginOperation(disconnectEvent);
	if (!SocketUtils::DisconnectEx(socket, &disconnectEvent, 0, 0))
	{
		const DWORD error = ::WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			AbandonOperation(disconnectEvent);
			SocketUtils::Close(socket);
		}
	}
}

void Session::ProcessConnect(DWORD error)
{
	if (state == State::DISCONNECTING || state == State::CLOSED)
	{
		return;
	}

	if (error || disconnectRequested)
	{
		BeginDisconnect(error ? error : ERROR_OPERATION_ABORTED);
		return;
	}

	if ((state == State::CONNECTING && !SocketUtils::SetUpdateConnectSocket(socket)) ||
	    !SocketUtils::SetTcpNoDelay(socket, true))
	{
		BeginDisconnect(::WSAGetLastError());
		return;
	}

	auto _service = GetService();
	if (!_service || _service->IsClosing())
	{
		BeginDisconnect(ERROR_OPERATION_ABORTED);
		return;
	}

	state = State::CONNECTED;
	isConnected = true;
	deadline.Cancel();
	lastActivity = ::GetTickCount64();
	ScheduleIdleCheck(options.idleTimeoutMs);
	OnConnected();
	CompleteConnect(ERROR_SUCCESS);
	RegisterRecv();
}

void Session::RegisterRecv()
{
	if (!IsConnected())
	{
		return;
	}

	if (recvBuffer.FreeSize() == 0)
	{
		BeginDisconnect(WSAEMSGSIZE);
		return;
	}

	WSABUF buffer{};
	buffer.buf = reinterpret_cast<char*>(recvBuffer.WritePos());
	buffer.len = static_cast<ULONG>(recvBuffer.FreeSize());
	DWORD bytes = 0;
	DWORD flags = 0;
	BeginOperation(recvEvent);
	if (::WSARecv(socket, &buffer, 1, &bytes, &flags, &recvEvent, nullptr) == SOCKET_ERROR)
	{
		const DWORD error = ::WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			AbandonOperation(recvEvent);
			BeginDisconnect(error);
		}
	}
}

void Session::ProcessRecv(uint32 bytes, DWORD error)
{
	if (state != State::CONNECTED)
	{
		return;
	}

	if (error || bytes == 0)
	{
		BeginDisconnect(error ? error : WSAECONNRESET);
		return;
	}

	if (!recvBuffer.OnWrite(static_cast<int32>(bytes)))
	{
		BeginDisconnect(WSAEMSGSIZE);
		return;
	}

	const int32 processed = OnRecv(recvBuffer.ReadPos(), recvBuffer.DataSize());
	if (!recvBuffer.OnRead(processed))
	{
		BeginDisconnect(ERROR_INVALID_DATA);
		return;
	}

	if (processed > 0)
	{
		lastActivity = ::GetTickCount64();
	}

	recvBuffer.Clean();
	RegisterRecv();
}

void Session::RegisterSend()
{
	if (!IsConnected())
	{
		return;
	}

	sendQueue.BeginBatch();
	if (!sendQueue.HasActive())
	{
		return;
	}

	auto buffers = sendQueue.GetBuffers();
	DWORD bytes = 0;
	sendRegistered = true;
	BeginOperation(sendEvent);
	if (::WSASend(socket, buffers.data(), static_cast<DWORD>(buffers.size()), &bytes, 0, &sendEvent, nullptr) == SOCKET_ERROR)
	{
		const DWORD error = ::WSAGetLastError();
		if (error != WSA_IO_PENDING)
		{
			AbandonOperation(sendEvent);
			sendRegistered = false;
			queuedBytes.fetch_sub(sendQueue.ClearActive());
			BeginDisconnect(error);
		}
	}
}

void Session::ProcessSend(uint32 bytes, DWORD error)
{
	sendRegistered = false;
	if (state != State::CONNECTED || error || bytes == 0)
	{
		queuedBytes.fetch_sub(sendQueue.ClearActive());
		if (state == State::CONNECTED)
		{
			BeginDisconnect(error ? error : WSAECONNRESET);
		}

		return;
	}

	if (!sendQueue.Consume(bytes))
	{
		BeginDisconnect(ERROR_INVALID_DATA);
		return;
	}

	queuedBytes.fetch_sub(bytes);
	OnSend(bytes);
	RegisterSend();
}
