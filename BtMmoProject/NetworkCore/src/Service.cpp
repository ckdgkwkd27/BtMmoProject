#include <Pch.h>
#include <NetworkCore/Service.h>
#include <NetworkCore/Session.h>
#include <NetworkCore/CoreRuntime.h>

Service::Service(const ServiceType _type, const NetAddress address, CoreRuntime& _runtime,
                 SessionFactory factory, const std::size_t _maxSessionCount)
    : type(_type), netAddress(address), runtime(_runtime),
      sessionFactory(std::move(factory)), maxSessionCount(_maxSessionCount)
{
}

Result Service::BeginStart()
{
	if (started || IsClosing())
	{
		return std::unexpected("Service can start only once");
	}

	if (!runtime.IsValid())
	{
		return std::unexpected("Initializing WinSock/IOCP failed");
	}

	if (!netAddress.IsValid())
	{
		return std::unexpected("Expected a numeric IPv4 address");
	}

	if (!sessionFactory || maxSessionCount == 0)
	{
		return std::unexpected("Invalid service options");
	}

	started = true;
	return {};
}

IocpCorePtr Service::GetIocpCore() const
{
	return runtime.GetIocpCore();
}

SessionPtr Service::CreateSession()
{
	if (!sessionFactory || !runtime.IsValid())
	{
		return {};
	}

	SessionPtr session = sessionFactory();
	if (!session || session->GetSocket() == INVALID_SOCKET)
	{
		return {};
	}

	session->SetService(shared_from_this());
	if (!GetIocpCore()->Register(session))
	{
		return {};
	}

	return session;
}

bool Service::AddSession(SessionPtr session)
{
	std::scoped_lock _lock(lock);
	if (!session || IsClosing() || sessions.size() >= maxSessionCount)
	{
		return false;
	}

	return sessions.insert(std::move(session)).second;
}

void Service::ReleaseSession(const SessionPtr& session)
{
	std::scoped_lock _lock(lock);
	sessions.erase(session);
}

std::size_t Service::GetCurrentSessionCount() const
{
	std::scoped_lock _lock(lock);
	return sessions.size();
}

std::vector<SessionPtr> Service::SnapshotSessions() const
{
	std::scoped_lock _lock(lock);
	return {sessions.begin(), sessions.end()};
}

void Service::CloseService()
{
	if (closing.exchange(true))
	{
		return;
	}

	for (const auto& session : SnapshotSessions())
	{
		session->Disconnect();
	}
}

void Service::Broadcast(const SendBufferPtr& buffer) const
{
	for (const auto& session : SnapshotSessions())
	{
		session->Send(buffer);
	}
}
