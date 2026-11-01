#pragma once
#include "NetAddress.h"
#include "IocpCore.h"

class CoreRuntime;
enum class ServiceType
{
	SERVER,
	CLIENT
};

using SessionFactory = std::function<SessionPtr()>;

class Service : public std::enable_shared_from_this<Service>
{
public:
	Service(ServiceType _type, NetAddress address, CoreRuntime& _runtime,
	        SessionFactory factory, std::size_t _maxSessionCount);
	virtual ~Service() = default;
	[[nodiscard]] virtual Result Start() = 0;
	virtual void CloseService();
	void Broadcast(const SendBufferPtr& buffer) const;
	SessionPtr CreateSession();
	bool AddSession(SessionPtr session);
	void ReleaseSession(const SessionPtr& session);
	std::size_t GetCurrentSessionCount() const;
	std::size_t GetMaxSessionCount() const
	{
		return maxSessionCount;
	}

	ServiceType GetServiceType() const
	{
		return type;
	}

	const NetAddress& GetNetAddress() const
	{
		return netAddress;
	}

	IocpCorePtr GetIocpCore() const;
	CoreRuntime& GetRuntime() const
	{
		return runtime;
	}

	bool IsClosing() const
	{
		return closing.load();
	}
protected:
	[[nodiscard]] Result BeginStart();
private:
	std::vector<SessionPtr> SnapshotSessions() const;
	ServiceType type;
	NetAddress netAddress;
	CoreRuntime& runtime;
	SessionFactory sessionFactory;
	std::size_t maxSessionCount;
	std::atomic<bool> closing{false};
	bool started{false};
	mutable std::mutex lock;
	std::unordered_set<SessionPtr> sessions;
};
