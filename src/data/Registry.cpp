#include "data/Registry.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include <algorithm>
#include <random>

namespace {

    const std::array<ClassDef, static_cast<size_t>(ClassType::COUNT)> CLASS_TABLE = {{
        {
            ClassType::Knights_Base,
            "Knight",
            "Standard balanced warrior of the royal guard.",
            "Guardian Shield",
            "Hold [Q] to raise a frontal shield that blocks all incoming attacks (15 MP per block).",
            1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 0.00f, 0.00f, 0.00f, 15
        },
        {
            ClassType::Paladin_Upgraded,
            "Paladin",
            "Holy champion blessed with divine lightning.",
            "Holy Guardian Shield",
            "Hold [Q] to block attacks and strike nearby enemies with divine lightning (1.5x INT).",
            1.10f, 1.10f, 1.05f, 1.15f, 1.15f, 1.00f, 0.00f, 0.00f, 0.00f, 15
        },
        {
            ClassType::Barbarian_Base,
            "Barbarian",
            "Towering northern tank with immense vitality.",
            "Barbarian Shout",
            "Press [Q] (25 MP) to emit a radial shockwave destroying projectiles and stunning foes.",
            1.50f, 0.60f, 0.75f, 0.50f, 1.20f, 0.96f, 0.00f, 0.00f, 0.00f, 25
        },
        {
            ClassType::BarbarianKing_Upgraded,
            "Barbarian King",
            "Unstoppable warlord whose roar shakes the castle walls.",
            "Royal Warcry",
            "Press [Q] (25 MP) to unleash a massive damaging shockwave that vaporizes projectiles.",
            1.65f, 0.60f, 0.85f, 0.50f, 1.25f, 1.00f, 0.00f, 0.00f, 0.00f, 25
        },
        {
            ClassType::Mage_Base,
            "Mage",
            "Master of arcane arts. Restores +6 MP on every enemy kill.",
            "Arcane Focus",
            "Press [Q] (0 MP) to channel an arcane pulse that restores 8 MP on long cooldown.",
            0.60f, 1.50f, 0.60f, 1.45f, 0.80f, 1.00f, 0.00f, 0.00f, 0.00f, 0
        },
        {
            ClassType::Archmage_Upgraded,
            "Archmage",
            "Wields 3 spells simultaneously. Restores +6 MP per kill.",
            "Spell Cycle",
            "Press [Q] (0 MP) to cycle between 3 prepared arcane spells at will.",
            0.65f, 1.70f, 0.65f, 1.60f, 0.85f, 1.00f, 0.00f, 0.00f, 0.00f, 0
        },
        {
            ClassType::Knave_Base,
            "Knave",
            "Lethal rogue specializing in devastating critical strikes.",
            "Smoke Bomb",
            "Press [Q] (14 MP/s) to enter Mist Form, becoming intangible to enemies and spikes.",
            0.75f, 0.80f, 0.85f, 0.80f, 0.85f, 1.05f, 0.20f, 1.25f, 0.00f, 14
        },
        {
            ClassType::Assassin_Upgraded,
            "Assassin",
            "Phantom executioner with supreme critical lethality.",
            "Mist Form",
            "Press [Q] (14 MP/s) to dissolve into invulnerable shadow mist.",
            0.80f, 0.85f, 0.90f, 0.85f, 0.90f, 1.08f, 0.28f, 1.60f, 0.00f, 14
        },
        {
            ClassType::Shinobi_Base,
            "Shinobi",
            "Blazingly fast ninja with massive physical attack, but cannot land Critical Hits.",
            "Replacement Jutsu",
            "Press [Q] (10 MP) to blink 190px forward through walls and foes, leaving a decoy log.",
            0.65f, 0.70f, 1.65f, 0.75f, 0.70f, 1.30f, -10.0f, 0.00f, 0.00f, 10
        },
        {
            ClassType::Hokage_Upgraded,
            "Hokage",
            "Grandmaster ninja with overwhelming blade strength and instant substitution.",
            "Replacement Jutsu",
            "Press [Q] (10 MP) to teleport 190px forward and drop an explosive decoy log.",
            0.70f, 0.75f, 1.80f, 0.80f, 0.75f, 1.32f, -10.0f, 0.00f, 0.00f, 10
        },
        {
            ClassType::Miner_Base,
            "Miner",
            "Frail treasure hunter who earns +40% bonus gold from all sources.",
            "Headlamp",
            "Press [Q] (0 MP) to toggle a bright headlamp cone that pierces darkness.",
            0.50f, 0.60f, 0.55f, 0.50f, 0.70f, 1.02f, 0.00f, 0.00f, 0.40f, 0
        },
        {
            ClassType::Spelunker_Upgraded,
            "Spelunker",
            "Master explorer earning +60% bonus gold and revealing all chests on the minimap.",
            "Spelunker Headlamp",
            "Press [Q] (0 MP) to toggle headlamp and reveal all treasure & fairy rooms on map.",
            0.55f, 0.65f, 0.60f, 0.55f, 0.75f, 1.05f, 0.00f, 0.00f, 0.60f, 0
        },
        {
            ClassType::Lich_Base,
            "Lich",
            "Undead sorcerer. Every enemy kill permanently adds +5 MaxHP (Soul Feast).",
            "HP Conversion",
            "Press [Q] (0 MP) to sacrifice 30% of current MaxHP and restore 100% MP.",
            0.40f, 0.50f, 0.70f, 1.40f, 0.80f, 1.00f, 0.00f, 0.00f, 0.00f, 0
        },
        {
            ClassType::LichKing_Upgraded,
            "Lich King",
            "Immortal lord of souls. Gains +8 MaxHP per kill up to 2.5x normal health.",
            "HP Conversion",
            "Press [Q] (0 MP) to convert 30% of MaxHP into full mana and release dark bolts.",
            0.45f, 0.65f, 0.75f, 1.55f, 0.85f, 1.00f, 0.00f, 0.00f, 0.00f, 0
        },
        {
            ClassType::Spellthief_Base,
            "Spellthief",
            "Hybrid duelist whose sword hits restore MP equal to 30% of damage dealt.",
            "Empowered Spell",
            "Press [Q] (2x Spell MP) to cast a colossal version of your spell dealing 2.5x damage.",
            0.80f, 0.80f, 0.85f, 1.15f, 0.90f, 1.02f, 0.00f, 0.00f, 0.00f, 20
        },
        {
            ClassType::Spellsword_Upgraded,
            "Spellsword",
            "Arcane blade master siphoning 35% of melee damage as mana.",
            "Empowered Spell",
            "Press [Q] (2x Spell MP) to unleash a giant 2.5x damage empowered spell.",
            0.85f, 0.85f, 0.90f, 1.25f, 0.95f, 1.04f, 0.05f, 0.15f, 0.00f, 20
        },
        {
            ClassType::Dragon_Special,
            "Dragon",
            "Man-beast with wings! Free 8-directional flight and regenerating mana (35/s).",
            "Dragon Flight & Fireball",
            "Attack fires aimed fireballs (12 MP). Press [Q] for a high-speed aerial wing burst.",
            0.80f, 1.25f, 1.00f, 1.35f, 1.10f, 1.10f, 0.00f, 0.00f, 0.00f, 15
        },
        {
            ClassType::Traitor_Special,
            "Traitor",
            "Avatar of Johannes wielding dark fountain relics.",
            "Fountain Surge",
            "Press [Q] (20 MP) to summon four ancestral blades from the sky.",
            0.90f, 1.20f, 1.15f, 1.25f, 1.00f, 1.15f, 0.10f, 0.30f, 0.00f, 20
        }
    }};

    const std::array<SpellDef, static_cast<size_t>(SpellType::COUNT)> SPELL_TABLE = {{
        { SpellType::Dagger, "Dagger", "Throws a fast straight-flying throwing knife horizontally.", 10, 1.15f, 0.25f },
        { SpellType::Axe, "Axe", "Lobs a heavy battle axe in a high parabolic arc, cleaving through foes.", 15, 1.55f, 0.40f },
        { SpellType::Chakram, "Chakram", "Hurls a spinning chakram that travels forward and boomerangs back to you.", 15, 1.35f, 0.50f },
        { SpellType::Scythe, "Scythe", "Launches a deathly scythe diagonally upward at a 45-degree angle.", 15, 1.45f, 0.38f },
        { SpellType::FlameBarrier, "Flame Barrier", "Summons 2 fiery orbs that orbit around your body, burning enemies on contact.", 25, 0.95f, 1.20f },
        { SpellType::Conflux, "Conflux", "Releases 4 bouncing magic spheres that ricochet off walls and floors.", 20, 1.05f, 0.55f },
        { SpellType::CrowStorm, "Crow Storm", "Calls a murder of spectral crows that home in on all enemies in the room.", 30, 1.85f, 0.90f },
        { SpellType::TimeStop, "Time Stop", "Freezes all enemies and enemy projectiles in the room for 3.5 seconds.", 35, 0.00f, 4.00f }
    }};

    const std::array<TraitDef, static_cast<size_t>(TraitType::COUNT)> TRAIT_TABLE = {{
        { TraitType::Colorblind, "Colorblind", "You can't see colors due to monochromacy.", TraitConflictGroup::SHADER_TONE, 0.00f },
        { TraitType::Nostalgic, "Nostalgic", "You miss the good old days. Everything looks like a vintage sepia film.", TraitConflictGroup::SHADER_TONE, 0.00f },
        { TraitType::Vertigo, "Vertigo", "Welcome to Barrels! The entire world is upside-down (+15% Gold Bonus).", TraitConflictGroup::NONE, 0.15f },
        { TraitType::Myopia, "Near-Sighted", "Anything farther than 210 pixels from you is blurry.", TraitConflictGroup::BLUR_VISION, 0.00f },
        { TraitType::Hyperopia, "Far-Sighted", "Anything closer than 135 pixels to you is blurry.", TraitConflictGroup::BLUR_VISION, 0.00f },
        { TraitType::Dwarfism, "Dwarfism", "You are tiny (0.48x scale) and can squeeze through narrow 1-tile Dwarf Tunnels.", TraitConflictGroup::BODY_SCALE, 0.00f },
        { TraitType::Gigantism, "Gigantism", "You are huge (1.55x scale) with a massive sword reach and hitbox.", TraitConflictGroup::BODY_SCALE, 0.00f },
        { TraitType::ADHD, "ADHD", "So much energy! You move and animate 35% faster than normal.", TraitConflictGroup::NONE, 0.00f },
        { TraitType::Ectomorph, "Ectomorph", "You are super skinny (0.6x width). Hits send you flying (2.0x knockback).", TraitConflictGroup::BODY_WIDTH, 0.00f },
        { TraitType::Endomorph, "Endomorph", "You are heavy-set (1.45x width). Enemies cannot knock you back at all.", TraitConflictGroup::BODY_WIDTH, 0.00f },
        { TraitType::CIP, "C.I.P.", "Congenital Insensitivity to Pain. No health bar values are visible [??? / ???].", TraitConflictGroup::NONE, 0.00f },
        { TraitType::Dextrocardia, "Dextrocardia", "Your Max HP and Max MP pools are swapped with each other.", TraitConflictGroup::NONE, 0.00f },
        { TraitType::PAD, "P.A.D.", "Peripheral Artery Disease. Floor pressure spike traps do not trigger when stepped on.", TraitConflictGroup::NONE, 0.00f },
        { TraitType::Dementia, "Dementia", "You are insane. Hallucinatory purple phantom enemies haunt every room.", TraitConflictGroup::NONE, 0.00f },
        { TraitType::Savant, "Savant", "Extremely talented, but randomized. Every spell cast fires a random spell.", TraitConflictGroup::NONE, 0.00f },
        { TraitType::IBS, "I.B.S.", "Irritable Bowel Syndrome. You uncontrollably toot gas clouds when jumping.", TraitConflictGroup::NONE, 0.00f }
    }};

    const std::array<std::array<EquipmentDef, 5>, 5> EQUIPMENT_TABLE = {{
        // SLOT 0: SWORD
        {{
            { EquipSlot::Sword, EquipTier::Squire,   "Squire Sword",   "Simple iron broadsword.",           250,  5,  0,  0,  8,  0,  0, 0.00f, 0.00f },
            { EquipSlot::Sword, EquipTier::Silver,   "Silver Sword",   "Alchemically forged silver blade.", 650,  10, 0,  5,  18, 8,  0, 0.02f, 0.05f },
            { EquipSlot::Sword, EquipTier::Imperial, "Imperial Sword", "Heavy gilded legionnaire blade.",   1400, 16, 10, 10, 30, 14, 2, 0.04f, 0.10f },
            { EquipSlot::Sword, EquipTier::Royal,    "Royal Sword",    "Masterwork blade of kings.",        2800, 24, 20, 15, 48, 22, 5, 0.06f, 0.20f },
            { EquipSlot::Sword, EquipTier::Dark,     "Dark Sword",     "Abyssal obsidian greatsword.",      5200, 32, 35, 25, 76, 35, 10, 0.10f, 0.45f }
        }},
        // SLOT 1: HELM
        {{
            { EquipSlot::Helm, EquipTier::Squire,   "Squire Helm",   "Sturdy steel nasal helmet.",        250,  5,  15,  0,  0,  0,  4, 0.00f, 0.00f },
            { EquipSlot::Helm, EquipTier::Silver,   "Silver Helm",   "Polished silver visored bascinet.", 650,  10, 33,  10, 2,  6,  9, 0.00f, 0.00f },
            { EquipSlot::Helm, EquipTier::Imperial, "Imperial Helm", "Plumed imperial armet.",            1400, 16, 57,  20, 5,  12, 15, 0.02f, 0.05f },
            { EquipSlot::Helm, EquipTier::Royal,    "Royal Helm",    "Crowned greathelm of Hamson.",      2800, 24, 90,  35, 10, 20, 24, 0.04f, 0.15f },
            { EquipSlot::Helm, EquipTier::Dark,     "Dark Helm",     "Horned dread-helm of the void.",    5200, 32, 142, 55, 18, 32, 38, 0.06f, 0.30f }
        }},
        // SLOT 2: CHEST
        {{
            { EquipSlot::Chest, EquipTier::Squire,   "Squire Cuirass",   "Brigandine over chainmail.",         250,  5,  25,  0,  0,  0,  8,  0.00f, 0.00f },
            { EquipSlot::Chest, EquipTier::Silver,   "Silver Cuirass",   "Fluted silver breastplate.",         650,  10, 55,  10, 3,  6,  18, 0.00f, 0.00f },
            { EquipSlot::Chest, EquipTier::Imperial, "Imperial Cuirass", "Heavy plate etched with runes.",     1400, 16, 95,  20, 6,  14, 30, 0.00f, 0.10f },
            { EquipSlot::Chest, EquipTier::Royal,    "Royal Cuirass",    "Sovereign golden full plate.",       2800, 24, 150, 35, 12, 24, 48, 0.03f, 0.20f },
            { EquipSlot::Chest, EquipTier::Dark,     "Dark Cuirass",     "Cursed fortress plate of darkness.", 5200, 32, 238, 60, 20, 40, 76, 0.05f, 0.40f }
        }},
        // SLOT 3: LIMBS
        {{
            { EquipSlot::Limbs, EquipTier::Squire,   "Squire Greaves",   "Iron gauntlets and shin guards.",    250,  5,  5,  10, 4,  0,  2,  0.00f, 0.00f },
            { EquipSlot::Limbs, EquipTier::Silver,   "Silver Greaves",   "Silver-plated articulated limbs.",   650,  10, 12, 22, 9,  8,  5,  0.02f, 0.05f },
            { EquipSlot::Limbs, EquipTier::Imperial, "Imperial Greaves", "Reinforced war gauntlets & boots.",  1400, 16, 22, 38, 15, 16, 9,  0.04f, 0.12f },
            { EquipSlot::Limbs, EquipTier::Royal,    "Royal Greaves",    "Regal greaves of swift strikes.",    2800, 24, 36, 60, 24, 28, 15, 0.06f, 0.25f },
            { EquipSlot::Limbs, EquipTier::Dark,     "Dark Greaves",     "Spiked shadowclaw gauntlets.",       5200, 32, 60, 95, 38, 45, 24, 0.10f, 0.50f }
        }},
        // SLOT 4: CAPE
        {{
            { EquipSlot::Cape, EquipTier::Squire,   "Squire Cape",   "Tattered crimson wool cloak.",      250,  5,  0,  5,  0,  0,  0,  0.05f, 0.10f },
            { EquipSlot::Cape, EquipTier::Silver,   "Silver Cape",   "Moonlit silk mantle.",              650,  10, 10, 15, 3,  6,  2,  0.09f, 0.22f },
            { EquipSlot::Cape, EquipTier::Imperial, "Imperial Cape", "Velvet cape with golden embroidery.",1400, 16, 20, 28, 6,  14, 5,  0.14f, 0.38f },
            { EquipSlot::Cape, EquipTier::Royal,    "Royal Cape",    "Ermine-trimmed monarch cloak.",     2800, 24, 35, 45, 10, 24, 9,  0.19f, 0.60f },
            { EquipSlot::Cape, EquipTier::Dark,     "Dark Cape",     "Living shadow wings of the abyss.", 5200, 32, 55, 75, 18, 38, 16, 0.26f, 0.95f }
        }}
    }};

    const std::array<RuneDef, static_cast<size_t>(RuneType::COUNT)> RUNE_TABLE = {{
        { RuneType::Vault, "Vault Rune", "Grants +1 extra mid-air jump per equipped Vault Rune.", 400 },
        { RuneType::Sprint, "Sprint Rune", "Grants +1 air/ground Dash per equipped Sprint Rune.", 450 },
        { RuneType::Vampire, "Vampire Rune", "Restores +4 HP for every enemy slain per equipped Vampire Rune.", 600 },
        { RuneType::Siphon, "Siphon Rune", "Restores +4 MP for every enemy slain per equipped Siphon Rune.", 650 },
        { RuneType::Retaliation, "Retaliation Rune", "Returns 60% of physical damage taken back to the attacker per rune.", 500 },
        { RuneType::Bounty, "Bounty Rune", "Increases all gold collected by +15% per equipped Bounty Rune.", 800 },
        { RuneType::Haste, "Haste Rune", "Increases movement speed by +12% per equipped Haste Rune.", 550 },
        { RuneType::Grace, "Grace Rune", "Hold Jump while falling to glide gently (420 px/s) and lowers enemy scaling.", 500 }
    }};

    const std::array<ManorNodeDef, static_cast<size_t>(ManorNodeId::COUNT)> MANOR_TABLE = {{
        { ManorNodeId::Unlock_Blacksmith, "Smithy", "Unlocks the Blacksmith in the Outskirts to forge weapons and armor.", 1, 4, ManorNodeId::None, 1, 100, 0, 1, 1.0f },
        { ManorNodeId::Unlock_Enchantress, "Enchantress Tent", "Unlocks the Enchantress in the Outskirts to imbue items with Runes.", 5, 4, ManorNodeId::None, 1, 150, 0, 2, 1.0f },
        { ManorNodeId::Unlock_Architect, "Architect Study", "Unlocks the Architect who can lock the last castle layout for 70% gold.", 6, 3, ManorNodeId::Unlock_Enchantress, 1, 300, 0, 3, 1.0f },
        { ManorNodeId::Stat_Health, "Health Up", "Increases base Maximum HP by +12 per level.", 3, 4, ManorNodeId::None, 75, 80, 35, 4, 12.0f },
        { ManorNodeId::Stat_Mana, "Mana Up", "Increases base Maximum MP by +8 per level.", 4, 4, ManorNodeId::Stat_Health, 75, 80, 35, 5, 8.0f },
        { ManorNodeId::Stat_Attack, "Attack Up", "Increases base physical Strength (STR) by +3 per level.", 2, 3, ManorNodeId::Stat_Health, 75, 110, 45, 6, 3.0f },
        { ManorNodeId::Stat_MagicDmg, "Magic Damage Up", "Increases base magical Intelligence (INT) by +4 per level.", 4, 3, ManorNodeId::Stat_Mana, 75, 110, 45, 7, 4.0f },
        { ManorNodeId::Stat_Armor, "Armor Up", "Increases base Armor rating by +3 per level.", 1, 3, ManorNodeId::Unlock_Blacksmith, 50, 120, 50, 8, 3.0f },
        { ManorNodeId::Stat_EquipWeight, "Equip Up", "Increases maximum Equipment Weight capacity by +8 per level.", 0, 3, ManorNodeId::Unlock_Blacksmith, 50, 100, 40, 9, 8.0f },
        { ManorNodeId::Stat_CritChance, "Crit Chance Up", "Increases Critical Strike chance by +2% per level.", 2, 2, ManorNodeId::Stat_Attack, 25, 160, 70, 10, 0.02f },
        { ManorNodeId::Stat_CritDmg, "Crit Damage Up", "Increases Critical Strike damage multiplier by +10% per level.", 2, 1, ManorNodeId::Stat_CritChance, 25, 180, 75, 11, 0.10f },
        { ManorNodeId::Stat_GoldGain, "Gold Gain Up", "Increases all gold dropped in the castle by +10% per level.", 6, 1, ManorNodeId::Class_Miner, 10, 350, 220, 12, 0.10f },
        { ManorNodeId::Stat_Haggling, "Haggling", "Charon allows you to keep +10% of your gold when entering the castle per level.", 6, 2, ManorNodeId::Unlock_Architect, 5, 280, 180, 13, 0.10f },
        { ManorNodeId::Stat_PotionEfficacy, "Potion Efficacy", "Increases HP and MP restored by meat and mana potions by +8% per level.", 5, 2, ManorNodeId::Stat_MagicDmg, 5, 180, 90, 14, 0.08f },
        { ManorNodeId::Stat_InvulnTime, "Invulnerability Time", "Increases post-hit invincibility frame duration by +0.10s per level.", 1, 2, ManorNodeId::Stat_Armor, 5, 200, 110, 15, 0.10f },
        { ManorNodeId::Class_Barbarian, "Unlock Barbarian", "Unlocks the high-HP Barbarian class for future heirs.", 3, 3, ManorNodeId::Stat_Health, 1, 140, 0, 16, 1.0f },
        { ManorNodeId::Class_Mage, "Unlock Mage", "Unlocks the spell-casting Mage class for future heirs.", 5, 3, ManorNodeId::Stat_Mana, 1, 140, 0, 17, 1.0f },
        { ManorNodeId::Class_Knave, "Unlock Knave", "Unlocks the critical-striking Knave class for future heirs.", 3, 2, ManorNodeId::Class_Barbarian, 1, 220, 0, 18, 1.0f },
        { ManorNodeId::Class_Shinobi, "Unlock Shinobi", "Unlocks the high-damage Shinobi ninja class for future heirs.", 1, 1, ManorNodeId::Stat_InvulnTime, 1, 260, 0, 19, 1.0f },
        { ManorNodeId::Class_Miner, "Unlock Miner", "Unlocks the gold-hunting Miner class for future heirs.", 5, 1, ManorNodeId::Stat_PotionEfficacy, 1, 240, 0, 20, 1.0f },
        { ManorNodeId::Class_Lich, "Unlock Lich", "Unlocks the soul-feasting Lich class and Spellthief hybrid.", 4, 2, ManorNodeId::Stat_MagicDmg, 1, 320, 0, 21, 1.0f },
        { ManorNodeId::Class_Dragon, "Unlock Dragon", "Unlocks the flying fire-breathing Dragon class!", 4, 0, ManorNodeId::Upgrade_AllBasicClasses, 1, 900, 0, 22, 1.0f },
        { ManorNodeId::Upgrade_AllBasicClasses, "Master Observatory", "Upgrades all classes (Paladin, Barbarian King, Archmage, Assassin, Hokage, Spelunker, Lich King, Spellsword).", 3, 1, ManorNodeId::Class_Knave, 1, 550, 0, 23, 1.0f },
        { ManorNodeId::Stat_DownStrikeBonus, "Down-Strike Up", "Increases aerial Pogo Down-Strike damage by +12% per level.", 0, 2, ManorNodeId::Stat_EquipWeight, 5, 170, 85, 24, 0.12f }
    }};

} // anonymous namespace

const ClassDef& Registry::GetClass(ClassType type) {
    int idx = std::clamp(static_cast<int>(type), 0, static_cast<int>(ClassType::COUNT) - 1);
    return CLASS_TABLE[static_cast<size_t>(idx)];
}

const SpellDef& Registry::GetSpell(SpellType type) {
    int idx = std::clamp(static_cast<int>(type), 0, static_cast<int>(SpellType::COUNT) - 1);
    return SPELL_TABLE[static_cast<size_t>(idx)];
}

const TraitDef& Registry::GetTrait(TraitType type) {
    int idx = std::clamp(static_cast<int>(type), 0, static_cast<int>(TraitType::COUNT) - 1);
    return TRAIT_TABLE[static_cast<size_t>(idx)];
}

const EquipmentDef& Registry::GetEquipment(EquipSlot slot, EquipTier tier) {
    int sIdx = std::clamp(static_cast<int>(slot), 0, static_cast<int>(EquipSlot::COUNT) - 1);
    int tIdx = std::clamp(static_cast<int>(tier), 0, static_cast<int>(EquipTier::COUNT) - 1);
    return EQUIPMENT_TABLE[static_cast<size_t>(sIdx)][static_cast<size_t>(tIdx)];
}

const RuneDef& Registry::GetRune(RuneType type) {
    int idx = std::clamp(static_cast<int>(type), 0, static_cast<int>(RuneType::COUNT) - 1);
    return RUNE_TABLE[static_cast<size_t>(idx)];
}

const ManorNodeDef& Registry::GetManorNode(ManorNodeId id) {
    int idx = std::clamp(static_cast<int>(id), 0, static_cast<int>(ManorNodeId::COUNT) - 1);
    return MANOR_TABLE[static_cast<size_t>(idx)];
}

int Registry::GetNodeCost(const ManorNodeDef& node, int currentLvl, int totalManorLevels) {
    return (node.baseCost + currentLvl * node.costStep) + (totalManorLevels * Config::MANOR_INFLATION_PER_LEVEL);
}

int Registry::GetNodeCostById(ManorNodeId id, int currentLvl, int totalManorLevels) {
    return GetNodeCost(GetManorNode(id), currentLvl, totalManorLevels);
}

std::string Registry::GetBiomeName(BiomeType biome) {
    switch (biome) {
        case BiomeType::CastleHamson:   return "Castle Hamson";
        case BiomeType::ForestAbkhazia: return "Forest Abkhazia";
        case BiomeType::TheMaya:        return "The Maya";
        case BiomeType::LandOfDarkness: return "Land of Darkness";
        default:                        return "Unknown Biome";
    }
}

std::string Registry::GetEquipSlotName(EquipSlot slot) {
    switch (slot) {
        case EquipSlot::Sword: return "Sword";
        case EquipSlot::Helm:  return "Helm";
        case EquipSlot::Chest: return "Chestplate";
        case EquipSlot::Limbs: return "Limbs";
        case EquipSlot::Cape:  return "Cape";
        default:               return "Slot";
    }
}

std::string Registry::GetEquipTierName(EquipTier tier) {
    switch (tier) {
        case EquipTier::Squire:   return "Squire";
        case EquipTier::Silver:   return "Silver";
        case EquipTier::Imperial: return "Imperial";
        case EquipTier::Royal:    return "Royal";
        case EquipTier::Dark:     return "Dark";
        default:                  return "None";
    }
}

std::string Registry::GetFairyConditionDesc(FairyCondition cond) {
    switch (cond) {
        case FairyCondition::NoDamage:  return "Take NO DAMAGE in this room!";
        case FairyCondition::TimeLimit: return "Reach the chest within 7.5 SECONDS!";
        case FairyCondition::KillAll:   return "Defeat ALL ENEMIES in this room!";
        case FairyCondition::NoJumping: return "Do NOT JUMP in this room!";
        default:                        return "Complete the Fairy Trial!";
    }
}

std::string Registry::GetBossName(BossType boss) {
    switch (boss) {
        case BossType::Khidr:           return "Khidr";
        case BossType::Alexander:       return "Alexander";
        case BossType::PonceDeLeon:     return "Ponce de Leon";
        case BossType::Herodotus:       return "Herodotus";
        case BossType::JohannesTraitor: return "Johannes the Traitor";
        default:                        return "Guardian";
    }
}

std::string Registry::GetBossTitle(BossType boss) {
    switch (boss) {
        case BossType::Khidr:           return "Gatekeeper of Castle Hamson";
        case BossType::Alexander:       return "Fury Skull of Forest Abkhazia";
        case BossType::PonceDeLeon:     return "Blazing Sentinel of The Maya";
        case BossType::Herodotus:       return "Infinite Ooze of Darkness";
        case BossType::JohannesTraitor: return "Heir of the Cursed Fountain";
        default:                        return "Ancient Boss";
    }
}

HeirCandidate Registry::GenerateHeir(const GameContext& ctx, int generationNumber) {
    static std::mt19937 rng(std::random_device{}());

    static const std::array<const char*, 24> MALE_NAMES = {
        "Sir Leon", "Sir Gareth", "Sir Roland", "Sir Arthur", "Sir Tristan", "Sir Percival",
        "Sir Dorian", "Sir Alaric", "Sir Cedric", "Sir Edmund", "Sir Viktor", "Sir Belmont",
        "Sir Corvus", "Sir Julian", "Sir Marcus", "Sir Hadrian", "Sir Lucian", "Sir Galahad",
        "Sir Oswin", "Sir Roderick", "Sir Hector", "Sir Damian", "Sir Cassius", "Sir Tobias"
    };

    static const std::array<const char*, 24> FEMALE_NAMES = {
        "Lady Lyra", "Lady Freya", "Lady Elena", "Lady Brienne", "Lady Astrid", "Lady Valeria",
        "Lady Genevieve", "Lady Isolde", "Lady Beatrice", "Lady Morrigan", "Lady Sylvia", "Lady Clara",
        "Lady Rosalind", "Lady Rowena", "Lady Camilla", "Lady Vivienne", "Lady Selena", "Lady Diana",
        "Lady Octavia", "Lady Cordelia", "Lady Evelyn", "Lady Fiona", "Lady Mirabel", "Lady Thalia"
    };

    static const std::array<const char*, 10> ROMAN_NUM = {
        "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"
    };

    HeirCandidate heir;
    heir.generation = generationNumber;
    heir.visualSeed = rng();
    heir.isFemale = (rng() % 2 == 0);

    size_t nameIdx = rng() % MALE_NAMES.size();
    std::string baseName = heir.isFemale ? FEMALE_NAMES[nameIdx] : MALE_NAMES[nameIdx];
    std::string numeral = ROMAN_NUM[static_cast<size_t>((generationNumber - 1) % 10)];
    heir.name = baseName + " " + numeral;

    std::vector<ClassType> availableClasses;
    availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Knights_Base));

    if (ctx.GetManorLevel(ManorNodeId::Class_Barbarian) > 0) {
        availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Barbarian_Base));
    }
    if (ctx.GetManorLevel(ManorNodeId::Class_Mage) > 0) {
        availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Mage_Base));
    }
    if (ctx.GetManorLevel(ManorNodeId::Class_Knave) > 0) {
        availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Knave_Base));
    }
    if (ctx.GetManorLevel(ManorNodeId::Class_Shinobi) > 0) {
        availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Shinobi_Base));
    }
    if (ctx.GetManorLevel(ManorNodeId::Class_Miner) > 0) {
        availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Miner_Base));
    }
    if (ctx.GetManorLevel(ManorNodeId::Class_Lich) > 0) {
        availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Lich_Base));
        availableClasses.push_back(ctx.ResolveClassWithUpgrade(ClassType::Spellthief_Base));
    }
    if (ctx.GetManorLevel(ManorNodeId::Class_Dragon) > 0) {
        availableClasses.push_back(ClassType::Dragon_Special);
    }
    if (ctx.ngPlusLevel > 0) {
        availableClasses.push_back(ClassType::Traitor_Special);
    }

    heir.classType = availableClasses[rng() % availableClasses.size()];
    heir.spell = static_cast<SpellType>(rng() % static_cast<int>(SpellType::COUNT));

    int numTraits = 1 + static_cast<int>(rng() % 2);
    if (rng() % 5 == 0) {
        numTraits = 3;
    }

    uint32_t activeConflictMask = 0;
    for (int i = 0; i < numTraits; ++i) {
        for (int attempt = 0; attempt < 20; ++attempt) {
            TraitType candidate = static_cast<TraitType>(rng() % static_cast<int>(TraitType::COUNT));
            bool alreadyHas = false;
            for (TraitType existing : heir.traits) {
                if (existing == candidate) {
                    alreadyHas = true;
                    break;
                }
            }
            if (alreadyHas) continue;

            const TraitDef& def = GetTrait(candidate);
            if ((def.conflictMask & activeConflictMask) != 0) {
                continue;
            }

            heir.traits.push_back(candidate);
            activeConflictMask |= def.conflictMask;
            break;
        }
    }

    return heir;
}
