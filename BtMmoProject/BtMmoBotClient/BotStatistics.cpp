#include <Pch.h>
#include "BotStatistics.h"

BotStatistics::BotStatistics(unsigned clients) : snapshots(clients)
{
}

bool BotStatistics::RecordWelcome(unsigned index, ReturnWelcome info)
{
	if (index >= snapshots.size() || !snapshots[index].Accept(info) || !sessionIds.insert(info.sessionId).second)
	{
		return false;
	}

	++ready;
	expected += info.spawnerCount;
	return true;
}

bool BotStatistics::RecordSpawner(unsigned index, NotifySpawner info)
{
	if (index >= snapshots.size() || !snapshots[index].Accept(info))
	{
		return false;
	}

	++received;
	return true;
}

bool BotStatistics::AllReceived() const
{
	return ready == snapshots.size() && std::all_of(snapshots.begin(), snapshots.end(),
	                                                [](const auto& snapshot)
	{
		return snapshot.IsComplete();
	});
}

void BotStatistics::Report(std::ostream& output, bool success, const std::string& reason) const
{
	output << (success ? "PASS" : "FAIL") << " clients=" << ready << '/' << snapshots.size()
	       << " spawners=" << received << " expected=" << expected << " reason=" << reason << '\n';
}
