#include <Pch.h>
#include "Monster.h"

Monster::Monster(uint64 _id, const MonsterSpawnInfo& info)
    : id(_id), templateId(info.templateId), ownerSpawnerId(info.ownerSpawnerId), position(info.position), maxHp(info.maxHp), hp(info.maxHp)
{
}

uint64 Monster::GetId() const
{
	return id;
}

uint32 Monster::GetTemplateId() const
{
	return templateId;
}

NavPoint Monster::GetPosition() const
{
	return position;
}

uint64 Monster::GetOwnerSpawnerId() const
{
	return ownerSpawnerId;
}

int Monster::GetHp() const
{
	return hp;
}

int Monster::GetMaxHp() const
{
	return maxHp;
}

MonsterState Monster::GetState() const
{
	return state;
}

bool Monster::IsAlive() const
{
	return state == MonsterState::ALIVE;
}

bool Monster::IsSpawned() const
{
	return state != MonsterState::DESPAWNED;
}

bool Monster::SetPosition(NavPoint _position)
{
	if (!IsAlive() || !std::isfinite(_position.x) || !std::isfinite(_position.y) || !std::isfinite(_position.z))
	{
		return false;
	}

	position = _position;
	return true;
}

int Monster::ApplyDamage(int damage)
{
	if (!IsAlive() || damage <= 0)
	{
		return 0;
	}

	const int applied = std::min(damage, hp);
	hp -= applied;
	if (hp == 0)
	{
		state = MonsterState::DEAD;
	}

	return applied;
}

void Monster::MarkDespawned()
{
	state = MonsterState::DESPAWNED;
}
