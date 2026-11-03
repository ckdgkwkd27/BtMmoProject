#pragma once

#include "BehaviorTree.h"
#include "BattleSlot.h"
#include "MonsterSpawner.h"
#include <WorldProtocol.h>

struct MonsterBehaviorSettings
{
	float walkSpeed{120};
	float runSpeed{320};
	float patrolRadius{600};
	float detectionRange{900};
	float attackRange{150};
	float attackHeight{100};
	float leashRange{1400};
	float attackDuration{WorldProtocol::MONSTER_ATTACK_SECONDS};
	float attackCooldown{1.5f};
	int attackDamage{4};
	float bodyRadius{38.5f};
	float surroundRange{480};
	float slotTolerance{12};
};

struct WorldPlayer
{
	static constexpr int MAX_HP = 100;
	NavPoint position{};
	int hp{MAX_HP};
	std::chrono::steady_clock::time_point lastSeen{};
	std::chrono::steady_clock::time_point attackEnded{};
	RequestPlayerPosition moveCheckPosition{};
	std::chrono::steady_clock::time_point moveCheckTime{};
};

struct MonsterBehaviorSnapshot
{
	NotifyMonsterMove movement;
	NotifyMonsterState state;
};

class MonsterBehaviorSystem final
{
public:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;
	using Players = std::unordered_map<uint64, WorldPlayer>;
	using StateCallback = std::function<void(const MonsterBehaviorSnapshot&)>;
	using DamageCallback = std::function<void(uint64, int)>;

	MonsterBehaviorSystem(std::shared_ptr<const NavigationMesh> _navigation, MonsterSpawner& _monsters, Players& _players, StateCallback _onState, DamageCallback _onDamage, uint32 seed);

	void Tick(TimePoint now);
	void Remove(uint64 id);

	MonsterBehaviorSnapshot Snapshot(const Monster& monster) const;
	const MonsterBehaviorSettings& GetSettings() const { return settings; }

	std::optional<BattleSlot::Key> GetBattleSlot(uint64 monsterId) const;
private:
	struct Blackboard
	{
		NavPoint home{};
		std::optional<NavPoint> slotPosition;
		std::vector<NavPoint> path;
		std::size_t waypoint{};
		uint64 targetId{};
		uint32 attackSequence{};
		float yaw{};
		float speed{};
		EMonsterAction action{EMonsterAction::IDLE};

		TimePoint nextPatrol{};
		TimePoint nextPath{};
		TimePoint nextTarget{};
		TimePoint nextAttack{};
		TimePoint attackUntil{};
		TimePoint nextAvoidance{};
	};

	struct Context
	{
		Monster& monster;
		Blackboard& brain;
		TimePoint now;
		float delta;
	};

	using Node = BehaviorNode<Context>;

	Node CreateBehaviorTree();

	bool HasTarget(const Context& context) const;
	void SelectTarget(Context& context);
	void ReleaseTarget(uint64 monsterId, Blackboard& brain);
	void UpdateBattleSlot(Context& context);

	BehaviorStatus Attack(Context& context);
	BehaviorStatus Chase(Context& context);
	BehaviorStatus Patrol(Context& context);
	void PausePatrol(Context& context);

	bool FollowPath(Context& context, float speed);
	void FaceTarget(Context& context, NavPoint target);

	bool FindCrowdPath(Context& context);
	bool CanMove(uint64 id, NavPoint from, NavPoint to) const;
	void UpdateCrowdPosition(uint64 id, NavPoint position);

	std::shared_ptr<const NavigationMesh> navigation;
	MonsterSpawner& monsters;
	Players& players;
	StateCallback onState;
	DamageCallback onDamage;
	MonsterBehaviorSettings settings;
	std::mt19937 random;

	std::unordered_map<uint64, Blackboard> blackboards;
	std::unordered_map<uint64, BattleSlot> battleSlots;
	std::unordered_map<uint64, NavPoint> crowdPositions;
	std::unordered_map<uint64, std::vector<uint64>> crowdCells;

	TimePoint previousTick{};
	Node tree;
};
