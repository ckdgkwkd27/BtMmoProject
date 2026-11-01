#pragma once
#include "Types.h"
#include "TimerHandle.h"

class GlobalQueue;
class JobTimer;

using CallbackType = std::function<void()>;

class Job
{
public:
	explicit Job(CallbackType _callback) : callback(std::move(_callback))
	{
	}

	void Execute()
	{
		callback();
	}
private:
	CallbackType callback;
};

class JobQueue : public std::enable_shared_from_this<JobQueue>
{
public:
	JobQueue(GlobalQueue& _global, JobTimer& _timer) : global(_global), timer(_timer)
	{
	}

	virtual ~JobQueue() = default;
	void DoAsync(CallbackType callback);
	TimerHandle DoTimer(uint64 tickAfter, CallbackType callback);
	void Push(JobPtr job);
	void Execute();
private:
	GlobalQueue& global;
	JobTimer& timer;
	std::mutex lock;
	std::queue<JobPtr> jobs;
	bool scheduled{false};
};
