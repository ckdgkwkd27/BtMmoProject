#include <Pch.h>
#include <NetworkCore/GlobalQueue.h>
#include <NetworkCore/IocpCore.h>

void GlobalQueue::Push(JobQueuePtr queue)
{
	jobQueues.Push(std::move(queue));
	core.Wake();
}

JobQueuePtr GlobalQueue::Pop()
{
	return jobQueues.Pop();
}

void GlobalQueue::JobAdded()
{
	core.WorkAdded();
	pending.fetch_add(1);
}

void GlobalQueue::JobCompleted()
{
	pending.fetch_sub(1);
	core.WorkCompleted();
}

void GlobalQueue::RecordFailure(std::string _failure)
{
	std::scoped_lock lock(failureLock);
	if (failure.empty())
	{
		failure = std::move(_failure);
	}
}

bool GlobalQueue::HasFailure() const
{
	std::scoped_lock lock(failureLock);
	return !failure.empty();
}

std::string GlobalQueue::GetFailure() const
{
	std::scoped_lock lock(failureLock);
	return failure;
}
