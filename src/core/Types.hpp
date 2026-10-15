#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <raylib.h>

enum class ClassType : int {
    Knights_Base = 0,
    Paladin_Upgraded = 1,
    Barbarian_Base = 2,
    BarbarianKing_Upgraded = 3,
    Mage_Base = 4,
    Archmage_Upgraded = 5,
    Knave_Base = 6,
    Assassin_Upgraded = 7,
    Shinobi_Base = 8,
    Hokage_Upgraded = 9,
    Miner_Base = 10,
    Spelunker_Upgraded = 11,
    Lich_Base = 12,
    LichKing_Upgraded = 13,
    Spellthief_Base = 14,
    Spellsword_Upgraded = 15,
    Dragon_Special = 16,
    Traitor_Special = 17,
    COUNT = 18
};

enum class SpellType : int {
    Dagger = 0,
    Axe = 1,
    Chakram = 2,
    Scythe = 3,
    FlameBarrier = 4,
    Conflux = 5,
    CrowStorm = 6,
    TimeStop = 7,
    COUNT = 8
};

enum class TraitType : int {
    Colorblind = 0,     // 1. Color Blind
    Gay = 1,            // 2. Gay
    NearSighted = 2,    // 3. Near-Sighted
    FarSighted = 3,     // 4. Far-Sighted
    Dyslexia = 4,       // 5. Dyslexia
    Gigantism = 5,      // 6. Gigantism
    Dwarfism = 6,       // 7. Dwarfism
    Baldness = 7,       // 8. Baldness
    Endomorph = 8,      // 9. Endomorph
    Ectomorph = 9,      // 10. Ectomorph
    Alzheimers = 10,    // 11. Alzheimers
    Dextrocardia = 11,  // 12. Dextrocardia
    Coprolalia = 12,    // 13. Coprolalia / Tourettes
    ADHD = 13,          // 14. ADHD / Hyperactive
    OCD = 14,           // 15. O.C.D.
    Hypergonadism = 15, // 16. Hypergonadism
    MuscleWk = 16,      // 17. Muscle Wk. / Hypogonadism
    StereoBlind = 17,   // 18. Stereo Blind
    IBS = 18,           // 19. I.B.S.
    Vertigo = 19,       // 20. Vertigo
    TunnelVision = 20,  // 21. Tunnel Vision
    Ambilevous = 21,    // 22. Ambilevous
    PAD = 22,           // 23. P.A.D.
    Alektorophobia = 23,// 24. Alektorophobia (Kentucky Chicken)
    Hypochondriac = 24, // 25. Hypochondriac
    Dementia = 25,      // 26. Dementia
    Flexible = 26,      // 27. Flexible / Hypermobility
    EideticMemory = 27, // 28. Eid. Mem.
    Nostalgic = 28,     // 29. Nostalgic
    CIP = 29,           // 30. C.I.P.
    Savant = 30,        // 31. Savant
    TheOne = 31,        // 32. The One
    Clumsy = 32,        // 33. Clumsy
    EHS = 33,           // 34. E.H.S.
    Glaucoma = 34,      // 35. Glaucoma
    Clonus = 35,        // 36. Clonus
    Prosopagnosia = 36, // 37. Prosopagnosia
    COUNT = 37,

    // Aliases for backward compatibility:
    Myopia = NearSighted,
    Hyperopia = FarSighted
};

enum class EquipSlot : int {
    Sword = 0,
    Helm = 1,
    Chest = 2,
    Limbs = 3,
    Cape = 4,
    COUNT = 5
};

enum class EquipTier : int {
    None = -1,
    Squire = 0,
    Silver = 1,
    Imperial = 2,
    Royal = 3,
    Dark = 4,
    COUNT = 5
};

enum class RuneType : int {
    None = -1,
    Vault = 0,
    Sprint = 1,
    Vampire = 2,
    Siphon = 3,
    Retaliation = 4,
    Bounty = 5,
    Haste = 6,
    Grace = 7,
    COUNT = 8
};

enum class BiomeType : int {
    CastleHamson = 0,
    ForestAbkhazia = 1,
    TheMaya = 2,
    LandOfDarkness = 3,
    COUNT = 4
};

enum class RoomType : int {
    Entrance = 0,
    Combat = 1,
    FairyChest = 2,
    Treasure = 3,
    Teleporter = 4,
    BossEntrance = 5,
    FinalBossRoom = 6
};

enum class FairyCondition : int {
    NoDamage = 0,
    TimeLimit = 1,
    KillAll = 2,
    NoJumping = 3
};

enum class ManorNodeId : int {
    None = -1,
    Unlock_Blacksmith = 0,
    Unlock_Enchantress = 1,
    Unlock_Architect = 2,
    Stat_Health = 3,
    Stat_Mana = 4,
    Stat_Attack = 5,
    Stat_MagicDmg = 6,
    Stat_Armor = 7,
    Stat_EquipWeight = 8,
    Stat_CritChance = 9,
    Stat_CritDmg = 10,
    Stat_GoldGain = 11,
    Stat_Haggling = 12,
    Stat_PotionEfficacy = 13,
    Stat_InvulnTime = 14,
    Class_Barbarian = 15,
    Class_Mage = 16,
    Class_Knave = 17,
    Class_Shinobi = 18,
    Class_Miner = 19,
    Class_Lich = 20,
    Class_Dragon = 21,
    Upgrade_AllBasicClasses = 22,
    Stat_DownStrikeBonus = 23,
    COUNT = 24
};

enum class EnemyType : int {
    Skeleton = 0,
    FloatingEye = 1,
    ShieldKnight = 2,
    FireMage = 3,
    Warg = 4,
    WallTurret = 5,
    COUNT = 6
};

enum class BossType : int {
    Khidr = 0,
    Alexander = 1,
    PonceDeLeon = 2,
    Herodotus = 3,
    JohannesTraitor = 4,
    COUNT = 5
};

enum class SfxId : int {
    SwordSlash = 0,
    HitEnemy = 1,
    HitShield = 2,
    PogoBounce = 3,
    Jump = 4,
    Dash = 5,
    CoinPickup = 6,
    ChestOpen = 7,
    FairyFail = 8,
    SpellCast = 9,
    PlayerHurt = 10,
    PlayerDeath = 11,
    ManorBuild = 12,
    BossRoar = 13,
    COUNT = 14
};

enum class GameStateId : int {
    Title = 0,
    Lineage = 1,
    Manor = 2,
    Outskirts = 3,
    CastlePlay = 4,
    DeathSummary = 5
};

namespace DoorMask {
    inline constexpr uint8_t NONE  = 0;
    inline constexpr uint8_t NORTH = 1 << 0;
    inline constexpr uint8_t EAST  = 1 << 1;
    inline constexpr uint8_t SOUTH = 1 << 2;
    inline constexpr uint8_t WEST  = 1 << 3;
}

namespace TraitConflictGroup {
    inline constexpr uint32_t NONE          = 0;
    inline constexpr uint32_t SHADER_TONE   = 1 << 0; // Colorblind, Nostalgic, TheOne
    inline constexpr uint32_t BLUR_VISION   = 1 << 1; // NearSighted, FarSighted
    inline constexpr uint32_t BODY_SCALE    = 1 << 2; // Gigantism, Dwarfism
    inline constexpr uint32_t BODY_WIDTH    = 1 << 3; // Endomorph, Ectomorph
    inline constexpr uint32_t KNOCKBACK_MOD = 1 << 4; // Hypergonadism, MuscleWk
}

struct HeirCandidate {
    std::string name = "Sir Johannes I";
    ClassType classType = ClassType::Knights_Base;
    SpellType spell = SpellType::Dagger;
    std::vector<TraitType> traits{};
    bool isFemale = false;
    int generation = 1;
    uint32_t visualSeed = 101;
};

struct CalculatedStats {
    int maxHp = 100;
    int maxMp = 50;
    int str = 18;
    int intelligence = 20;
    int armor = 0;
    int currentWeight = 0;
    int maxWeight = 25;

    float moveSpeed = 290.0f;
    float critChance = 0.05f;
    float critDmg = 1.50f;
    float goldGainMult = 1.00f;
    float hagglingKeptRatio = 0.00f;
    float potionEfficacyMult = 1.00f;
    float iframeDuration = 0.85f;
    float downStrikeDmgMult = 1.00f;

    int extraAirJumps = 0;
    int maxAirDashes = 0;
    int vampireHealPerKill = 0;
    int siphonManaPerKill = 0;
    float retaliationFactor = 0.0f;
    int graceRuneCount = 0;

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float knockbackTakenMult = 1.0f;
};

struct SavedRoomNode {
    bool active = false;
    RoomType roomType = RoomType::Combat;
    BiomeType biome = BiomeType::CastleHamson;
    uint8_t doorMask = 0;
    int templateIdx = 0;
    FairyCondition fairyCond = FairyCondition::NoDamage;
    BossType bossType = BossType::Khidr;
    bool visited = false;
    bool cleared = false;
    bool chestOpened = false;
};

struct SlainRecord {
    std::string heroName = "Sir Lee I";
    ClassType heroClass = ClassType::Knights_Base;
    std::string killerName = "Skeleton";
    int goldCollected = 0;
    int enemiesSlain = 0;
    bool wasVictory = false;
};
