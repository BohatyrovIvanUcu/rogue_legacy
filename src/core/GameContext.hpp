#pragma once
#include "core/Config.hpp"
#include "core/Types.hpp"
#include "core/InputConfig.hpp"
#include "data/Registry.hpp"
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

class SoundSynth;
class ShaderManager;
class ParticleSystem;
class ProceduralRenderer;

class GameContext {
public:
    SoundSynth* audio = nullptr;
    ShaderManager* shaders = nullptr;
    ParticleSystem* particles = nullptr;
    ProceduralRenderer* renderer = nullptr;

    int gold = 0;
    int ngPlusLevel = 0;
    int generationCount = 1;
    bool musicMuted = false;
    bool sfxMuted = false;
    float musicVolume = 1.0f;
    float sfxVolume = 1.0f;
    bool showFps = false;
    int targetFps = 60;
    bool vsync = true;
    int displayMode = 0; // 0 = Fullscreen, 1 = Borderless, 2 = Windowed
    InputConfig input{};

    std::array<int, static_cast<size_t>(ManorNodeId::COUNT)> manorLevels{};

    std::array<std::array<bool, 5>, 5> unlockedBlueprints{};
    std::array<std::array<bool, 5>, 5> ownedEquipment{};
    std::array<int, 5> equippedTier{};

    std::array<std::array<bool, 8>, 5> unlockedRunes{};
    std::array<std::array<bool, 8>, 5> ownedRunes{};
    std::array<RuneType, 5> equippedRunes{};

    std::array<bool, 5> bossDefeated{};

    bool isCastleLocked = false;
    bool hasSavedCastle = false;
    std::array<std::array<SavedRoomNode, Config::CASTLE_GRID_SIZE>, Config::CASTLE_GRID_SIZE> savedCastle{};

    HeirCandidate activeHeir{};
    std::array<HeirCandidate, 3> heirCandidates{};
    int lichBonusMaxHp = 0;

    int runKills = 0;
    int runGoldCollected = 0;
    int charonTollPaid = 0;
    std::string lastKillerName = "Skeleton";
    bool lastRunVictory = false;
    std::array<int, static_cast<size_t>(EnemyType::COUNT)> runSlainByType{};
    std::vector<SlainRecord> lineageHistory{};

    GameStateId currentStateId = GameStateId::Title;
    GameStateId requestedState = GameStateId::Title;
    bool hasRequestedState = false;
    bool shouldQuit = false;

    // Phase 7 Game Feel: Trauma screen shake and Hit-stop
    float screenTrauma = 0.0f;
    int hitStopFrames = 0;

    void AddTrauma(float t) {
        screenTrauma = std::clamp(screenTrauma + t, 0.0f, 1.0f);
    }

    void RequestHitStop(int frames = 3) {
        hitStopFrames = std::max(hitStopFrames, frames);
    }

public:
    GameContext() {
        ResetToDefaultNewGame();
    }

    void ResetToDefaultNewGame() {
        gold = 150;
        ngPlusLevel = 0;
        generationCount = 1;
        lichBonusMaxHp = 0;

        manorLevels.fill(0);

        for (size_t s = 0; s < 5; ++s) {
            unlockedBlueprints[s].fill(false);
            ownedEquipment[s].fill(false);
            unlockedBlueprints[s][0] = true;
            equippedTier[s] = -1;

            unlockedRunes[s].fill(false);
            ownedRunes[s].fill(false);
            unlockedRunes[s][static_cast<size_t>(RuneType::Vault)] = true;
            unlockedRunes[s][static_cast<size_t>(RuneType::Sprint)] = true;
            equippedRunes[s] = RuneType::None;
        }

        ownedEquipment[static_cast<size_t>(EquipSlot::Sword)][0] = true;
        equippedTier[static_cast<size_t>(EquipSlot::Sword)] = 0;

        bossDefeated.fill(false);
        isCastleLocked = false;
        hasSavedCastle = false;

        for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
            for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
                savedCastle[static_cast<size_t>(y)][static_cast<size_t>(x)] = SavedRoomNode{};
            }
        }

        runKills = 0;
        runGoldCollected = 0;
        charonTollPaid = 0;
        lastKillerName = "Castle Guardians";
        lastRunVictory = false;
        runSlainByType.fill(0);
        lineageHistory.clear();

        activeHeir.name = "Sir Johannes I";
        activeHeir.classType = ClassType::Knights_Base;
        activeHeir.spell = SpellType::Dagger;
        activeHeir.traits.clear();
        activeHeir.generation = 1;
        activeHeir.isFemale = false;
        activeHeir.visualSeed = 777;

        GenerateNewHeirTrio();
    }

    void RequestStateChange(GameStateId nextState) {
        requestedState = nextState;
        hasRequestedState = true;
    }

    int GetManorLevel(ManorNodeId id) const {
        if (id == ManorNodeId::None || id == ManorNodeId::COUNT) return 0;
        return manorLevels[static_cast<size_t>(id)];
    }

    int GetTotalManorLevels() const {
        int sum = 0;
        for (int lvl : manorLevels) {
            sum += lvl;
        }
        return sum;
    }

    ClassType ResolveClassWithUpgrade(ClassType baseType) const {
        bool upgraded = (GetManorLevel(ManorNodeId::Upgrade_AllBasicClasses) > 0);
        if (!upgraded) return baseType;

        switch (baseType) {
            case ClassType::Knights_Base:    return ClassType::Paladin_Upgraded;
            case ClassType::Barbarian_Base:  return ClassType::BarbarianKing_Upgraded;
            case ClassType::Mage_Base:       return ClassType::Archmage_Upgraded;
            case ClassType::Knave_Base:      return ClassType::Assassin_Upgraded;
            case ClassType::Shinobi_Base:    return ClassType::Hokage_Upgraded;
            case ClassType::Miner_Base:      return ClassType::Spelunker_Upgraded;
            case ClassType::Lich_Base:       return ClassType::LichKing_Upgraded;
            case ClassType::Spellthief_Base: return ClassType::Spellsword_Upgraded;
            default:                         return baseType;
        }
    }

    void GenerateNewHeirTrio() {
        for (size_t i = 0; i < 3; ++i) {
            heirCandidates[i] = Registry::GenerateHeir(*this, generationCount + 1);
            heirCandidates[i].visualSeed += static_cast<uint32_t>(i * 997);
        }
    }

    bool CurrentHeirHasTrait(TraitType t) const {
        for (TraitType trait : activeHeir.traits) {
            if (trait == t) return true;
        }
        return false;
    }

    int CountEquippedRune(RuneType r) const {
        int count = 0;
        for (RuneType eq : equippedRunes) {
            if (eq == r) ++count;
        }
        return count;
    }

    int GetCurrentEquipWeight() const {
        int weight = 0;
        for (size_t slot = 0; slot < 5; ++slot) {
            int tier = equippedTier[slot];
            if (tier >= 0 && tier < 5) {
                weight += Registry::GetEquipment(static_cast<EquipSlot>(slot), static_cast<EquipTier>(tier)).weight;
            }
        }
        return weight;
    }

    int GetMaxEquipWeight() const {
        return Config::BASE_EQUIP_WEIGHT + GetManorLevel(ManorNodeId::Stat_EquipWeight) * 8;
    }

    CalculatedStats ComputePlayerStats() const {
        CalculatedStats st;
        const ClassDef& cls = Registry::GetClass(activeHeir.classType);

        int rawHp = Config::BASE_PLAYER_HP + GetManorLevel(ManorNodeId::Stat_Health) * 12;
        int rawMp = Config::BASE_PLAYER_MP + GetManorLevel(ManorNodeId::Stat_Mana) * 8;
        int rawStr = Config::BASE_PLAYER_STR + GetManorLevel(ManorNodeId::Stat_Attack) * 3;
        int rawInt = Config::BASE_PLAYER_INT + GetManorLevel(ManorNodeId::Stat_MagicDmg) * 4;
        int rawArmor = Config::BASE_PLAYER_ARMOR + GetManorLevel(ManorNodeId::Stat_Armor) * 3;

        float rawCritChance = Config::BASE_CRIT_CHANCE + GetManorLevel(ManorNodeId::Stat_CritChance) * 0.02f;
        float rawCritDmg = Config::BASE_CRIT_DMG + GetManorLevel(ManorNodeId::Stat_CritDmg) * 0.10f;

        st.currentWeight = 0;
        st.maxWeight = GetMaxEquipWeight();

        for (size_t slot = 0; slot < 5; ++slot) {
            int tier = equippedTier[slot];
            if (tier >= 0 && tier < 5) {
                const EquipmentDef& eq = Registry::GetEquipment(static_cast<EquipSlot>(slot), static_cast<EquipTier>(tier));
                st.currentWeight += eq.weight;
                rawHp += eq.hpBonus;
                rawMp += eq.mpBonus;
                rawStr += eq.strBonus;
                rawInt += eq.intBonus;
                rawArmor += eq.armorBonus;
                rawCritChance += eq.critChanceBonus;
                rawCritDmg += eq.critDmgBonus;
            }
        }

        st.maxHp = std::max(10, static_cast<int>(std::round(rawHp * cls.hpMult)) + lichBonusMaxHp);
        st.maxMp = std::max(10, static_cast<int>(std::round(rawMp * cls.mpMult)));
        st.str = std::max(1, static_cast<int>(std::round(rawStr * cls.strMult)));
        st.intelligence = std::max(1, static_cast<int>(std::round(rawInt * cls.intMult)));
        st.armor = std::max(0, static_cast<int>(std::round(rawArmor * cls.armorMult)));

        st.critChance = std::clamp(rawCritChance + cls.critChanceBonus, 0.0f, 1.0f);
        st.critDmg = std::max(1.1f, rawCritDmg + cls.critDmgBonus);

        st.extraAirJumps = CountEquippedRune(RuneType::Vault);
        st.maxAirDashes = CountEquippedRune(RuneType::Sprint);
        st.vampireHealPerKill = CountEquippedRune(RuneType::Vampire) * 4;
        st.siphonManaPerKill = CountEquippedRune(RuneType::Siphon) * 4;
        st.retaliationFactor = CountEquippedRune(RuneType::Retaliation) * 0.60f;
        st.graceRuneCount = CountEquippedRune(RuneType::Grace);

        int bountyRunes = CountEquippedRune(RuneType::Bounty);
        int hasteRunes = CountEquippedRune(RuneType::Haste);

        float speedMult = cls.speedMult * (1.0f + hasteRunes * 0.12f);
        if (CurrentHeirHasTrait(TraitType::ADHD)) {
            speedMult *= 1.35f;
        }
        st.moveSpeed = Config::PLAYER_BASE_SPEED * speedMult;

        float goldMult = 1.0f + GetManorLevel(ManorNodeId::Stat_GoldGain) * 0.10f + cls.goldBonus + bountyRunes * 0.15f + (ngPlusLevel * 0.50f);
        for (TraitType t : activeHeir.traits) {
            goldMult += Registry::GetTrait(t).goldBonus;
        }
        if (isCastleLocked) {
            goldMult *= Config::ARCHITECT_GOLD_MULTIPLIER;
        }
        st.goldGainMult = goldMult;

        st.hagglingKeptRatio = std::clamp(GetManorLevel(ManorNodeId::Stat_Haggling) * 0.10f, 0.0f, 0.50f);
        st.potionEfficacyMult = 1.0f + GetManorLevel(ManorNodeId::Stat_PotionEfficacy) * 0.08f;
        st.iframeDuration = Config::BASE_IFRAME_DURATION + GetManorLevel(ManorNodeId::Stat_InvulnTime) * 0.10f;
        st.downStrikeDmgMult = 1.0f + GetManorLevel(ManorNodeId::Stat_DownStrikeBonus) * 0.12f;

        st.scaleX = 1.0f;
        st.scaleY = 1.0f;
        st.knockbackTakenMult = 1.0f;

        if (CurrentHeirHasTrait(TraitType::Dwarfism)) {
            st.scaleX *= 0.48f;
            st.scaleY *= 0.48f;
        } else if (CurrentHeirHasTrait(TraitType::Gigantism)) {
            st.scaleX *= 1.55f;
            st.scaleY *= 1.55f;
        }

        if (CurrentHeirHasTrait(TraitType::Ectomorph)) {
            st.scaleX *= 0.60f;
            st.knockbackTakenMult = 2.0f;
        } else if (CurrentHeirHasTrait(TraitType::Endomorph)) {
            st.scaleX *= 1.45f;
            st.knockbackTakenMult = 0.0f;
        }

        if (CurrentHeirHasTrait(TraitType::Dextrocardia)) {
            std::swap(st.maxHp, st.maxMp);
        }

        return st;
    }
};
