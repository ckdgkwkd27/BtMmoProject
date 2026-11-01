#include <Pch.h>
#include "PlayerCellGrid.h"
#include <WorldProtocol.h>

static int32 CellCoordinate(double value)
{
	return static_cast<int32>(std::floor(value / PlayerCellGrid::CELL_SIZE));
}

static uint64 CellKey(int32 x, int32 y)
{
	return (static_cast<uint64>(static_cast<uint32>(x)) << 32) | static_cast<uint32>(y);
}

static uint64 CellKey(RequestPlayerPosition position)
{
	return CellKey(CellCoordinate(position.x), CellCoordinate(position.y));
}

static bool BlocksMovement(RequestPlayerPosition from, RequestPlayerPosition to, RequestPlayerPosition other)
{
	constexpr double radius = PlayerCellGrid::BODY_RADIUS * 2;
	constexpr double height = PlayerCellGrid::BODY_HEIGHT;
	const double x = from.x - other.x, y = from.y - other.y, z = from.z - other.z;
	const double dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
	const double distanceSquared = x * x + y * y;
	const double dot = x * dx + y * dy;
	const double travelSquared = dx * dx + dy * dy;

	if (distanceSquared < radius * radius && std::abs(z) < height)
	{
		return travelSquared > 0 ? dot < 0 : z * dz < 0;
	}

	double enter = 0, leave = 1;
	if (travelSquared == 0)
	{
		if (distanceSquared >= radius * radius)
		{
			return false;
		}
	}
	else
	{
		const double discriminant = dot * dot - travelSquared * (distanceSquared - radius * radius);
		if (discriminant <= 0)
		{
			return false; // Tangency does not penetrate the body.
		}

		const double root = std::sqrt(discriminant);
		enter = std::max(enter, (-dot - root) / travelSquared);
		leave = std::min(leave, (-dot + root) / travelSquared);
	}

	if (dz == 0)
	{
		if (std::abs(z) >= height)
		{
			return false;
		}
	}
	else
	{
		const double first = (-height - z) / dz;
		const double second = (height - z) / dz;
		enter = std::max(enter, std::min(first, second));
		leave = std::min(leave, std::max(first, second));
	}

	return enter < leave;
}

bool PlayerCellGrid::Update(uint64 id, RequestPlayerPosition position)
{
	if (id == 0 || !WorldProtocol::IsValid(position))
	{
		return false;
	}

	if (const auto found = positions.find(id); found != positions.end())
	{
		if (CellKey(found->second) == CellKey(position))
		{
			found->second = position;
			return true;
		}

		Remove(id);
	}

	positions.emplace(id, position);
	cells[CellKey(position)].push_back(id);
	return true;
}

void PlayerCellGrid::Remove(uint64 id)
{
	const auto found = positions.find(id);
	if (found == positions.end())
	{
		return;
	}

	const auto cell = cells.find(CellKey(found->second));
	std::erase(cell->second, id);
	if (cell->second.empty())
	{
		cells.erase(cell);
	}

	positions.erase(found);
}

bool PlayerCellGrid::CanMove(uint64 id, RequestPlayerPosition from, RequestPlayerPosition to) const
{
	if (id == 0 || !WorldProtocol::IsValid(from) || !WorldProtocol::IsValid(to))
	{
		return false;
	}

	constexpr double clearance = BODY_RADIUS * 2;
	const int32 minX = CellCoordinate(std::min(from.x, to.x) - clearance);
	const int32 maxX = CellCoordinate(std::max(from.x, to.x) + clearance);
	const int32 minY = CellCoordinate(std::min(from.y, to.y) - clearance);
	const int32 maxY = CellCoordinate(std::max(from.y, to.y) + clearance);
	const uint64 cellCount = static_cast<uint64>(maxX - minX + 1) * static_cast<uint64>(maxY - minY + 1);
	if (cellCount > cells.size())
	{
		for (const auto& [otherId, position] : positions)
		{
			const int32 x = CellCoordinate(position.x), y = CellCoordinate(position.y);
			if (otherId != id && x >= minX && x <= maxX && y >= minY && y <= maxY && BlocksMovement(from, to, position))
			{
				return false;
			}
		}

		return true;
	}

	for (int32 x = minX; x <= maxX; ++x)
	{
		for (int32 y = minY; y <= maxY; ++y)
		{
			const auto cell = cells.find(CellKey(x, y));
			if (cell == cells.end())
			{
				continue;
			}

			for (uint64 otherId : cell->second)
			{
				if (otherId != id && BlocksMovement(from, to, positions.at(otherId)))
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool PlayerCellGrid::CanOccupy(uint64 id, RequestPlayerPosition position) const
{
	if (id == 0 || !WorldProtocol::IsValid(position))
	{
		return false;
	}

	constexpr double clearance = BODY_RADIUS * 2;
	for (int32 x = CellCoordinate(position.x - clearance); x <= CellCoordinate(position.x + clearance); ++x)
	{
		for (int32 y = CellCoordinate(position.y - clearance); y <= CellCoordinate(position.y + clearance); ++y)
		{
			const auto cell = cells.find(CellKey(x, y));
			if (cell == cells.end())
			{
				continue;
			}

			for (uint64 otherId : cell->second)
			{
				const auto& other = positions.at(otherId);
				const double dx = position.x - other.x, dy = position.y - other.y;
				if (otherId != id && dx * dx + dy * dy < clearance * clearance && std::abs(position.z - other.z) < BODY_HEIGHT)
				{
					return false;
				}
			}
		}
	}

	return true;
}
