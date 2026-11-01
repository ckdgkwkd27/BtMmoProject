#pragma once
#include "IocpCore.h"
#include "GlobalQueue.h"
#include "JobTimer.h"
#include "ThreadManager.h"

class SocketRuntime
{
public:
	SocketRuntime();
	~SocketRuntime();
	SocketRuntime(const SocketRuntime&) = delete;
	SocketRuntime& operator=(const SocketRuntime&) = delete;
	bool IsValid() const
	{
		return initialized;
	}
private:
	bool initialized{false};
};

class CoreRuntime
{
public:
	CoreRuntime();
	~CoreRuntime();
	[[nodiscard]] Result Start(unsigned _threads);
	void Shutdown();
	JobQueuePtr CreateJobQueue();
	IocpCorePtr GetIocpCore() const
	{
		return core;
	}

	GlobalQueue& GetGlobalQueue()
	{
		return global;
	}

	JobTimer& GetJobTimer()
	{
		return timer;
	}

	bool HasFailure() const
	{
		return global.HasFailure();
	}

	std::string GetFailure() const
	{
		return global.GetFailure();
	}

	bool IsValid() const
	{
		return sockets.IsValid() && core->IsValid();
	}
private:
	void WorkerLoop();
	SocketRuntime sockets;
	IocpCorePtr core;
	GlobalQueue global;
	JobTimer timer;
	ThreadManager threads;
	std::atomic<bool> stop{false};
	unsigned threadCount{0};
};
