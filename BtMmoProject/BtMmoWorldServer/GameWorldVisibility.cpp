#include <Pch.h>
#include "GameWorld.h"
#include <WorldPacketFactory.h>

bool GameWorld::SendToPlayer(uint64 id, SendBufferPtr packet)
{
	const auto found = sessions.find(id);
	if (found == sessions.end())
	{
		return false;
	}

	const auto session = found->second.lock();
	if (!session || !session->IsConnected())
	{
		return false;
	}

	if (!session->Send(packet))
	{
		session->Disconnect();
		return false;
	}

	return true;
}

bool GameWorld::ShowPlayer(uint64 viewerId, uint64 playerId)
{
	const auto view = playerViews.find(viewerId);
	if (viewerId == playerId || view == playerViews.end() || !view->second.players.insert(playerId).second)
	{
		return false;
	}

	const auto& player = players.at(playerId);
	SendToPlayer(viewerId, WorldPacketFactory::MakeSendBuffer(
	                           NotifyPlayerSpawn{.id = playerId, .movement = playerMoves.at(playerId), .state = playerStates.at(playerId), .hp = static_cast<uint32>(player.hp), .maxHp = WorldPlayer::MAX_HP}));
	return true;
}

void GameWorld::HidePlayer(uint64 viewerId, uint64 playerId)
{
	if (const auto view = playerViews.find(viewerId); view != playerViews.end() && view->second.players.erase(playerId))
	{
		SendToPlayer(viewerId, WorldPacketFactory::MakeSendBuffer(NotifyPlayerDespawn{playerId}));
	}
}

bool GameWorld::ShowMonster(uint64 viewerId, uint64 monsterId)
{
	const auto view = playerViews.find(viewerId);
	const auto monster = monsterSpawner.Find(monsterId);
	if (!monster || view == playerViews.end() || !view->second.monsters.insert(monsterId).second)
	{
		return false;
	}

	const auto position = monster->GetPosition();
	if (!SendToPlayer(viewerId, WorldPacketFactory::MakeSendBuffer(NotifyMonsterSpawn{
	                                monsterId, monster->GetOwnerSpawnerId(), monster->GetTemplateId(), position.x, position.y, position.z})) ||
	    !SendToPlayer(viewerId, WorldPacketFactory::MakeSendBuffer(NotifyMonsterHealth{
	                                monsterId, static_cast<uint32>(monster->GetHp()), static_cast<uint32>(monster->GetMaxHp())})))
	{
		return true;
	}

	if (monsterBehaviorSystem)
	{
		const auto snapshot = monsterBehaviorSystem->Snapshot(*monster);
		// Re-entry includes current state even when it has not changed this tick.
		if (SendToPlayer(viewerId, WorldPacketFactory::MakeSendBuffer(snapshot.state)))
		{
			SendToPlayer(viewerId, WorldPacketFactory::MakeSendBuffer(snapshot.movement));
		}
	}

	return true;
}

void GameWorld::HideMonster(uint64 viewerId, uint64 monsterId)
{
	const auto view = playerViews.find(viewerId);
	if (view != playerViews.end() && view->second.monsters.erase(monsterId))
	{
		SendToPlayer(viewerId, WorldPacketFactory::MakeSendBuffer(NotifyMonsterDespawn{monsterId}));
	}
}

void GameWorld::RefreshPlayerView(uint64 id)
{
	const auto tile = playerTiles.FindTile(id);
	if (!tile)
	{
		return;
	}

	auto& view = playerViews[id];
	const auto nearbyPlayers = playerTiles.FindNearby(*tile);
	const auto nearbyMonsters = monsterTiles.FindNearby(*tile);
	const auto previousPlayers = view.players;
	const auto previousMonsters = view.monsters;
	for (uint64 other : previousPlayers)
	{
		if (!playerTiles.IsNearby(other, *tile))
		{
			HidePlayer(id, other);
		}
	}

	for (uint64 other : previousMonsters)
	{
		if (!monsterTiles.IsNearby(other, *tile))
		{
			HideMonster(id, other);
		}
	}

	for (uint64 other : nearbyPlayers)
	{
		ShowPlayer(id, other);
	}

	for (uint64 other : nearbyMonsters)
	{
		ShowMonster(id, other);
	}
}

void GameWorld::UpdatePlayerVisibility(uint64 id)
{
	const auto& movement = playerMoves.at(id);
	const auto previous = playerTiles.FindTile(id);
	const bool changed = playerTiles.Update(id, {movement.x, movement.y, movement.z});
	const auto tile = playerTiles.FindTile(id);
	if (!tile)
	{
		return;
	}

	if (changed)
	{
		RefreshPlayerView(id);
		if (previous)
		{
			for (uint64 viewer : playerTiles.FindNearby(*previous))
			{
				if (!playerTiles.IsNearby(viewer, *tile))
				{
					HidePlayer(viewer, id);
				}
			}
		}
	}

	const auto packet = WorldPacketFactory::MakeSendBuffer(NotifyPlayerMove{id, movement});
	for (uint64 viewer : playerTiles.FindNearby(*tile))
	{
		if (viewer != id && !ShowPlayer(viewer, id))
		{
			SendToPlayer(viewer, packet);
		}
	}
}

void GameWorld::RemovePlayerVisibility(uint64 id)
{
	if (const auto tile = playerTiles.FindTile(id))
	{
		for (uint64 viewer : playerTiles.FindNearby(*tile))
		{
			HidePlayer(viewer, id);
		}
	}

	playerTiles.Remove(id);
	playerViews.erase(id);
}

void GameWorld::AddMonsterVisibility(const Monster& monster)
{
	const auto position = monster.GetPosition();
	monsterTiles.Update(monster.GetId(), {position.x, position.y, position.z});
	if (const auto tile = monsterTiles.FindTile(monster.GetId()))
	{
		for (uint64 viewer : playerTiles.FindNearby(*tile))
		{
			ShowMonster(viewer, monster.GetId());
		}
	}
}

void GameWorld::UpdateMonsterVisibility(const MonsterBehaviorSnapshot& info)
{
	const uint64 id = info.movement.id;
	const auto previous = monsterSnapshots.find(id);
	const bool changedState = previous == monsterSnapshots.end() || previous->second.state != info.state;
	const auto& move = info.movement;
	const bool changedMove = previous == monsterSnapshots.end() ||
	                         std::tie(previous->second.movement.x, previous->second.movement.y, previous->second.movement.z,
	                                  previous->second.movement.yaw, previous->second.movement.speed) !=
	                             std::tie(move.x, move.y, move.z, move.yaw, move.speed);
	if (!changedState && !changedMove)
	{
		return;
	}

	monsterSnapshots.insert_or_assign(id, info);
	const auto previousTile = monsterTiles.FindTile(id);
	const bool changedTile = monsterTiles.Update(id, {info.movement.x, info.movement.y, info.movement.z});
	const auto tile = monsterTiles.FindTile(id);
	if (!tile)
	{
		return;
	}

	if (changedTile && previousTile)
	{
		for (uint64 viewer : playerTiles.FindNearby(*previousTile))
		{
			if (!playerTiles.IsNearby(viewer, *tile))
			{
				HideMonster(viewer, id);
			}
		}
	}

	const auto statePacket = changedState ? WorldPacketFactory::MakeSendBuffer(info.state) : SendBufferPtr{};
	// The UE client consumes animation state with a movement sample, including
	// stationary attacks. Always pair a changed state with the current movement.
	const auto movePacket = WorldPacketFactory::MakeSendBuffer(info.movement);
	for (uint64 viewer : playerTiles.FindNearby(*tile))
	{
		if (ShowMonster(viewer, id))
		{
			continue;
		}

		if (statePacket)
		{
			SendToPlayer(viewer, statePacket);
		}

		SendToPlayer(viewer, movePacket);
	}
}

void GameWorld::RemoveMonsterVisibility(uint64 id)
{
	if (const auto tile = monsterTiles.FindTile(id))
	{
		for (uint64 viewer : playerTiles.FindNearby(*tile))
		{
			HideMonster(viewer, id);
		}
	}

	monsterTiles.Remove(id);
}

void GameWorld::BroadcastPlayer(uint64 id, SendBufferPtr packet)
{
	if (const auto tile = playerTiles.FindTile(id))
	{
		for (uint64 viewer : playerTiles.FindNearby(*tile))
		{
			const auto view = playerViews.find(viewer);
			if (view != playerViews.end() && view->second.players.contains(id))
			{
				SendToPlayer(viewer, packet);
			}
		}
	}
}

void GameWorld::BroadcastMonster(uint64 id, SendBufferPtr packet)
{
	if (const auto tile = monsterTiles.FindTile(id))
	{
		for (uint64 viewer : playerTiles.FindNearby(*tile))
		{
			const auto view = playerViews.find(viewer);
			if (view != playerViews.end() && view->second.monsters.contains(id))
			{
				SendToPlayer(viewer, packet);
			}
		}
	}
}
