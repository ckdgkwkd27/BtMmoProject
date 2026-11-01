#pragma once

#include <WorldPackets.h>

struct TileCoordinate
{
	int32 x{}, y{};
	bool operator==(const TileCoordinate&) const = default;
};

class TileGrid final
{
public:
	static constexpr uint32 DEFAULT_TILE_SIZE = 2000;
	explicit TileGrid(uint32 _tileSize = DEFAULT_TILE_SIZE);
	bool Update(uint64 id, RequestPlayerPosition position);
	void Remove(uint64 id);
	std::optional<TileCoordinate> FindTile(uint64 id) const;
	std::vector<uint64> FindNearby(TileCoordinate center) const;
	bool IsNearby(uint64 id, TileCoordinate center) const;
	std::size_t Count() const { return objects.size(); }

	std::size_t TileCount() const { return tiles.size(); }
private:
	uint32 tileSize;
	std::unordered_map<uint64, TileCoordinate> objects;
	std::unordered_map<uint64, std::unordered_set<uint64>> tiles;
};
