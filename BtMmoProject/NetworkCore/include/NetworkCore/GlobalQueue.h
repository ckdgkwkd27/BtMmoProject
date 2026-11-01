#pragma once
#include "Types.h"
#include "LockQueue.h"

class GlobalQueue
{
public:
	explicit GlobalQueue(IocpCore& _core) : core(_core)
	{
	}

	void Push(JobQueuePtr queue);
	JobQueuePtr Pop();
	void JobAdded();
	void JobCompleted();
	std::size_t PendingJobs() const
	{
		return pending.load();
	}

	void RecordFailure(std::string _failure);
	bool HasFailure() const;
	std::string GetFailure() const;
private:
	IocpCore& core;
	LockQueue<JobQueuePtr> jobQueues;
	std::atomic<std::size_t> pending{0};
	mutable std::mutex failureLock;
	std::string failure;
};
