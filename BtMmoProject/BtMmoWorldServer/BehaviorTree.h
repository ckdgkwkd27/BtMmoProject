#pragma once

enum class BehaviorStatus
{
	FAILURE,
	SUCCESS,
	RUNNING
};

template <typename Context>
class BehaviorNode
{
public:
	using Action = std::function<BehaviorStatus(Context&)>;
	static BehaviorNode Leaf(Action action) { return BehaviorNode(std::move(action)); }

	static BehaviorNode Selector(std::vector<BehaviorNode> children) { return BehaviorNode(false, std::move(children)); }

	static BehaviorNode Sequence(std::vector<BehaviorNode> children) { return BehaviorNode(true, std::move(children)); }
	
	BehaviorStatus Tick(Context& context) const
	{
		if (action)
		{
			return action(context);
		}
		
		for (const auto& child : children)
		{
			const auto status = child.Tick(context);
			if (status == BehaviorStatus::RUNNING)
			{
				return status;
			}

			if (sequence && status == BehaviorStatus::FAILURE)
			{
				return status;
			}

			if (!sequence && status == BehaviorStatus::SUCCESS)
			{
				return status;
			}
		}

		return sequence ? BehaviorStatus::SUCCESS : BehaviorStatus::FAILURE;
	}
	
private:
	explicit BehaviorNode(Action _action) : action(std::move(_action)) {}

	BehaviorNode(bool _sequence, std::vector<BehaviorNode> _children) : sequence(_sequence), children(std::move(_children)) {}

	Action action;
	bool sequence{};
	std::vector<BehaviorNode> children;
};
