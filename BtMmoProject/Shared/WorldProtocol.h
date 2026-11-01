#pragma once
#include "WorldPackets.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <span>
#include <optional>
#include <tuple>
#include <unordered_set>

class WorldProtocol
{
public:
	static constexpr std::size_t HEADER_SIZE = 4;
	static constexpr std::size_t MAX_PACKET_SIZE = 8192;
	static constexpr uint16 MAX_SPAWNERS = 256;
	static constexpr std::size_t WELCOME_SIZE = 10;
	static constexpr std::size_t SPAWNER_SIZE = 38;
	static constexpr std::size_t MONSTER_SPAWN_SIZE = 44;
	static constexpr std::size_t MONSTER_DESPAWN_SIZE = 8;
	static constexpr std::size_t PLAYER_POSITION_SIZE = 24;
	static constexpr std::size_t MONSTER_STATE_SIZE = 21;
	static constexpr std::size_t MONSTER_MOVE_SIZE = 48;
	static constexpr std::size_t PLAYER_HEALTH_SIZE = 8;
	static constexpr std::size_t MELEE_HIT_SIZE = 16;
	static constexpr std::size_t MONSTER_HEALTH_SIZE = 16;
	static constexpr unsigned MONSTER_TICK_MS = 100;
	static constexpr float MONSTER_ATTACK_SECONDS = 0.8f;
	static constexpr std::size_t PLAYER_MOVE_SIZE = 57;
	static constexpr std::size_t PLAYER_MOVE_NOTIFY_SIZE = 65;
	static constexpr std::size_t PLAYER_STATE_SIZE = 9;
	static constexpr std::size_t PLAYER_SPAWN_SIZE = 82;
	static constexpr std::size_t PLAYER_NOTIFY_SIZE = 25;
	static constexpr unsigned PLAYER_TICK_MS = 100;

	static bool IsValid(const RequestPlayerMove& value)
	{
		return IsValid(RequestPlayerPosition{value.x, value.y, value.z}) &&
		       std::isfinite(value.yaw) && std::abs(value.yaw) <= 180 &&
		       std::isfinite(value.velocityX) && std::abs(value.velocityX) <= 10000 &&
		       std::isfinite(value.velocityY) && std::abs(value.velocityY) <= 10000 &&
		       std::isfinite(value.velocityZ) && std::abs(value.velocityZ) <= 10000 && value.falling <= 1;
	}

	static bool IsValid(const RequestPlayerState& value)
	{
		return value.attackIndex <= 3 && (value.attackIndex == 0 || value.attackSequence != 0);
	}

	static bool IsValid(const NotifyPlayerMove& value) { return value.id != 0 && IsValid(value.movement); }

	static bool IsValid(const NotifyPlayerState& value)
	{
		return value.id != 0 && IsValid(value.state) && IsValid(NotifyPlayerHealth{value.hp, value.maxHp});
	}

	static bool IsValid(const NotifyPlayerSpawn& value)
	{
		return IsValid(value.movement) && IsValid(NotifyPlayerState{value.id, value.state, value.hp, value.maxHp});
	}

	static bool IsValid(const NotifyPlayerDespawn& value) { return value.id != 0; }

	static std::array<BYTE, PLAYER_MOVE_SIZE> Encode(const RequestPlayerMove& value)
	{
		std::array<BYTE, PLAYER_MOVE_SIZE> bytes{};
		const double fields[] = {value.x, value.y, value.z, value.yaw, value.velocityX, value.velocityY, value.velocityZ};
		for (std::size_t i = 0; i < 7; ++i)
		{
			WriteU64(bytes.data() + i * 8, std::bit_cast<uint64>(fields[i]));
		}

		bytes[56] = value.falling;
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, RequestPlayerMove& value)
	{
		if (bytes.size() != PLAYER_MOVE_SIZE)
		{
			return false;
		}

		double fields[7]{};
		for (std::size_t i = 0; i < 7; ++i)
		{
			fields[i] = std::bit_cast<double>(ReadU64(bytes.data() + i * 8));
		}

		value = {fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6], bytes[56]};
		return IsValid(value);
	}

	static std::array<BYTE, PLAYER_MOVE_NOTIFY_SIZE> Encode(const NotifyPlayerMove& value)
	{
		std::array<BYTE, PLAYER_MOVE_NOTIFY_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		const auto movement = Encode(value.movement);
		for (std::size_t i = 0; i < movement.size(); ++i)
		{
			bytes[8 + i] = movement[i];
		}

		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyPlayerMove& value)
	{
		if (bytes.size() != PLAYER_MOVE_NOTIFY_SIZE || !Decode(bytes.subspan(8), value.movement))
		{
			return false;
		}

		value.id = ReadU64(bytes.data());
		return IsValid(value);
	}

	static std::array<BYTE, PLAYER_STATE_SIZE> Encode(const RequestPlayerState& value)
	{
		std::array<BYTE, PLAYER_STATE_SIZE> bytes{};
		bytes[0] = value.attackIndex;
		WriteU64(bytes.data() + 1, value.attackSequence);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, RequestPlayerState& value)
	{
		if (bytes.size() != PLAYER_STATE_SIZE)
		{
			return false;
		}

		value = {bytes[0], ReadU64(bytes.data() + 1)};
		return IsValid(value);
	}

	static std::array<BYTE, PLAYER_NOTIFY_SIZE> Encode(const NotifyPlayerState& value)
	{
		std::array<BYTE, PLAYER_NOTIFY_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		const auto state = Encode(value.state);
		for (std::size_t i = 0; i < state.size(); ++i)
		{
			bytes[8 + i] = state[i];
		}

		WriteU32(bytes.data() + 17, value.hp);
		WriteU32(bytes.data() + 21, value.maxHp);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyPlayerState& value)
	{
		if (bytes.size() != PLAYER_NOTIFY_SIZE || !Decode(bytes.subspan(8, PLAYER_STATE_SIZE), value.state))
		{
			return false;
		}

		value.id = ReadU64(bytes.data());
		value.hp = ReadU32(bytes.data() + 17);
		value.maxHp = ReadU32(bytes.data() + 21);
		return IsValid(value);
	}

	static std::array<BYTE, PLAYER_SPAWN_SIZE> Encode(const NotifyPlayerSpawn& value)
	{
		std::array<BYTE, PLAYER_SPAWN_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		const auto movement = Encode(value.movement);
		const auto state = Encode(value.state);
		for (std::size_t i = 0; i < movement.size(); ++i)
		{
			bytes[8 + i] = movement[i];
		}

		for (std::size_t i = 0; i < state.size(); ++i)
		{
			bytes[65 + i] = state[i];
		}

		WriteU32(bytes.data() + 74, value.hp);
		WriteU32(bytes.data() + 78, value.maxHp);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyPlayerSpawn& value)
	{
		if (bytes.size() != PLAYER_SPAWN_SIZE || !Decode(bytes.subspan(8, PLAYER_MOVE_SIZE), value.movement) ||
		    !Decode(bytes.subspan(65, PLAYER_STATE_SIZE), value.state))
		{
			return false;
		}

		value.id = ReadU64(bytes.data());
		value.hp = ReadU32(bytes.data() + 74);
		value.maxHp = ReadU32(bytes.data() + 78);
		return IsValid(value);
	}

	static std::array<BYTE, 8> Encode(const NotifyPlayerDespawn& value)
	{
		std::array<BYTE, 8> bytes{};
		WriteU64(bytes.data(), value.id);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyPlayerDespawn& value)
	{
		if (bytes.size() != 8)
		{
			return false;
		}

		value.id = ReadU64(bytes.data());
		return IsValid(value);
	}

	static bool IsValid(const RequestMeleeHit& value)
	{
		return value.attackSequence != 0 && value.monsterId != 0;
	}

	static bool IsValid(const NotifyMonsterHealth& value)
	{
		return value.id != 0 && value.maxHp > 0 && value.hp <= value.maxHp;
	}

	static std::array<BYTE, MELEE_HIT_SIZE> Encode(const RequestMeleeHit& value)
	{
		std::array<BYTE, MELEE_HIT_SIZE> bytes{};
		WriteU64(bytes.data(), value.attackSequence);
		WriteU64(bytes.data() + 8, value.monsterId);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, RequestMeleeHit& value)
	{
		if (bytes.size() != MELEE_HIT_SIZE)
		{
			return false;
		}

		value = {ReadU64(bytes.data()), ReadU64(bytes.data() + 8)};
		return IsValid(value);
	}

	static std::array<BYTE, MONSTER_HEALTH_SIZE> Encode(const NotifyMonsterHealth& value)
	{
		std::array<BYTE, MONSTER_HEALTH_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		WriteU32(bytes.data() + 8, value.hp);
		WriteU32(bytes.data() + 12, value.maxHp);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyMonsterHealth& value)
	{
		if (bytes.size() != MONSTER_HEALTH_SIZE)
		{
			return false;
		}

		value = {ReadU64(bytes.data()), ReadU32(bytes.data() + 8), ReadU32(bytes.data() + 12)};
		return IsValid(value);
	}

	static bool IsValid(const RequestPlayerPosition& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
		       std::abs(value.x) <= 10000000 && std::abs(value.y) <= 10000000 && std::abs(value.z) <= 10000000;
	}

	static bool IsValid(const NotifyMonsterMove& value)
	{
		return value.id != 0 && IsValid(RequestPlayerPosition{value.x, value.y, value.z}) &&
		       std::isfinite(value.yaw) && std::abs(value.yaw) <= 180 &&
		       std::isfinite(value.speed) && value.speed >= 0 && value.speed <= 10000;
	}

	static bool IsValid(const NotifyMonsterState& value)
	{
		return value.id != 0 && value.action <= EMonsterAction::ATTACK &&
		       (value.action != EMonsterAction::ATTACK || (value.targetId != 0 && value.attackSequence != 0));
	}

	static bool IsValid(const NotifyPlayerHealth& value)
	{
		return value.maxHp > 0 && value.hp <= value.maxHp;
	}

	static std::array<BYTE, PLAYER_POSITION_SIZE> Encode(const RequestPlayerPosition& value)
	{
		std::array<BYTE, PLAYER_POSITION_SIZE> bytes{};
		WriteU64(bytes.data(), std::bit_cast<uint64>(value.x));
		WriteU64(bytes.data() + 8, std::bit_cast<uint64>(value.y));
		WriteU64(bytes.data() + 16, std::bit_cast<uint64>(value.z));
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, RequestPlayerPosition& value)
	{
		if (bytes.size() != PLAYER_POSITION_SIZE)
		{
			return false;
		}

		value = {std::bit_cast<double>(ReadU64(bytes.data())), std::bit_cast<double>(ReadU64(bytes.data() + 8)),
		         std::bit_cast<double>(ReadU64(bytes.data() + 16))};
		return IsValid(value);
	}

	static std::array<BYTE, MONSTER_MOVE_SIZE> Encode(const NotifyMonsterMove& value)
	{
		std::array<BYTE, MONSTER_MOVE_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		const double fields[] = {value.x, value.y, value.z, value.yaw, value.speed};
		for (std::size_t i = 0; i < 5; ++i)
		{
			WriteU64(bytes.data() + 8 + i * 8, std::bit_cast<uint64>(fields[i]));
		}

		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyMonsterMove& value)
	{
		if (bytes.size() != MONSTER_MOVE_SIZE)
		{
			return false;
		}

		double fields[5]{};
		for (std::size_t i = 0; i < 5; ++i)
		{
			fields[i] = std::bit_cast<double>(ReadU64(bytes.data() + 8 + i * 8));
		}

		value = {ReadU64(bytes.data()), fields[0], fields[1], fields[2], fields[3], fields[4]};
		return IsValid(value);
	}

	static std::array<BYTE, MONSTER_STATE_SIZE> Encode(const NotifyMonsterState& value)
	{
		std::array<BYTE, MONSTER_STATE_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		bytes[8] = static_cast<BYTE>(value.action);
		WriteU32(bytes.data() + 9, value.attackSequence);
		WriteU64(bytes.data() + 13, value.targetId);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyMonsterState& value)
	{
		if (bytes.size() != MONSTER_STATE_SIZE)
		{
			return false;
		}

		value = {ReadU64(bytes.data()), static_cast<EMonsterAction>(bytes[8]), ReadU32(bytes.data() + 9), ReadU64(bytes.data() + 13)};
		return IsValid(value);
	}

	static std::array<BYTE, PLAYER_HEALTH_SIZE> Encode(const NotifyPlayerHealth& value)
	{
		std::array<BYTE, PLAYER_HEALTH_SIZE> bytes{};
		WriteU32(bytes.data(), value.hp);
		WriteU32(bytes.data() + 4, value.maxHp);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyPlayerHealth& value)
	{
		if (bytes.size() != PLAYER_HEALTH_SIZE)
		{
			return false;
		}

		value = {ReadU32(bytes.data()), ReadU32(bytes.data() + 4)};
		return IsValid(value);
	}

	static uint32 ReadU32(const BYTE* p)
	{
		return (static_cast<uint32>(ReadU16(p)) << 16) | ReadU16(p + 2);
	}

	static void WriteU32(BYTE* p, uint32 value)
	{
		WriteU16(p, static_cast<uint16>(value >> 16));
		WriteU16(p + 2, static_cast<uint16>(value));
	}

	static uint16 ReadU16(const BYTE* p)
	{
		return static_cast<uint16>((p[0] << 8) | p[1]);
	}

	static void WriteU16(BYTE* p, uint16 value)
	{
		p[0] = static_cast<BYTE>(value >> 8);
		p[1] = static_cast<BYTE>(value);
	}

	static uint64 ReadU64(const BYTE* p)
	{
		uint64 value = 0;
		for (unsigned i = 0; i < 8; ++i)
		{
			value = (value << 8) | p[i];
		}

		return value;
	}

	static void WriteU64(BYTE* p, uint64 value)
	{
		for (unsigned i = 0; i < 8; ++i)
		{
			p[i] = static_cast<BYTE>(value >> (56 - i * 8));
		}
	}

	static bool IsValid(const ReturnWelcome& value)
	{
		return value.sessionId != 0 && value.spawnerCount <= MAX_SPAWNERS;
	}

	static bool IsValid(const NotifySpawner& value)
	{
		return value.id != 0 && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && value.maxAlive > 0 && value.maxAlive <= 1000 && value.alive + value.pending <= value.maxAlive;
	}

	static bool IsValid(const NotifyMonsterSpawn& value)
	{
		return value.id != 0 && value.templateId != 0 && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	static bool IsValid(const NotifyMonsterDespawn& value)
	{
		return value.id != 0;
	}

	static std::array<BYTE, MONSTER_SPAWN_SIZE> Encode(const NotifyMonsterSpawn& value)
	{
		std::array<BYTE, MONSTER_SPAWN_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		WriteU64(bytes.data() + 8, value.ownerSpawnerId);
		WriteU32(bytes.data() + 16, value.templateId);
		WriteU64(bytes.data() + 20, std::bit_cast<uint64>(value.x));
		WriteU64(bytes.data() + 28, std::bit_cast<uint64>(value.y));
		WriteU64(bytes.data() + 36, std::bit_cast<uint64>(value.z));
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyMonsterSpawn& value)
	{
		if (bytes.size() != MONSTER_SPAWN_SIZE)
		{
			return false;
		}

		value = {ReadU64(bytes.data()), ReadU64(bytes.data() + 8), ReadU32(bytes.data() + 16),
		         std::bit_cast<double>(ReadU64(bytes.data() + 20)), std::bit_cast<double>(ReadU64(bytes.data() + 28)),
		         std::bit_cast<double>(ReadU64(bytes.data() + 36))};
		return IsValid(value);
	}

	static std::array<BYTE, MONSTER_DESPAWN_SIZE> Encode(const NotifyMonsterDespawn& value)
	{
		std::array<BYTE, MONSTER_DESPAWN_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifyMonsterDespawn& value)
	{
		if (bytes.size() != MONSTER_DESPAWN_SIZE)
		{
			return false;
		}

		value = {ReadU64(bytes.data())};
		return IsValid(value);
	}

	static std::array<BYTE, WELCOME_SIZE> Encode(const ReturnWelcome& value)
	{
		std::array<BYTE, WELCOME_SIZE> bytes{};
		WriteU64(bytes.data(), value.sessionId);
		WriteU16(bytes.data() + 8, value.spawnerCount);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, ReturnWelcome& value)
	{
		if (bytes.size() != WELCOME_SIZE)
		{
			return false;
		}

		value = {ReadU64(bytes.data()), ReadU16(bytes.data() + 8)};
		return IsValid(value);
	}

	static std::array<BYTE, SPAWNER_SIZE> Encode(const NotifySpawner& value)
	{
		std::array<BYTE, SPAWNER_SIZE> bytes{};
		WriteU64(bytes.data(), value.id);
		WriteU64(bytes.data() + 8, std::bit_cast<uint64>(value.x));
		WriteU64(bytes.data() + 16, std::bit_cast<uint64>(value.y));
		WriteU64(bytes.data() + 24, std::bit_cast<uint64>(value.z));
		WriteU16(bytes.data() + 32, value.maxAlive);
		WriteU16(bytes.data() + 34, value.alive);
		WriteU16(bytes.data() + 36, value.pending);
		return bytes;
	}

	static bool Decode(std::span<const BYTE> bytes, NotifySpawner& value)
	{
		if (bytes.size() != SPAWNER_SIZE)
		{
			return false;
		}

		value = {ReadU64(bytes.data()), std::bit_cast<double>(ReadU64(bytes.data() + 8)),
		         std::bit_cast<double>(ReadU64(bytes.data() + 16)), std::bit_cast<double>(ReadU64(bytes.data() + 24)),
		         ReadU16(bytes.data() + 32), ReadU16(bytes.data() + 34), ReadU16(bytes.data() + 36)};
		return IsValid(value);
	}

	static bool IsValid(const RequestPing&) { return true; }

	static bool IsValid(const ReturnPong&) { return true; }

	static std::array<BYTE, 0> Encode(const RequestPing&) { return {}; }

	static std::array<BYTE, 0> Encode(const ReturnPong&) { return {}; }

	static bool Decode(std::span<const BYTE> bytes, RequestPing&) { return bytes.empty(); }

	static bool Decode(std::span<const BYTE> bytes, ReturnPong&) { return bytes.empty(); }

	// Both the IOCP applications and UE use this complete wire frame.
	template <typename PacketType>
	static auto MakePacket(const PacketType& packet)
	{
		constexpr std::size_t payloadSize = std::tuple_size_v<decltype(Encode(packet))>;
		static_assert(HEADER_SIZE + payloadSize <= MAX_PACKET_SIZE);
		using PacketBytes = std::array<BYTE, HEADER_SIZE + payloadSize>;
		if (!IsValid(packet))
		{
			return std::optional<PacketBytes>{};
		}

		PacketBytes bytes{};
		WriteU16(bytes.data(), static_cast<uint16>(bytes.size()));
		WriteU16(bytes.data() + 2, static_cast<uint16>(PacketType::ID));
		const auto payload = Encode(packet);
		for (std::size_t i = 0; i < payload.size(); ++i)
		{
			bytes[HEADER_SIZE + i] = payload[i];
		}

		return std::optional<PacketBytes>{bytes};
	}
};

class WorldSnapshotState
{
public:
	bool Accept(const ReturnWelcome& value)
	{
		if (welcomed || !WorldProtocol::IsValid(value))
		{
			return false;
		}

		welcomed = true;
		expected = value.spawnerCount;
		return true;
	}

	bool Accept(const NotifySpawner& value)
	{
		return welcomed && WorldProtocol::IsValid(value) && ids.size() < expected && ids.insert(value.id).second;
	}

	bool IsWelcomed() const
	{
		return welcomed;
	}

	bool IsComplete() const
	{
		return welcomed && ids.size() == expected;
	}
private:
	bool welcomed{false};
	uint16 expected{};
	std::unordered_set<uint64> ids;
};
