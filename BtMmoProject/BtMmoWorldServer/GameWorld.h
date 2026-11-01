#pragma once
#include <Types.h>
#include "MonsterSpawner.h"
#include "MonsterDespawner.h"
#include "MonsterSpawnSystem.h"
#include "MonsterBehaviorSystem.h"
#include "PlayerCellGrid.h"
#include "TileGrid.h"
#include <NetworkCore/JobQueue.h>
#include <NetworkCore/Session.h>

struct PlayerMeleeSettings
{
	float range{350};
	float height{200};
	int damage{50};
	std::chrono::milliseconds cooldown{400};
	std::chrono::milliseconds hitWindow{600};
	std::chrono::seconds positionTimeout{2};
	std::size_t maxTargets{64};
};

class WorldMetrics;
class GameWorld final : public JobQueue
{
public:
	explicit GameWorld(CoreRuntime& runtime, uint32 tileSize = TileGrid::DEFAULT_TILE_SIZE,
	                   std::shared_ptr<const NavigationMesh> navigation = nullptr, std::shared_ptr<WorldMetrics> metrics = nullptr);
	~GameWorld();
	void EnterWorld(uint64 id, SessionPtr session);
	void LeaveWorld(uint64 id);
	void UpdatePlayerPosition(uint64 id, RequestPlayerPosition position);
	void UpdatePlayerMove(uint64 id, RequestPlayerMove movement);
	void UpdatePlayerState(uint64 id, RequestPlayerState state);
	void ReportMeleeHit(uint64 playerId, RequestMeleeHit hit);
	using SpawnResult = std::expected<uint64, std::string>;
	void SpawnMonster(MonsterSpawnInfo info, std::function<void(SpawnResult)> onComplete = {});
	void DespawnMonster(uint64 id, std::function<void(bool)> onComplete = {});
	void DamageMonster(uint64 id, int damage, std::function<void(int)> onComplete = {});
	void GetMonsterCount(std::function<void(std::size_t)> onComplete);
	std::future<std::expected<void, std::string>> StartMonsterSpawning(std::shared_ptr<const NavigationMesh> navigation, MonsterSpawnConfig config);
	void StopMonsterSpawning();
	void GetMonsterSpawnPoints(std::function<void(std::vector<MonsterSpawnPointSnapshot>)> onComplete);
private:
	std::shared_ptr<GameWorld> GetWorldPtr();
	void ScheduleMonsterTick();
	void ScheduleBehaviorTick();
	bool RemoveMonster(uint64 id);
	bool SendToPlayer(uint64 id, SendBufferPtr packet);
	void BroadcastPlayer(uint64 id, SendBufferPtr packet);
	void BroadcastMonster(uint64 id, SendBufferPtr packet);
	bool ShowPlayer(uint64 viewerId, uint64 playerId);
	void HidePlayer(uint64 viewerId, uint64 playerId);
	bool ShowMonster(uint64 viewerId, uint64 monsterId);
	void HideMonster(uint64 viewerId, uint64 monsterId);
	void RefreshPlayerView(uint64 id);
	void UpdatePlayerVisibility(uint64 id);
	void RemovePlayerVisibility(uint64 id);
	void AddMonsterVisibility(const Monster& monster);
	void UpdateMonsterVisibility(const MonsterBehaviorSnapshot& info);
	void RemoveMonsterVisibility(uint64 id);
	int ApplyMonsterDamage(uint64 id, int damage);
	void ApplyPlayerMove(uint64 id, RequestPlayerMove movement);
	std::optional<RequestPlayerPosition> FindPlayerSpawn(uint64 id, RequestPlayerPosition requested) const;
	struct MeleeAttack
	{
		uint64 sequence{};
		std::chrono::steady_clock::time_point started{};
		std::unordered_set<uint64> targets;
	};

	std::shared_ptr<WorldMetrics> metrics;
	PlayerMeleeSettings meleeSettings;
	std::unordered_map<uint64, MeleeAttack> meleeAttacks;
	std::unordered_map<uint64, std::weak_ptr<Session>> sessions;
	MonsterBehaviorSystem::Players players;
	PlayerCellGrid playerCells;
	std::shared_ptr<const NavigationMesh> playerNavigation;
	TileGrid playerTiles;
	TileGrid monsterTiles;
	struct PlayerView
	{
		std::unordered_set<uint64> players;
		std::unordered_set<uint64> monsters;
	};

	std::unordered_map<uint64, PlayerView> playerViews;
	std::unordered_map<uint64, RequestPlayerState> playerStates;
	std::unordered_map<uint64, RequestPlayerMove> playerMoves;
	std::unordered_map<uint64, NotifyMonsterState> monsterStates;
	MonsterSpawner monsterSpawner;
	MonsterDespawner monsterDespawner;
	std::unique_ptr<MonsterSpawnSystem> monsterSpawnSystem;
	std::unique_ptr<MonsterBehaviorSystem> monsterBehaviorSystem;
	TimerHandle monsterTick;
	TimerHandle behaviorTick;
};
