#pragma once
#include "core/Types.hpp"
#include <array>
#include <string>

class GameContext;

struct ClassDef {
    ClassType type;
    std::string name;
    std::string description;
    std::string talentName;
    std::string talentDesc;
    float hpMult;
    float mpMult;
    float strMult;
    float intMult;
    float armorMult;
    float speedMult;
    float critChanceBonus;
    float critDmgBonus;
    float goldBonus;
    int talentManaCost;
};

struct SpellDef {
    SpellType type;
    std::string name;
    std::string description;
    int manaCost;
    float intMultiplier;
    float cooldown;
};

struct TraitDef {
    TraitType type;
    std::string name;
    std::string description;
    uint32_t conflictMask;
    float goldBonus;
};

struct EquipmentDef {
    EquipSlot slot;
    EquipTier tier;
    std::string name;
    std::string description;
    int cost;
    int weight;
    int hpBonus;
    int mpBonus;
    int strBonus;
    int intBonus;
    int armorBonus;
    float critChanceBonus;
    float critDmgBonus;
};

struct RuneDef {
    RuneType type;
    std::string name;
    std::string description;
    int cost;
};

struct ManorNodeDef {
    ManorNodeId id;
    std::string name;
    std::string description;
    int gridX;
    int gridY;
    ManorNodeId parentNodeId;
    int maxLevel;
    int baseCost;
    int costStep;
    int visualPartId;
    float bonusPerLevel;
};

class Registry {
public:
    static const ClassDef& GetClass(ClassType type);
    static const SpellDef& GetSpell(SpellType type);
    static const TraitDef& GetTrait(TraitType type);
    static const EquipmentDef& GetEquipment(EquipSlot slot, EquipTier tier);
    static const RuneDef& GetRune(RuneType type);
    static const ManorNodeDef& GetManorNode(ManorNodeId id);

    static int GetNodeCost(const ManorNodeDef& node, int currentLvl, int totalManorLevels);
    static int GetNodeCostById(ManorNodeId id, int currentLvl, int totalManorLevels);

    static std::string GetBiomeName(BiomeType biome);
    static std::string GetEquipSlotName(EquipSlot slot);
    static std::string GetEquipTierName(EquipTier tier);
    static std::string GetFairyConditionDesc(FairyCondition cond);
    static std::string GetBossName(BossType boss);
    static std::string GetBossTitle(BossType boss);

    static HeirCandidate GenerateHeir(const GameContext& ctx, int generationNumber);
};
