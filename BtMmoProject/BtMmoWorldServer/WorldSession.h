#pragma once
#include <NetworkCore/PacketSession.h>
#include <WorldProtocol.h>

class GameWorld;
class WorldMetrics;
class WorldSession final : public PacketSession
{
public:
	WorldSession(CoreRuntime& runtime, std::shared_ptr<GameWorld> _world,
	             uint64 _id, SessionOptions options, std::shared_ptr<WorldMetrics> metrics = nullptr);
	bool ReplyPing();
	void UpdatePlayerPosition(RequestPlayerPosition position) const;
	void UpdatePlayerMove(RequestPlayerMove movement) const;
	void UpdatePlayerState(RequestPlayerState state) const;
	void ReportMeleeHit(RequestMeleeHit hit) const;
protected:
	void OnConnected() override;
	void OnSend(uint32 bytes) override;
	bool OnRecvPacket(uint16 _id, std::span<const BYTE> payload) override;
	void OnDisconnected(DWORD error) override;
private:
	std::shared_ptr<GameWorld> world;
	std::shared_ptr<WorldMetrics> metrics;
	uint64 id;
	uint64 rateWindow{0};
	unsigned packetsInWindow{0};
};
