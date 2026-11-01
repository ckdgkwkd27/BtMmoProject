#pragma once

class TimerHandle
{
public:
	TimerHandle() = default;
	explicit TimerHandle(std::shared_ptr<std::atomic<bool>> _cancelled) : cancelled(std::move(_cancelled))
	{
	}

	void Cancel() const
	{
		if (cancelled)
		{
			cancelled->store(true);
		}
	}
private:
	std::shared_ptr<std::atomic<bool>> cancelled;
};
