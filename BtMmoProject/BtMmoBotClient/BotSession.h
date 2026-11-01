#pragma once
#include <NetworkCore/PacketSession.h>
#include <WorldProtocol.h>

class IBotObserver
{
public:
	virtual ~IBotObserver() = default;
	virtual void OnWelcome(unsigned index, ReturnWelcome info) = 0;
	virtual void OnSpawner(unsigned index, NotifySpawner info) = 0;
	virtual void OnError(std::string reason) = 0;
};

class BotSession final : public PacketSession
{
public:
	BotSession(CoreRuntime& runtime, std::weak_ptr<IBotObserver> _observer, unsigned _index);
	void AttachConnection(boost::future<DWORD> connection);
	bool OnWelcome(ReturnWelcome info);
	bool OnSpawner(NotifySpawner info);
	bool IsWelcomed() const { return snapshot.IsWelcomed(); }

	bool IsSnapshotComplete() const { return snapshot.IsComplete(); }
protected:
	void OnConnected() override;
	bool OnRecvPacket(uint16 id, std::span<const BYTE> payload) override;
	void OnDisconnected(DWORD error) override;
private:
	std::weak_ptr<IBotObserver> observer;
	unsigned index;
	bool wasConnected{false};
	WorldSnapshotState snapshot;
	boost::future<void> continuation;
};
