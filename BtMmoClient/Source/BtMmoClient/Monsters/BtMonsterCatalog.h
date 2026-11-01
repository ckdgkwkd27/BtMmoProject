#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BtMonster.h"
#include "BtMonsterCatalog.generated.h"

UCLASS(BlueprintType)
class BTMMOCLIENT_API UBtMonsterCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Monsters")
	TMap<int32, TSubclassOf<ABtMonster>> MonsterClasses;

	TSubclassOf<ABtMonster> FindMonsterClass(uint32 TemplateId) const
	{
		if (TemplateId == 0 || TemplateId > MAX_int32) return nullptr;
		const auto* Class = MonsterClasses.Find(static_cast<int32>(TemplateId));
		if (!Class || !*Class || (*Class)->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) return nullptr;
		return *Class;
	}
};
