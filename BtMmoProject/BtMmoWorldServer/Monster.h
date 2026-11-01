#pragma once
#include "NavigationMesh.h"
#include <Types.h>

struct MonsterSpawnInfo
{
	uint32 templateId{};
	NavPoint position{};
	int maxHp{100};
	uint64 ownerSpawnerId{};
};

enum class MonsterState
{
	ALIVE,
	DEAD,
	DESPAWNED
};

class Monster final
{
public:
	Monster(const Monster&) = delete;
	Monster& operator=(const Monster&) = delete;
	uint64 GetId() const;
	uint32 GetTemplateId() const;
	uint64 GetOwnerSpawnerId() const;
	NavPoint GetPosition() const;
	int GetHp() const;
	int GetMaxHp() const;
	MonsterState GetState() const;
	bool IsAlive() const;
	bool IsSpawned() const;
	bool SetPosition(NavPoint _position);
	int ApplyDamage(int damage);
private:
	friend class MonsterSpawner;
	friend class MonsterDespawner;
	Monster(uint64 _id, const MonsterSpawnInfo& info);
	void MarkDespawned();
	uint64 id;
	uint32 templateId;
	uint64 ownerSpawnerId;
	NavPoint position;
	int maxHp;
	int hp;
	MonsterState state{MonsterState::ALIVE};
};

using MonsterPtr = std::shared_ptr<Monster>;
