#pragma once
#include <Types.h>
#include <WorldProtocol.h>

class BotStatistics
{
public:
	explicit BotStatistics(unsigned clients);
	bool RecordWelcome(unsigned index, ReturnWelcome info);
	bool RecordSpawner(unsigned index, NotifySpawner info);
	bool AllReceived() const;
	bool IsReady(unsigned index) const { return snapshots.at(index).IsWelcomed(); }

	void Report(std::ostream& output, bool success, const std::string& reason) const;
private:
	std::vector<WorldSnapshotState> snapshots;
	std::unordered_set<uint64> sessionIds;
	unsigned ready{0};
	uint64 received{0}, expected{0};
};
