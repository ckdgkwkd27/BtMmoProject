#include <Pch.h>
#include "BotScenario.h"
#include <NetworkCore/CoreRuntime.h>
#include <WorldPacketFactory.h>

BotScenario::BotScenario(CoreRuntime& runtime, BotOptions _options)
    : JobQueue(runtime.GetGlobalQueue(), runtime.GetJobTimer()),
      options(std::move(_options)), statistics(options.clients)
{
}

std::shared_ptr<BotScenario> BotScenario::GetScenarioPtr()
{
	return std::static_pointer_cast<BotScenario>(shared_from_this());
}

void BotScenario::AddBot(std::shared_ptr<BotSession> bot)
{
	bots.push_back(std::move(bot));
}

std::future<bool> BotScenario::GetCompletion()
{
	return completion.get_future();
}

void BotScenario::Start()
{
	DoAsync([self = GetScenarioPtr()]
	{
		if (self->stopping)
		{
			return;
		}

		self->deadline = self->DoTimer(self->options.timeoutSeconds * 1000ULL, [weak = std::weak_ptr(self)]
		{
			if (auto scenario = weak.lock())
			{
				scenario->Stop(false, "Timed out waiting for spawner snapshots");
			}
		});

		self->ScheduleHeartbeat();
	});
}

void BotScenario::OnWelcome(unsigned index, ReturnWelcome info)
{
	DoAsync([self = GetScenarioPtr(), index, info]
	{
		if (self->stopping)
		{
			return;
		}

		if (!self->statistics.RecordWelcome(index, info))
		{
			self->Stop(false, "Duplicate or invalid welcome");
			return;
		}

		if (self->options.verbose)
		{
			std::cout << "[CONNECTED][client " << index + 1 << "] session=" << info.sessionId
			          << " spawners=" << info.spawnerCount << '\n';
		}

		if (self->statistics.AllReceived())
		{
			self->Stop(true, "All spawner snapshots received");
		}
	});
}

void BotScenario::OnSpawner(unsigned index, NotifySpawner info)
{
	DoAsync([self = GetScenarioPtr(), index, info]
	{
		if (self->stopping)
		{
			return;
		}

		if (!self->statistics.RecordSpawner(index, info))
		{
			self->Stop(false, "Unexpected or duplicate spawner");
			return;
		}

		if (self->options.verbose)
		{
			std::cout << "[SPAWNER][client " << index + 1 << "] id=" << info.id << " center=("
			          << info.x << ',' << info.y << ',' << info.z << ") alive=" << info.alive
			          << '/' << info.maxAlive << " pending=" << info.pending << '\n';
		}

		if (self->statistics.AllReceived())
		{
			self->Stop(true, "All spawner snapshots received");
		}
	});
}

void BotScenario::OnError(std::string _reason)
{
	RequestStop(std::move(_reason));
}

void BotScenario::RequestStop(std::string _reason)
{
	DoAsync([self = GetScenarioPtr(), _reason = std::move(_reason)]
	{
		self->Stop(false, _reason);
	});
}

void BotScenario::ScheduleHeartbeat()
{
	heartbeat = DoTimer(10000, [weak = std::weak_ptr(GetScenarioPtr())]
	{
		if (auto self = weak.lock(); self && !self->stopping)
		{
			for (unsigned i = 0; i < self->bots.size(); ++i)
			{
				if (self->statistics.IsReady(i))
				{
					self->bots[i]->Send(WorldPacketFactory::MakeSendBuffer(RequestPing{}));
				}
			}

			self->ScheduleHeartbeat();
		}
	});
}

void BotScenario::Stop(bool _success, std::string _reason)
{
	if (stopping)
	{
		return;
	}

	stopping = true;
	success = _success;
	reason = std::move(_reason);
	deadline.Cancel();
	heartbeat.Cancel();
	for (const auto& bot : bots)
	{
		bot->Disconnect();
	}

	completion.set_value(_success);
}

void BotScenario::Report() const
{
	statistics.Report(std::cout, success, reason);
}
