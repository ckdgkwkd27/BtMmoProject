#pragma once

template <typename T>
class LockQueue
{
public:
	void Push(T item)
	{
		std::scoped_lock _lock(lock);
		items.push(std::move(item));
	}

	T Pop()
	{
		std::scoped_lock _lock(lock);
		if (items.empty())
		{
			return T{};
		}

		T result = std::move(items.front());
		items.pop();
		return result;
	}
private:
	std::mutex lock;
	std::queue<T> items;
};
