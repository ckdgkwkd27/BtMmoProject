#pragma once
#include "BotOptions.h"
#include "BotStatistics.h"
#include "BotSession.h"
#include <NetworkCore/JobQueue.h>

class BotScenario final : public JobQueue, public IBotObserver
{
public:
	BotScenario(CoreRuntime& runtime, BotOptions _options);
	void AddBot(std::shared_ptr<BotSession> bot);
	std::future<bool> GetCompletion();
	void Start();
	void RequestStop(std::string _reason);
	void OnWelcome(unsigned index, ReturnWelcome info) override;
	void OnSpawner(unsigned index, NotifySpawner info) override;
	void OnError(std::string _reason) override;
	void Report() const;
private:
	std::shared_ptr<BotScenario> GetScenarioPtr();
	void ScheduleHeartbeat();
	void Stop(bool _success, std::string _reason);
	BotOptions options;
	BotStatistics statistics;
	std::vector<std::shared_ptr<BotSession>> bots;
	std::promise<bool> completion;
	TimerHandle deadline, heartbeat;
	bool stopping{false}, success{false};
	std::string reason;
};
