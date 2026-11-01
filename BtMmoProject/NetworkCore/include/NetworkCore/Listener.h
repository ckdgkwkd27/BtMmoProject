#pragma once
#include "IocpCore.h"
#include "IocpEvent.h"
#include "NetAddress.h"
#include "JobQueue.h"

class CoreRuntime;
class Listener : public IocpObject
{
public:
	explicit Listener(CoreRuntime& runtime);
	~Listener() override;
	[[nodiscard]] Result StartAccept(const ServerServicePtr& _service);
	void CloseSocket();
	HANDLE GetHandle() const override;
	void Dispatch(IocpEvent* event, uint32 bytes, DWORD error) override;
	const NetAddress& GetAddress() const
	{
		return address;
	}
private:
	bool RegisterAccept(AcceptEvent* event);
	void ProcessAccept(AcceptEvent* event, DWORD error);
	void RetryAccept(AcceptEvent* event);
	std::shared_ptr<Listener> GetListenerPtr();
	SOCKET socket{INVALID_SOCKET};
	IocpCorePtr iocpCore;
	JobQueuePtr jobs;
	std::vector<std::unique_ptr<AcceptEvent>> acceptEvents;
	std::weak_ptr<ServerService> service;
	NetAddress address;
	std::atomic<bool> closing{false};
};
