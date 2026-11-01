#include <Pch.h>
#include <NetworkCore/CoreRuntime.h>
#include <NetworkCore/JobQueue.h>
#include <NetworkCore/SocketUtils.h>

SocketRuntime::SocketRuntime() : initialized(SocketUtils::Init())
{
}

SocketRuntime::~SocketRuntime()
{
	if (initialized)
	{
		SocketUtils::Clear();
	}
}

CoreRuntime::CoreRuntime() : core(std::make_shared<IocpCore>()), global(*core)
{
}

CoreRuntime::~CoreRuntime()
{
	Shutdown();
}

Result CoreRuntime::Start(unsigned _threads)
{
	if (!IsValid())
	{
		return std::unexpected("Initializing WinSock/IOCP failed");
	}

	if (_threads == 0 || _threads > 256 || threadCount != 0 || stop)
	{
		return std::unexpected("CoreRuntime requires 1..256 workers and can start only once");
	}

	for (unsigned i = 0; i < _threads; ++i)
	{
		threads.Launch([this]
		{
			WorkerLoop();
		});

		++threadCount;
	}

	return {};
}

void CoreRuntime::WorkerLoop()
{
	while (!stop.load())
	{
		core->Dispatch(10);
		if (const auto error = core->GetError(); error != ERROR_SUCCESS)
		{
			global.RecordFailure("IOCP dispatch failed: " + std::to_string(error));
		}

		ThreadManager::DistributeReservedJobs(timer);
		ThreadManager::DoGlobalQueueWork(global);
	}
}

void CoreRuntime::Shutdown()
{
	if (stop.load())
	{
		return;
	}

	timer.Stop();
	while (core->PendingWork() != 0)
	{
		if (threadCount == 0)
		{
			core->Dispatch(10);
			ThreadManager::DoGlobalQueueWork(global);
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	stop = true;
	for (unsigned i = 0; i < threadCount; ++i)
	{
		core->Wake();
	}

	threads.Join();
}

JobQueuePtr CoreRuntime::CreateJobQueue()
{
	return std::make_shared<JobQueue>(global, timer);
}
