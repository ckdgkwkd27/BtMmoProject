#include <Pch.h>
#include "BattleSlot.h"

static constexpr float TWO_PI = 6.28318530718f;

std::size_t BattleSlot::Key::Hash::operator()(const Key& key) const
{
	return std::hash<uint64>{}((static_cast<uint64>(static_cast<uint32>(key.slotIndex)) << 32) |
	                           static_cast<uint32>(key.ringIndex));
}

int32 BattleSlot::CalcRingSlotCount(int32 ringIndex)
{
	return 5 * (2 * ringIndex + 1);
}

NavPoint BattleSlot::CalcSlotPos(NavPoint basePos, Key slotKey)
{
	const float angle = TWO_PI * slotKey.slotIndex / CalcRingSlotCount(slotKey.ringIndex);
	const float distance = (slotKey.ringIndex + 1) * RING_WIDTH;
	return {basePos.x + std::cos(angle) * distance, basePos.y + std::sin(angle) * distance, basePos.z};
}

std::optional<BattleSlot::Key> BattleSlot::FindSlot(uint64 objectId) const
{
	const auto found = objectSlotIndex.find(objectId);
	return found != objectSlotIndex.end() ? std::optional(found->second) : std::nullopt;
}

std::optional<BattleSlot::SlotInfo> BattleSlot::GetBestSlot(const NavigationMesh& geo, NavPoint targetPos,
	NavPoint startPos, float radius, float distance, uint64 objectId) const
{
	if (objectId == 0 || !std::isfinite(radius) || radius <= 0 || !std::isfinite(distance) || distance < RING_WIDTH)
	{
		return std::nullopt;
	}

	const auto base = geo.ProjectPoint(targetPos);
	if (!base || std::hypot(base->x - targetPos.x, base->y - targetPos.y) > 20 || std::abs(base->z - targetPos.z) > 100)
	{
		return std::nullopt;
	}

	const auto owned = FindSlot(objectId);
	const float yaw = std::atan2(startPos.y - targetPos.y, startPos.x - targetPos.x);
	const int32 ringCount = static_cast<int32>(std::min(distance / RING_WIDTH, static_cast<float>(MAX_RINGS)));
	
	for (int32 ring = 0; ring < ringCount; ++ring)
	{
		const int32 count = CalcRingSlotCount(ring);
		const int32 preferred = owned && owned->ringIndex == ring ? owned->slotIndex : (static_cast<int32>(std::lround(yaw * count / TWO_PI)) + count) % count;
		for (int32 step = 0; step < count; ++step)
		{
			const int32 offset = step % 2 ? (step + 1) / 2 : -step / 2;
			const Key key{(preferred + offset + count) % count, ring};
			const auto occupant = slots.find(key);
			if (occupant != slots.end() && occupant->second != objectId)
			{
				continue;
			}

			const auto desired = CalcSlotPos(*base, key);
			const auto point = geo.ProjectPoint(desired);
			if (!point || std::hypot(point->x - desired.x, point->y - desired.y) > 20 ||
			    std::abs(point->z - base->z) > 100 || !geo.TraceSurface(*base, *point))
			{
				continue;
			}

			bool overlaps = false;
			for (const auto& [otherKey, otherId] : slots)
			{
				if (otherId == objectId)
				{
					continue;
				}

				const auto other = geo.ProjectPoint(CalcSlotPos(*base, otherKey));
				if (other && std::hypot(other->x - point->x, other->y - point->y) < 2 * radius + 8)
				{
					overlaps = true;
					break;
				}
			}

			if (overlaps)
			{
				continue;
			}

			auto path = geo.FindPath(startPos, *point);
			if (path)
			{
				return SlotInfo{key, targetPos, *point, std::move(*path)};
			}
		}
	}

	return std::nullopt;
}

bool BattleSlot::AcquireSlot(Key slotKey, uint64 objectId)
{
	if (objectId == 0 || slotKey.ringIndex < 0 || slotKey.ringIndex >= MAX_RINGS ||
	    slotKey.slotIndex < 0 || slotKey.slotIndex >= CalcRingSlotCount(slotKey.ringIndex))
	{
		return false;
	}

	const auto occupied = slots.find(slotKey);
	if (occupied != slots.end())
	{
		return occupied->second == objectId;
	}

	// Failed acquisition must retain the old reservation; successful transfers update both maps.
	ReleaseSlot(objectId);
	slots.emplace(slotKey, objectId);
	objectSlotIndex.insert_or_assign(objectId, slotKey);
	return true;
}

void BattleSlot::ReleaseSlot(uint64 objectId)
{
	const auto found = objectSlotIndex.find(objectId);
	if (found == objectSlotIndex.end())
	{
		return;
	}

	slots.erase(found->second);
	objectSlotIndex.erase(found);
}
