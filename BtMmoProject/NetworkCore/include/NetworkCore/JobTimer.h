#pragma once
#include "Types.h"
#include "TimerHandle.h"

class JobTimer
{
public:
	TimerHandle Reserve(uint64 tickAfter, std::weak_ptr<JobQueue> owner, JobPtr job);
	void Distribute(uint64 now);
	void Stop();
private:
	struct TimerItem
	{
		bool operator<(const TimerItem& other) const
		{
			return executeTick > other.executeTick;
		}

		uint64 executeTick;
		std::weak_ptr<JobQueue> owner;
		JobPtr job;
		std::shared_ptr<std::atomic<bool>> cancelled;
	};

	std::mutex lock;
	std::priority_queue<TimerItem> items;
	bool stopped{false};
};
