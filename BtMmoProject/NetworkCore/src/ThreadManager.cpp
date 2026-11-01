#include <Pch.h>
#include <NetworkCore/ThreadManager.h>
#include <NetworkCore/GlobalQueue.h>
#include <NetworkCore/JobQueue.h>
#include <NetworkCore/JobTimer.h>

ThreadManager::~ThreadManager()
{
	Join();
}

void ThreadManager::Launch(std::function<void()> callback)
{
	threads.emplace_back(std::move(callback));
}

void ThreadManager::Join()
{
	threads.clear();
}

void ThreadManager::DoGlobalQueueWork(GlobalQueue& global)
{
	const uint64 endTick = ::GetTickCount64() + 5;
	do
	{
		auto queue = global.Pop();
		if (!queue)
		{
			break;
		}

		queue->Execute();
	}
	while (::GetTickCount64() < endTick);
}

void ThreadManager::DistributeReservedJobs(JobTimer& timer)
{
	timer.Distribute(::GetTickCount64());
}
