#pragma once
#include "IocpCore.h"
#include "IocpEvent.h"
#include "NetAddress.h"
#include "RecvBuffer.h"
#include "SendBufferQueue.h"
#include "JobQueue.h"
#include <boost/thread/future.hpp>

class CoreRuntime;

struct SessionOptions
{
	std::size_t maxQueuedBytes = 1024 * 1024;
	uint64 idleTimeoutMs = 60000;
	uint64 connectTimeoutMs = 5000;
};

class Session : public IocpObject
{
	friend class Listener;
	friend class Service;
public:
	explicit Session(CoreRuntime& runtime, SessionOptions _options = {});
	~Session() override;
	boost::future<DWORD> Connect();
	bool Send(SendBufferPtr buffer);
	void Disconnect(DWORD reason = ERROR_OPERATION_ABORTED);
	bool IsConnected() const
	{
		return isConnected.load();
	}

	SessionPtr GetSessionPtr();
	std::shared_ptr<Service> GetService() const
	{
		return service.lock();
	}

	const NetAddress& GetAddress() const
	{
		return netAddress;
	}

	SOCKET GetSocket() const
	{
		return socket;
	}

	HANDLE GetHandle() const override;
	void Dispatch(IocpEvent* event, uint32 bytes, DWORD error) override;
protected:
	virtual void OnConnected()
	{
	}

	virtual int32 OnRecv(BYTE* buffer, int32 length) = 0;
	virtual void OnSend(uint32)
	{
	}

	virtual void OnDisconnected(DWORD)
	{
	}
private:
	enum class State
	{
		CREATED,
		CONNECTING,
		CONNECTED,
		DISCONNECTING,
		CLOSED
	};

	void SetService(std::shared_ptr<Service> _service)
	{
		service = _service;
	}

	void AcceptConnected(NetAddress address);
	void DispatchCompletion(IocpEvent* event, uint32 bytes, DWORD error);
	void BeginOperation(IocpEvent& event);
	void AbandonOperation(IocpEvent& event);
	void RegisterConnect();
	void RegisterDisconnect();
	void RegisterRecv();
	void RegisterSend();
	void ProcessConnect(DWORD error);
	void ProcessDisconnect();
	void ProcessRecv(uint32 bytes, DWORD error);
	void ProcessSend(uint32 bytes, DWORD error);
	void BeginDisconnect(DWORD error);
	void CompleteConnect(DWORD error);
	void ScheduleIdleCheck(uint64 delay);
	void CheckIdle();

	IocpCorePtr iocpCore;
	JobQueuePtr jobs;
	std::weak_ptr<Service> service;
	SOCKET socket{INVALID_SOCKET};
	NetAddress netAddress;
	SessionOptions options;
	State state{State::CREATED};
	std::atomic<bool> isConnected{false};
	std::atomic<bool> disconnectRequested{false};
	std::atomic<std::size_t> queuedBytes{0};
	std::size_t pendingIo{0};
	DWORD disconnectReason{ERROR_OPERATION_ABORTED};
	bool sendRegistered{false};
	uint64 lastActivity{0};
	RecvBuffer recvBuffer;
	SendBufferQueue sendQueue;
	ConnectEvent connectEvent;
	DisconnectEvent disconnectEvent;
	RecvEvent recvEvent;
	SendEvent sendEvent;
	TimerHandle deadline;
	std::shared_ptr<boost::promise<DWORD>> connectPromise;
};
