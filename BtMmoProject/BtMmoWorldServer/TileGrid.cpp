#include <Pch.h>
#include "TileGrid.h"
#include <WorldProtocol.h>

static uint64 TileKey(TileCoordinate tile)
{
	return (static_cast<uint64>(static_cast<uint32>(tile.x)) << 32) | static_cast<uint32>(tile.y);
}

TileGrid::TileGrid(uint32 _tileSize) : tileSize(_tileSize)
{
	if (tileSize == 0)
	{
		throw std::invalid_argument("Tile size must be positive");
	}
}

bool TileGrid::Update(uint64 id, RequestPlayerPosition position)
{
	if (id == 0 || !WorldProtocol::IsValid(position))
	{
		return false;
	}

	const TileCoordinate tile{static_cast<int32>(std::floor(position.x / tileSize)),
	                          static_cast<int32>(std::floor(position.y / tileSize))};
	if (const auto previous = FindTile(id); previous && *previous == tile)
	{
		return false;
	}

	Remove(id);
	objects.emplace(id, tile);
	tiles[TileKey(tile)].insert(id);
	return true;
}

void TileGrid::Remove(uint64 id)
{
	const auto found = objects.find(id);
	if (found == objects.end())
	{
		return;
	}

	const auto tile = tiles.find(TileKey(found->second));
	tile->second.erase(id);
	if (tile->second.empty())
	{
		tiles.erase(tile);
	}

	objects.erase(found);
}

std::optional<TileCoordinate> TileGrid::FindTile(uint64 id) const
{
	const auto found = objects.find(id);
	return found == objects.end() ? std::nullopt : std::optional{found->second};
}

std::vector<uint64> TileGrid::FindNearby(TileCoordinate center) const
{
	std::vector<uint64> result;
	for (int32 dx = -1; dx <= 1; ++dx)
	{
		for (int32 dy = -1; dy <= 1; ++dy)
		{
			if (const auto found = tiles.find(TileKey({center.x + dx, center.y + dy})); found != tiles.end())
			{
				result.append_range(found->second);
			}
		}
	}

	std::ranges::sort(result);
	return result;
}

bool TileGrid::IsNearby(uint64 id, TileCoordinate center) const
{
	const auto tile = FindTile(id);
	return tile && std::abs(tile->x - center.x) <= 1 && std::abs(tile->y - center.y) <= 1;
}
