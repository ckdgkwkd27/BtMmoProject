#pragma once

#include <WorldPackets.h>

class PlayerCellGrid final
{
public:
	static constexpr double CELL_SIZE = 100;
	static constexpr double BODY_RADIUS = 42;
	static constexpr double BODY_HEIGHT = 190;

	bool Update(uint64 id, RequestPlayerPosition position);
	void Remove(uint64 id);
	bool CanMove(uint64 id, RequestPlayerPosition from, RequestPlayerPosition to) const;
	bool CanOccupy(uint64 id, RequestPlayerPosition position) const;
	std::size_t Count() const { return positions.size(); }

	std::size_t CellCount() const { return cells.size(); }
private:
	std::unordered_map<uint64, RequestPlayerPosition> positions;
	std::unordered_map<uint64, std::vector<uint64>> cells;
};
