#pragma once
#include "Types.h"

enum class WorldPacketId : uint16
{
	PKT_CS_PING = 3,
	PKT_CS_PLAYER_POSITION = 4,
	PKT_CS_MELEE_HIT = 5,
	PKT_CS_PLAYER_STATE = 6,
	PKT_CS_PLAYER_MOVE = 7,
	PKT_SC_WELCOME = 101,
	PKT_SC_PONG = 104,
	PKT_SC_SPAWNER = 105,
	PKT_SC_MONSTER_SPAWN = 106,
	PKT_SC_MONSTER_DESPAWN = 107,
	PKT_SC_MONSTER_STATE = 108,
	PKT_SC_PLAYER_HEALTH = 109,
	PKT_SC_MONSTER_HEALTH = 110,
	PKT_SC_PLAYER_SPAWN = 111,
	PKT_SC_PLAYER_STATE = 112,
	PKT_SC_PLAYER_DESPAWN = 113,
	PKT_SC_PLAYER_MOVE = 114,
	PKT_SC_MONSTER_MOVE = 115
};

struct RequestPing
{
	static constexpr auto ID = WorldPacketId::PKT_CS_PING;
};

struct ReturnPong
{
	static constexpr auto ID = WorldPacketId::PKT_SC_PONG;
};

struct ReturnWelcome
{
	static constexpr auto ID = WorldPacketId::PKT_SC_WELCOME;

	uint64 sessionId{};
	uint16 spawnerCount{};
};

struct NotifySpawner
{
	static constexpr auto ID = WorldPacketId::PKT_SC_SPAWNER;

	uint64 id{};
	double x{}, y{}, z{};
	uint16 maxAlive{}, alive{}, pending{};
};

struct NotifyMonsterSpawn
{
	static constexpr auto ID = WorldPacketId::PKT_SC_MONSTER_SPAWN;

	uint64 id{}, ownerSpawnerId{};
	uint32 templateId{};
	double x{}, y{}, z{};
};

struct NotifyMonsterDespawn
{
	static constexpr auto ID = WorldPacketId::PKT_SC_MONSTER_DESPAWN;

	uint64 id{};
};

enum class EMonsterAction : BYTE
{
	IDLE,
	WALK,
	RUN,
	ATTACK
};

struct RequestPlayerPosition
{
	static constexpr auto ID = WorldPacketId::PKT_CS_PLAYER_POSITION;

	double x{}, y{}, z{};
};

struct NotifyMonsterMove
{
	static constexpr auto ID = WorldPacketId::PKT_SC_MONSTER_MOVE;

	uint64 id{};
	double x{}, y{}, z{}, yaw{}, speed{};
};

struct NotifyMonsterState
{
	static constexpr auto ID = WorldPacketId::PKT_SC_MONSTER_STATE;

	uint64 id{};
	EMonsterAction action{EMonsterAction::IDLE};
	uint32 attackSequence{};
	uint64 targetId{};
	bool operator==(const NotifyMonsterState&) const = default;
};

struct NotifyPlayerHealth
{
	static constexpr auto ID = WorldPacketId::PKT_SC_PLAYER_HEALTH;

	uint32 hp{}, maxHp{100};
};

struct RequestMeleeHit
{
	static constexpr auto ID = WorldPacketId::PKT_CS_MELEE_HIT;

	uint64 attackSequence{}, monsterId{};
};

struct NotifyMonsterHealth
{
	static constexpr auto ID = WorldPacketId::PKT_SC_MONSTER_HEALTH;

	uint64 id{};
	uint32 hp{}, maxHp{};
};

struct RequestPlayerMove
{
	static constexpr auto ID = WorldPacketId::PKT_CS_PLAYER_MOVE;

	double x{}, y{}, z{}, yaw{}, velocityX{}, velocityY{}, velocityZ{};
	BYTE falling{};
};

struct NotifyPlayerMove
{
	static constexpr auto ID = WorldPacketId::PKT_SC_PLAYER_MOVE;

	uint64 id{};
	RequestPlayerMove movement{};
};

struct RequestPlayerState
{
	static constexpr auto ID = WorldPacketId::PKT_CS_PLAYER_STATE;
	BYTE attackIndex{};
	uint64 attackSequence{};
	bool operator==(const RequestPlayerState&) const = default;
};

struct NotifyPlayerSpawn
{
	static constexpr auto ID = WorldPacketId::PKT_SC_PLAYER_SPAWN;
	uint64 id{};
	RequestPlayerMove movement{};
	RequestPlayerState state{};
	uint32 hp{100}, maxHp{100};
};

struct NotifyPlayerState
{
	static constexpr auto ID = WorldPacketId::PKT_SC_PLAYER_STATE;
	uint64 id{};
	RequestPlayerState state{};
	uint32 hp{100}, maxHp{100};
};

struct NotifyPlayerDespawn
{
	static constexpr auto ID = WorldPacketId::PKT_SC_PLAYER_DESPAWN;
	uint64 id{};
};
