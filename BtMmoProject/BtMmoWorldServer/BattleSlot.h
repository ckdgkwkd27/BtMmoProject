#pragma once
#include "NavigationMesh.h"
#include <Types.h>

// One reservation table per target. Called only on GameWorld's serialized job queue.
class BattleSlot final
{
public:
	struct Key final
	{
		int32 slotIndex{}, ringIndex{};
		bool operator==(const Key&) const = default;
		struct Hash
		{
			std::size_t operator()(const Key& key) const;
		};
	};

	struct SlotInfo final
	{
		Key slotKey;
		NavPoint slotBasePos, targetPos;
		std::vector<NavPoint> path;
	};

	std::optional<SlotInfo> GetBestSlot(const NavigationMesh& geo, NavPoint targetPos, NavPoint startPos,
	                                    float radius, float distance, uint64 objectId) const;
	bool AcquireSlot(Key slotKey, uint64 objectId);
	void ReleaseSlot(uint64 objectId);
	std::optional<Key> FindSlot(uint64 objectId) const;
	std::size_t GetOccupiedCount() const { return slots.size(); }

	static NavPoint CalcSlotPos(NavPoint basePos, Key slotKey);
	static int32 CalcRingSlotCount(int32 ringIndex);
	static constexpr float RING_WIDTH = 120;
	static constexpr int32 MAX_RINGS = 4;
private:
	std::unordered_map<Key, uint64, Key::Hash> slots;
	std::unordered_map<uint64, Key> objectSlotIndex;
};
