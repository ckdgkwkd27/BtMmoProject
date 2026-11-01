#include <Pch.h>
#include <NetworkCore/JobTimer.h>
#include <NetworkCore/JobQueue.h>

TimerHandle JobTimer::Reserve(uint64 tickAfter, std::weak_ptr<JobQueue> owner, JobPtr job)
{
	auto cancelled = std::make_shared<std::atomic<bool>>(false);
	std::scoped_lock _lock(lock);
	if (stopped)
	{
		cancelled->store(true);
	}
	else
	{
		items.push(TimerItem{::GetTickCount64() + tickAfter, std::move(owner), std::move(job), cancelled});
	}

	return TimerHandle(cancelled);
}

void JobTimer::Distribute(uint64 now)
{
	std::scoped_lock _lock(lock);
	while (!items.empty() && items.top().executeTick <= now)
	{
		auto item = items.top();
		items.pop();
		if (item.cancelled->load())
		{
			continue;
		}

		if (auto owner = item.owner.lock())
		{
			owner->DoAsync([job = std::move(item.job), cancelled = std::move(item.cancelled)]
			{
				if (!cancelled->load())
				{
					job->Execute();
				}
			});
		}
	}
}

void JobTimer::Stop()
{
	std::scoped_lock _lock(lock);
	stopped = true;
	while (!items.empty())
	{
		items.top().cancelled->store(true);
		items.pop();
	}
}
