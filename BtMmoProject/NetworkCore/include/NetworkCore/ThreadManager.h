#pragma once

class GlobalQueue;
class JobTimer;
class ThreadManager
{
public:
	~ThreadManager();
	void Launch(std::function<void()> callback);
	void Join();
	static void DoGlobalQueueWork(GlobalQueue& global);
	static void DistributeReservedJobs(JobTimer& timer);
private:
	std::vector<std::jthread> threads;
};
