#include <Pch.h>
#include <NetworkCore/JobQueue.h>
#include <NetworkCore/GlobalQueue.h>
#include <NetworkCore/JobTimer.h>

void JobQueue::DoAsync(CallbackType callback)
{
	Push(std::make_shared<Job>(std::move(callback)));
}

TimerHandle JobQueue::DoTimer(uint64 tickAfter, CallbackType callback)
{
	return timer.Reserve(tickAfter, weak_from_this(), std::make_shared<Job>(std::move(callback)));
}

void JobQueue::Push(JobPtr job)
{
	bool schedule;
	{
		std::scoped_lock _lock(lock);
		jobs.push(std::move(job));
		global.JobAdded();
		schedule = !scheduled;
		scheduled = true;
	}

	if (schedule)
	{
		global.Push(shared_from_this());
	}
}

void JobQueue::Execute()
{
	std::vector<JobPtr> _jobs;
	{
		std::scoped_lock _lock(lock);
		while (!jobs.empty() && _jobs.size() < 64)
		{
			_jobs.push_back(std::move(jobs.front()));
			jobs.pop();
		}
	}

	for (const auto& job : _jobs)
	{
		job->Execute();
		global.JobCompleted();
	}

	bool reschedule;
	{
		std::scoped_lock _lock(lock);
		reschedule = !jobs.empty();
		scheduled = reschedule;
	}

	if (reschedule)
	{
		global.Push(shared_from_this());
	}
}
