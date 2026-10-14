// === FILE: src/world/Room.cpp ===
#include "world/Room.hpp"
#include "data/RoomTemplates.hpp"
#include "data/Registry.hpp"
#include "entities/Player.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/ShaderManager.hpp"
#include "gfx/Palette.hpp"
#include "gfx/UiText.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

char Room::GetTileAt(int col, int row) const {
    // Якщо координати виходять за межі кімнати — перевіряємо, чи це відкритий дверний прохід
    if (row >= Config::ROOM_ROWS) {
        if ((doorMask & DoorMask::SOUTH) && col >= 10 && col <= 14) return '.';
        return '#';
    }
    if (row < 0) {
        if ((doorMask & DoorMask::NORTH) && col >= 10 && col <= 14) return '.';
        return '#';
    }
    if (col < 0) {
        if ((doorMask & DoorMask::WEST) && row >= 5 && row <= 9) return '.';
        return '#';
    }
    if (col >= Config::ROOM_COLS) {
        if ((doorMask & DoorMask::EAST) && row >= 5 && row <= 9) return '.';
        return '#';
    }

    return tiles[static_cast<size_t>(row)][static_cast<size_t>(col)];
}

void Room::BuildFromNode(
    int gx,
    int gy,
    const SavedRoomNode& node,
    int calculatedRoomLevel,
    const GameContext& ctx
) {
    gridX = gx;
    gridY = gy;
    roomType = node.roomType;
    biome = node.biome;
    doorMask = node.doorMask;
    templateIdx = std::clamp(node.templateIdx, 0, static_cast<int>(RoomTemplates::TEMPLATES.size()) - 1);
    roomLevel = std::max(1, calculatedRoomLevel);
    fairyCond = node.fairyCond;
    bossType = node.bossType;
    visited = node.visited;
    cleared = node.cleared;
    chestOpened = node.chestOpened;

    isBossArenaActive = false;
    hasActiveBoss = false;
    bossDefeatedInArena = false;

    const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(templateIdx)];
    for (int r = 0; r < Config::ROOM_ROWS; ++r) {
        std::string_view line = tmpl.rows[static_cast<size_t>(r)];
        for (int c = 0; c < Config::ROOM_COLS; ++c) {
            tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] =
                (c < static_cast<int>(line.size())) ? line[static_cast<size_t>(c)] : '.';
        }
    }

    ApplyDoorMasksToTiles();
    SpawnEntitiesFromTemplate(ctx);
}

void Room::ApplyDoorMasksToTiles() {
    if (isBossArenaActive) return;

    // NORTH (рядок 0, стовпці 11..13)
    for (int c = 11; c <= 13; ++c) {
        tiles[0][static_cast<size_t>(c)] = (doorMask & DoorMask::NORTH) ? '.' : '#';
    }
    // Під північним проходом ставимо напівпрозору платформу '=', щоб гравець міг вистрибнути наверх
    if (doorMask & DoorMask::NORTH) {
        for (int c = 10; c <= 14; ++c) {
            if (tiles[3][static_cast<size_t>(c)] == '.') {
                tiles[3][static_cast<size_t>(c)] = '=';
            }
        }
    }

    // SOUTH (стовпці 11..13) — встановлюємо платформу '=', щоб гравець міг ходити по кімнаті,
    // а спуск вниз робився навмисно комбінацією [S + Space] (або замуровуємо '#', якщо дверей немає)
    for (int c = 11; c <= 13; ++c) {
        tiles[Config::ROOM_ROWS - 1][static_cast<size_t>(c)] = (doorMask & DoorMask::SOUTH) ? '=' : '#';
        if (doorMask & DoorMask::SOUTH) {
            if (tiles[Config::ROOM_ROWS - 2][static_cast<size_t>(c)] == '^' ||
                tiles[Config::ROOM_ROWS - 2][static_cast<size_t>(c)] == '~') {
                tiles[Config::ROOM_ROWS - 2][static_cast<size_t>(c)] = '.';
            }
        }
    }

    // WEST (стовпець 0, рядки 6..8)
    for (int r = 6; r <= 8; ++r) {
        tiles[static_cast<size_t>(r)][0] = (doorMask & DoorMask::WEST) ? '.' : '#';
    }

    // EAST (стовпець 24, рядки 6..8)
    for (int r = 6; r <= 8; ++r) {
        tiles[static_cast<size_t>(r)][Config::ROOM_COLS - 1] = (doorMask & DoorMask::EAST) ? '.' : '#';
    }
}

void Room::SpawnEntitiesFromTemplate(const GameContext& ctx) {
    enemies.clear();
    interactables.clear();
    projectiles.clear();
    hasActiveBoss = false;

    int enemyTier = 1;
    if (biome == BiomeType::ForestAbkhazia)      enemyTier = (std::rand() % 2 == 0) ? 1 : 2;
    else if (biome == BiomeType::TheMaya)        enemyTier = 2;
    else if (biome == BiomeType::LandOfDarkness) enemyTier = 3;
    if (ctx.ngPlusLevel > 0) enemyTier = std::min(3, enemyTier + 1);

    for (int r = 0; r < Config::ROOM_ROWS; ++r) {
        for (int c = 0; c < Config::ROOM_COLS; ++c) {
            char ch = tiles[static_cast<size_t>(r)][static_cast<size_t>(c)];
            Rectangle tRec = GetTileRect(c, r);
            Vector2 center = { tRec.x + tRec.width * 0.5f, tRec.y + tRec.height * 0.5f };
            Vector2 bottomCenter = { tRec.x + tRec.width * 0.5f, tRec.y + tRec.height };

            switch (ch) {
                case 'C': {
                    interactables.push_back(Interactable::MakeTorch(center));
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    break;
                }
                case 'B': {
                    interactables.push_back(Interactable::MakeBreakableProp(bottomCenter, (c + r) % 3));
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    break;
                }
                case '$': {
                    bool isSilver = (roomType == RoomType::Treasure) || ((c + r) % 3 == 0);
                    interactables.push_back(Interactable::MakeChest(bottomCenter, isSilver, chestOpened));
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    break;
                }
                case 'F': {
                    interactables.push_back(Interactable::MakeFairyChest(bottomCenter, fairyCond, chestOpened));
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    fairyTrialActive = !chestOpened;
                    fairyTrialFailed = false;
                    fairyTrialCompleted = chestOpened;
                    fairyTimer = 7.5f;
                    break;
                }
                case 'P': {
                    interactables.push_back(Interactable::MakeTeleporter(bottomCenter));
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    break;
                }
                case 'T': {
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    if (!cleared) {
                        Enemy turret;
                        turret.Init(EnemyType::WallTurret, enemyTier, roomLevel, center, false);
                        turret.facingDir = (c < Config::ROOM_COLS / 2) ? 1.0f : -1.0f;
                        enemies.push_back(turret);
                    }
                    break;
                }
                case 'S': {
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    if (!cleared) {
                        EnemyType eType = EnemyType::Skeleton;
                        int roll = (c * 13 + r * 7 + gridX * 5 + gridY * 3) % 5;
                        if (roll == 0)      eType = EnemyType::Skeleton;
                        else if (roll == 1) eType = EnemyType::FloatingEye;
                        else if (roll == 2) eType = EnemyType::ShieldKnight;
                        else if (roll == 3) eType = EnemyType::FireMage;
                        else                eType = EnemyType::Warg;

                        Enemy en;
                        en.Init(eType, enemyTier, roomLevel, { center.x, bottomCenter.y - 24.0f }, false);
                        enemies.push_back(en);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    // У кімнаті BossEntrance спавнимо двері до Боса відповідного біому
    if (roomType == RoomType::BossEntrance && !isBossArenaActive) {
        Vector2 doorPos = {
            static_cast<float>(Config::ROOM_OFFSET_X + 12 * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f),
            static_cast<float>(Config::ROOM_OFFSET_Y + 12 * Config::TILE_SIZE)
        };
        interactables.push_back(Interactable::MakeBossDoor(doorPos, bossType));
    }
}

void Room::OnPlayerEnterRoom(GameContext& ctx, Player& player) {
    visited = true;
    projectiles.clear();
    trapTimer = 0.0f;
    player.ResetRoomTrialFlags();

    // 1. Очищаємо залишкових фантомів деменції перед повторним входом, щоб вони не накопичувалися
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.isDementiaPhantom; }),
        enemies.end()
    );

    // 2. Скидаємо розташування та стан усіх живих ворогів до точок спавну, обнуляючи таймери та заряджені атаки
    for (auto& en : enemies) {
        if (en.active) {
            en.ResetToSpawn();
        }
    }

    if (roomType == RoomType::FairyChest && !chestOpened) {
        fairyTrialActive = true;
        fairyTrialFailed = false;
        fairyTrialCompleted = false;
        fairyTimer = 7.5f;
        for (auto& it : interactables) {
            if (it.kind == InteractableKind::FairyChest) {
                it.isLocked = (fairyCond == FairyCondition::KillAll && !enemies.empty());
            }
        }
    }

    // Риса Dementia: у кожній бойовій кімнаті спавняться 1-2 напівпрозорі фіолетові фантоми
    if (ctx.CurrentHeirHasTrait(TraitType::Dementia) &&
        roomType == RoomType::Combat && !cleared) {
        int phantomCount = 1 + (std::rand() % 2);
        for (int i = 0; i < phantomCount; ++i) {
            Enemy ph;
            EnemyType pType = (i % 2 == 0) ? EnemyType::FloatingEye : EnemyType::Skeleton;
            Vector2 pPos = {
                static_cast<float>(Config::VIRTUAL_WIDTH * (0.35f + i * 0.30f)),
                static_cast<float>(Config::VIRTUAL_HEIGHT * 0.40f)
            };
            ph.Init(pType, 1, roomLevel, pPos, true);
            enemies.push_back(ph);
        }
    }
}

void Room::EnterBossArena(BossType targetBoss, int bossLvl, GameContext& ctx, Player& player) {
    isBossArenaActive = true;
    bossDefeatedInArena = false;
    bossType = targetBoss;

    int arenaTemplateIdx = 15 + std::clamp(static_cast<int>(targetBoss), 0, 4);
    const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(arenaTemplateIdx)];
    for (int r = 0; r < Config::ROOM_ROWS; ++r) {
        std::string_view line = tmpl.rows[static_cast<size_t>(r)];
        for (int c = 0; c < Config::ROOM_COLS; ++c) {
            tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] =
                (c < static_cast<int>(line.size())) ? line[static_cast<size_t>(c)] : '#';
        }
    }

    enemies.clear();
    interactables.clear();
    projectiles.clear();

    // Розставляємо факели/бочки з шаблону арени
    for (int r = 0; r < Config::ROOM_ROWS; ++r) {
        for (int c = 0; c < Config::ROOM_COLS; ++c) {
            char ch = tiles[static_cast<size_t>(r)][static_cast<size_t>(c)];
            Rectangle tRec = GetTileRect(c, r);
            Vector2 center = { tRec.x + tRec.width * 0.5f, tRec.y + tRec.height * 0.5f };
            Vector2 bottomCenter = { tRec.x + tRec.width * 0.5f, tRec.y + tRec.height };
            if (ch == 'C') {
                interactables.push_back(Interactable::MakeTorch(center));
                tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
            } else if (ch == 'B') {
                interactables.push_back(Interactable::MakeBreakableProp(bottomCenter, 0));
                tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
            }
        }
    }

    activeBoss.Init(targetBoss, bossLvl, { Config::VIRTUAL_WIDTH * 0.5f, 280.0f });
    hasActiveBoss = true;

    // Безпечний спавн гравця на твердій кам'яній підлозі (без шипів під ногами)
    float spawnCol = 2.0f;
    float floorRow = 13.0f;
    if (targetBoss == BossType::Alexander) {
        // На твердому кам'яному балконі Alexander (рядок 12 — суцільний камінь)
        floorRow = 12.0f;
    }
    float spawnX = static_cast<float>(Config::ROOM_OFFSET_X) + spawnCol * static_cast<float>(Config::TILE_SIZE) + static_cast<float>(Config::TILE_SIZE) * 0.5f;
    float spawnY = floorRow * static_cast<float>(Config::TILE_SIZE) - player.height;

    player.pos = { spawnX - player.width * 0.5f, spawnY };
    player.vel = { 0.0f, 0.0f };
    player.onGround = true;
    player.dropThroughPlatformTimer = 0.0f;
    player.facingDir = 1.0f; // повернутий праворуч на боса
    player.iframeTimer = std::max(player.iframeTimer, 1.2f); // захисні кадри при вході в арену

    if (ctx.audio) {
        ctx.audio->SetBossMusic(true);
        ctx.audio->PlaySfx(SfxId::BossRoar);
    }
}

void Room::Update(float dt, GameContext& ctx, Player& player) {
    trapTimer += dt;

    // 1. Оновлення гравця
    player.Update(dt, ctx, this, projectiles);

    // 2. Обробка крику Варвара (Barbarian Shout: знищує ворожі снаряди, відкидає і оглушує ворогів)
    ShoutPulseEvent shout{};
    if (player.ConsumeShoutPulse(shout)) {
        for (auto& proj : projectiles) {
            if (!proj.fromPlayer) {
                if (ctx.particles) ctx.particles->SpawnSparks(proj.pos, WHITE, 4);
                proj.active = false;
            }
        }
        for (auto& en : enemies) {
            if (!en.active) continue;
            Vector2 d = { en.GetCenter().x - shout.origin.x, en.GetCenter().y - shout.origin.y };
            float len = std::max(1.0f, std::hypot(d.x, d.y));
            if (len <= shout.radius) {
                Vector2 kb = { (d.x / len) * shout.knockbackSpeed, -240.0f };
                en.ApplyStunAndKnockback(kb, shout.stunDuration);
                if (shout.damage > 0) {
                    if (en.TakeDamage(shout.damage, kb, false, ctx)) {
                        player.OnEnemyKilled(ctx, en.type);
                        if (!en.isDementiaPhantom) {
                            Interactable::SpawnLootBurst(interactables, en.GetCenter(), en.goldReward, true);
                        }
                    }
                }
            }
        }
    }

    // 3. Наведення воронів (CrowStorm) на найближчого живого ворога чи боса
    for (auto& proj : projectiles) {
        if (proj.active && proj.kind == ProjectileKind::PlayerCrow) {
            proj.hasValidTargetHint = false;
            float bestDist = 99999.0f;
            if (hasActiveBoss && activeBoss.active) {
                proj.targetHint = activeBoss.GetCenter();
                proj.hasValidTargetHint = true;
            } else {
                for (const auto& en : enemies) {
                    if (!en.active) continue;
                    float d = std::hypot(en.GetCenter().x - proj.pos.x, en.GetCenter().y - proj.pos.y);
                    if (d < bestDist) {
                        bestDist = d;
                        proj.targetHint = en.GetCenter();
                        proj.hasValidTargetHint = true;
                    }
                }
            }
        }
    }

    // 4. Оновлення снарядів
    bool ts = player.IsTimeStopActive();
    for (auto& proj : projectiles) {
        proj.Update(dt, ctx, this, player.GetCenter(), ts);
    }

    // 5. Оновлення ворогів
    for (auto& en : enemies) {
        en.Update(dt, ctx, this, player, projectiles, ts);
    }

    // 6. Оновлення Боса (якщо активний)
    if (hasActiveBoss && activeBoss.active) {
        activeBoss.Update(dt, ctx, this, player, projectiles, enemies, ts);
    }

    // 7. Перевірка бойових колізій (меч, снаряди, контакт із ворогами/босом)
    HandleCombatCollisions(ctx, player);

    // 8. Очищення мертвих ворогів та снарядів
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return !e.active; }),
        enemies.end()
    );
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.active; }),
        projectiles.end()
    );

    if (enemies.empty() && !hasActiveBoss && roomType == RoomType::Combat) {
        cleared = true;
    }

    // 9. Перевірка перемоги над Босом в арені
    if (hasActiveBoss && !activeBoss.active && !bossDefeatedInArena) {
        bossDefeatedInArena = true;
        hasActiveBoss = false;
        int bIdx = std::clamp(static_cast<int>(bossType), 0, 4);
        ctx.bossDefeated[static_cast<size_t>(bIdx)] = true;
        if (ctx.audio) ctx.audio->SetBossMusic(false);

        if (bossType == BossType::JohannesTraitor) {
            // Перемога над фінальним босом (Фаза 2 The Fountain)!
            requestFinalVictory = true;
        } else {
            // Висипаємо велику нагороду золотом + скриню та портал виходу з арени
            Vector2 centerFloor = {
                Config::VIRTUAL_WIDTH * 0.5f,
                static_cast<float>((Config::ROOM_ROWS - 1) * Config::TILE_SIZE)
            };
            Interactable::SpawnLootBurst(interactables, { centerFloor.x, centerFloor.y - 60.0f }, 450 + roomLevel * 25, true);
            interactables.push_back(Interactable::MakeChest({ centerFloor.x - 90.0f, centerFloor.y }, true, false));
            interactables.push_back(Interactable::MakeTeleporter({ centerFloor.x + 90.0f, centerFloor.y }));
        }
    }

    // 10. Оновлення випробування Скрині Феї
    if (roomType == RoomType::FairyChest && fairyTrialActive && !fairyTrialFailed && !fairyTrialCompleted) {
        EvaluateFairyTrial(dt, ctx, player);
    }

    // 11. Оновлення інтерактивних об'єктів та підбір луту
    std::vector<Interactable> newlySpawnedDrops;
    for (auto& it : interactables) {
        it.Update(dt, ctx, this, player, newlySpawnedDrops);
        if ((it.kind == InteractableKind::Chest || it.kind == InteractableKind::FairyChest) && it.isOpened) {
            chestOpened = true;
            if (it.kind == InteractableKind::FairyChest) {
                fairyTrialCompleted = true;
                fairyTrialActive = false;
            }
        }

        // Перевірка взаємодії з Телепортом, Дверима Боса та Золотими Дверима (W / E / UP)
        if (it.active && CheckCollisionRecs(it.GetBounds(), player.GetBounds())) {
            bool interactKey = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_UP);
            if (interactKey) {
                if (it.kind == InteractableKind::Teleporter) {
                    if (isBossArenaActive) {
                        requestExitBossArena = true;
                    } else {
                        requestOpenTeleporterMap = true;
                    }
                } else if (it.kind == InteractableKind::BossDoor) {
                    int bIdx = static_cast<int>(it.bossDoorTarget);
                    if (!ctx.bossDefeated[static_cast<size_t>(bIdx)]) {
                        requestEnterBossArena = true;
                        requestedBossArenaType = it.bossDoorTarget;
                    } else if (ctx.particles) {
                        ctx.particles->SpawnFloatingText(it.GetCenter(), "BOSS ALREADY SLAIN", GRAY, 20, false);
                    }
                }
            }
        }
    }

    // Перевірка входу у Золоті Двері Фінального Боса у кімнаті Entrance (4, 4)
    if (roomType == RoomType::Entrance && !isBossArenaActive) {
        Rectangle goldenDoorRec = { Config::VIRTUAL_WIDTH * 0.5f - 58.0f, 14.0f * Config::TILE_SIZE - 168.0f, 116.0f, 168.0f };
        if (CheckCollisionRecs(player.GetBounds(), goldenDoorRec)) {
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_UP)) {
                bool allFourSlain = ctx.bossDefeated[0] && ctx.bossDefeated[1] &&
                                    ctx.bossDefeated[2] && ctx.bossDefeated[3];
                if (allFourSlain) {
                    requestEnterBossArena = true;
                    requestedBossArenaType = BossType::JohannesTraitor;
                } else if (ctx.particles) {
                    ctx.particles->SpawnFloatingText(
                        { Config::VIRTUAL_WIDTH * 0.5f, goldenDoorRec.y + 40.0f },
                        "DEFEAT ALL 4 GUARDIANS TO UNSEAL!",
                        GOLD, 20, false
                    );
                }
            }
        }
    }

    for (const auto& drop : newlySpawnedDrops) {
        interactables.push_back(drop);
    }
    interactables.erase(
        std::remove_if(interactables.begin(), interactables.end(), [](const Interactable& i) { return !i.active; }),
        interactables.end()
    );
}

void Room::EvaluateFairyTrial(float dt, GameContext& ctx, Player& player) {
    bool failedNow = false;

    switch (fairyCond) {
        case FairyCondition::NoDamage:
            if (player.damagedInCurrentRoom) failedNow = true;
            break;
        case FairyCondition::TimeLimit:
            fairyTimer -= dt;
            if (fairyTimer <= 0.0f) {
                fairyTimer = 0.0f;
                failedNow = true;
            }
            break;
        case FairyCondition::KillAll:
            if (enemies.empty()) {
                for (auto& it : interactables) {
                    if (it.kind == InteractableKind::FairyChest) {
                        it.isLocked = false;
                    }
                }
            }
            break;
        case FairyCondition::NoJumping:
            if (player.jumpedInCurrentRoom) failedNow = true;
            break;
    }

    if (!failedNow && fairyCond != FairyCondition::KillAll) {
        for (auto& it : interactables) {
            if (it.kind == InteractableKind::FairyChest) {
                it.isLocked = false;
            }
        }
    }

    if (failedNow) {
        fairyTrialFailed = true;
        fairyTrialActive = false;
        for (auto& it : interactables) {
            if (it.kind == InteractableKind::FairyChest) {
                it.isLocked = true;
            }
        }
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
        if (ctx.particles) {
            ctx.particles->SpawnFloatingText(player.GetCenter(), "FAIRY TRIAL FAILED!", RED, 24, true);
        }
    }
}

void Room::HandleCombatCollisions(GameContext& ctx, Player& player) {
    std::vector<Interactable> combatDrops;

    // 1. УДАР МЕЧЕМ ГРАВЦЯ (включаючи Pogo Down-Strike)
    if (player.isAttacking) {
        Rectangle swordBox = player.GetSwordHitbox();

        // 1a. Руйнування розтрісканих секретних стін 'X' мечем
        int scMin = WorldToCol(swordBox.x);
        int scMax = WorldToCol(swordBox.x + swordBox.width);
        int srMin = WorldToRow(swordBox.y);
        int srMax = WorldToRow(swordBox.y + swordBox.height);
        for (int r = srMin; r <= srMax; ++r) {
            for (int c = scMin; c <= scMax; ++c) {
                if (r >= 0 && r < Config::ROOM_ROWS && c >= 0 && c < Config::ROOM_COLS) {
                    if (tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] == 'X') {
                        tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                        if (player.isDownStriking) player.TriggerPogoBounce(ctx);
                        if (ctx.audio) {
                            ctx.audio->PlaySfx(SfxId::HitEnemy, 0.7f, 1.0f);
                            ctx.audio->PlaySfx(SfxId::ManorBuild, 1.3f, 0.8f);
                        }
                        Rectangle tr = GetTileRect(c, r);
                        Vector2 tCenter = { tr.x + tr.width * 0.5f, tr.y + tr.height * 0.5f };
                        if (ctx.particles) {
                            Color debrisCol = (biome == BiomeType::ForestAbkhazia) ? Color{ 86, 138, 92, 255 } :
                                              (biome == BiomeType::TheMaya) ? Color{ 188, 164, 132, 255 } :
                                              Color{ 105, 118, 148, 255 };
                            ctx.particles->SpawnDebris(tCenter, debrisCol, 18);
                            ctx.particles->SpawnDust(tCenter, 10);
                            ctx.particles->SpawnSparks(tCenter, GetPalette().fireGold[3], 8);
                            ctx.particles->SpawnFloatingText({ tCenter.x, tr.y - 6.0f }, "SECRET WALL BROKEN!", GetPalette().fireGold[3], 20, true);
                        }
                    }
                }
            }
        }

        // 1b. Руйнування інтерактивних об'єктів та Pogo від них
        for (auto& it : interactables) {
            if (!it.active) continue;
            bool alreadyHit = (std::find(player.currentSwingHitIds.begin(), player.currentSwingHitIds.end(), it.id) != player.currentSwingHitIds.end());
            if (!alreadyHit && CheckCollisionRecs(swordBox, it.GetBounds())) {
                player.currentSwingHitIds.push_back(it.id);
                it.OnHitByPlayer(ctx, player, combatDrops);
            }
        }

        // 1b. Збивання ворожих снарядів (кісток та стріл) мечем
        for (auto& proj : projectiles) {
            if (proj.active && !proj.fromPlayer && proj.destroyableBySword) {
                if (CheckCollisionRecs(swordBox, proj.GetHitbox())) {
                    proj.active = false;
                    if (ctx.particles) ctx.particles->SpawnSparks(proj.pos, WHITE, 6);
                    if (player.isDownStriking) player.TriggerPogoBounce(ctx);
                }
            }
        }

        // 1c. Удар мечем по звичайних ворогах
        for (auto& en : enemies) {
            if (!en.active) continue;
            bool alreadyHit = (std::find(player.currentSwingHitIds.begin(), player.currentSwingHitIds.end(), en.id) != player.currentSwingHitIds.end());
            if (alreadyHit || !CheckCollisionRecs(swordBox, en.GetBounds())) continue;

            player.currentSwingHitIds.push_back(en.id);

            // Перевірка щита ShieldKnight (Pogo Down-Strike пробиває щит зверху!)
            if (en.IsBlockedByShield(player.GetCenter(), player.isDownStriking)) {
                if (ctx.audio)     ctx.audio->PlaySfx(SfxId::HitShield);
                if (ctx.particles) ctx.particles->SpawnSparks(en.GetCenter(), SKYBLUE, 14);
                ctx.AddTrauma(0.18f);
                if (player.isDownStriking) player.TriggerPogoBounce(ctx);
                continue;
            }

            bool isCrit = false;
            int dmg = player.RollMeleeDamage(player.isDownStriking, isCrit);
            float kbDir = (en.GetCenter().x >= player.GetCenter().x) ? 1.0f : -1.0f;
            Vector2 kbVel = { kbDir * 210.0f, -150.0f };

            // Phase 7: Hit-stop (3 кадри) та trauma screen shake (0.25 normal, 0.40 crit)
            ctx.RequestHitStop(3);
            ctx.AddTrauma(isCrit ? 0.40f : 0.25f);

            bool killed = en.TakeDamage(dmg, kbVel, isCrit, ctx);
            player.OnMeleeDamageDealt(dmg, ctx);
            if (player.isDownStriking) {
                player.TriggerPogoBounce(ctx);
            }
            if (killed) {
                if (en.type == EnemyType::Skeleton && ctx.particles) {
                    ctx.particles->SpawnBones(en.GetCenter(), 14);
                }
                player.OnEnemyKilled(ctx, en.type);
                if (!en.isDementiaPhantom) {
                    Interactable::SpawnLootBurst(combatDrops, en.GetCenter(), en.goldReward, true);
                }
            }
        }

        // 1d. Удар мечем по Босу
        if (hasActiveBoss && activeBoss.active) {
            bool isCrit = false;
            int dmg = player.RollMeleeDamage(player.isDownStriking, isCrit);
            Vector2 hitPos{};
            if (activeBoss.CheckAndApplyPlayerAttack(swordBox, dmg, isCrit, 0, player.currentSwingHitIds, ctx, projectiles, hitPos)) {
                // Phase 7: Hit-stop (3 кадри) та trauma screen shake при ударі по босу
                ctx.RequestHitStop(3);
                ctx.AddTrauma(isCrit ? 0.40f : 0.25f);

                player.OnMeleeDamageDealt(dmg, ctx);
                if (player.isDownStriking) {
                    player.TriggerPogoBounce(ctx);
                }
            }
        }
    }

    // 2. СНАРЯДИ ГРАВЦЯ ПРОТИ ВОРОГІВ, ОБ'ЄКТІВ ТА БОСА
    for (auto& proj : projectiles) {
        if (!proj.active || !proj.fromPlayer) continue;
        Rectangle pBox = proj.GetHitbox();

        // Руйнування розтрісканих секретних стін 'X' закляттями чи кулями дракона
        int pcMin = WorldToCol(pBox.x);
        int pcMax = WorldToCol(pBox.x + pBox.width);
        int prMin = WorldToRow(pBox.y);
        int prMax = WorldToRow(pBox.y + pBox.height);
        for (int r = prMin; r <= prMax; ++r) {
            for (int c = pcMin; c <= pcMax; ++c) {
                if (r >= 0 && r < Config::ROOM_ROWS && c >= 0 && c < Config::ROOM_COLS) {
                    if (tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] == 'X') {
                        tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                        if (!proj.piercing) proj.active = false;
                        if (ctx.audio) {
                            ctx.audio->PlaySfx(SfxId::HitEnemy, 0.7f, 1.0f);
                            ctx.audio->PlaySfx(SfxId::ManorBuild, 1.3f, 0.8f);
                        }
                        Rectangle tr = GetTileRect(c, r);
                        Vector2 tCenter = { tr.x + tr.width * 0.5f, tr.y + tr.height * 0.5f };
                        if (ctx.particles) {
                            Color debrisCol = (biome == BiomeType::ForestAbkhazia) ? Color{ 86, 138, 92, 255 } :
                                              (biome == BiomeType::TheMaya) ? Color{ 188, 164, 132, 255 } :
                                              Color{ 105, 118, 148, 255 };
                            ctx.particles->SpawnDebris(tCenter, debrisCol, 18);
                            ctx.particles->SpawnDust(tCenter, 10);
                            ctx.particles->SpawnSparks(tCenter, GOLD, 8);
                            ctx.particles->SpawnFloatingText({ tCenter.x, tr.y - 6.0f }, "SECRET WALL BROKEN!", GOLD, 20, true);
                        }
                        ctx.AddTrauma(0.60f); // Screen shake trauma: 0.60 on explosion/wall break
                    }
                }
            }
        }
        if (!proj.active) continue;

        for (auto& it : interactables) {
            if (!it.active) continue;
            if ((it.kind == InteractableKind::Torch || it.kind == InteractableKind::BreakableProp) &&
                CheckCollisionRecs(pBox, it.GetBounds())) {
                it.OnHitByPlayer(ctx, player, combatDrops);
                if (!proj.piercing) {
                    proj.active = false;
                    break;
                }
            }
        }
        if (!proj.active) continue;

        for (auto& en : enemies) {
            if (!en.active || proj.HasAlreadyHit(en.id)) continue;
            if (!CheckCollisionRecs(pBox, en.GetBounds())) continue;

            proj.RegisterHit(en.id);
            if (en.IsBlockedByShield(proj.pos, false)) {
                if (ctx.audio)     ctx.audio->PlaySfx(SfxId::HitShield);
                if (ctx.particles) ctx.particles->SpawnSparks(proj.pos, SKYBLUE, 8);
                if (!proj.piercing) {
                    proj.active = false;
                    break;
                }
                continue;
            }

            float kbDir = (en.GetCenter().x >= proj.pos.x) ? 1.0f : -1.0f;
            bool killed = en.TakeDamage(proj.damage, { kbDir * 140.0f, -100.0f }, false, ctx);
            if (killed) {
                player.OnEnemyKilled(ctx, en.type);
                if (!en.isDementiaPhantom) {
                    Interactable::SpawnLootBurst(combatDrops, en.GetCenter(), en.goldReward, true);
                }
            }
            if (!proj.piercing) {
                proj.active = false;
                break;
            }
        }
        if (!proj.active) continue;

        if (hasActiveBoss && activeBoss.active) {
            Vector2 hitCenter{};
            if (activeBoss.CheckAndApplyPlayerAttack(pBox, proj.damage, false, proj.id, proj.hitIds, ctx, projectiles, hitCenter)) {
                if (!proj.piercing) {
                    proj.active = false;
                }
            }
        }
    }

    // 3. ВОРОЖІ СНАРЯДИ ПРОТИ ГРАВЦЯ
    Rectangle playerBox = player.GetBounds();
    for (auto& proj : projectiles) {
        if (!proj.active || proj.fromPlayer) continue;
        if (CheckCollisionRecs(proj.GetHitbox(), playerBox)) {
            int retal = 0;
            player.TakeDamage(proj.damage, proj.pos, ctx, "Enemy Spell", retal, &projectiles);
            if (!proj.piercing) {
                proj.active = false;
            }
        }
    }

    // 4. КОНТАКТНА ШКОДА ВІД ВОРОГІВ ТА БОСА ПО ГРАВЦЮ (З ПІДТРИМКОЮ РУНИ RETALIATION)
    for (auto& en : enemies) {
        if (!en.active || en.stunTimer > 0.0f) continue;
        if (CheckCollisionRecs(en.GetBounds(), playerBox)) {
            int retalDmg = 0;
            if (player.TakeDamage(en.contactDamage, en.GetCenter(), ctx, en.GetDisplayName(), retalDmg, &projectiles)) {
                if (retalDmg > 0) {
                    if (en.TakeDamage(retalDmg, { -en.facingDir * 200.0f, -150.0f }, false, ctx)) {
                        player.OnEnemyKilled(ctx, en.type);
                        if (!en.isDementiaPhantom) {
                            Interactable::SpawnLootBurst(combatDrops, en.GetCenter(), en.goldReward, true);
                        }
                    }
                }
            }
        }
    }

    if (hasActiveBoss && activeBoss.active) {
        Vector2 contactPos{};
        if (activeBoss.CheckPlayerContact(playerBox, contactPos)) {
            int retalDmg = 0;
            if (player.TakeDamage(activeBoss.contactDamage, contactPos, ctx, activeBoss.GetCurrentName(), retalDmg, &projectiles)) {
                if (retalDmg > 0) {
                    std::vector<uint32_t> dummyReg;
                    Vector2 dummyCenter{};
                    activeBoss.CheckAndApplyPlayerAttack(playerBox, retalDmg, false, 999999, dummyReg, ctx, projectiles, dummyCenter);
                }
            }
        }
    }

    for (const auto& d : combatDrops) {
        interactables.push_back(d);
    }
}

std::vector<Vector2> Room::GetActiveTorchScreenPositions() const {
    std::vector<Vector2> positions;
    for (const auto& it : interactables) {
        if (it.active && it.kind == InteractableKind::Torch) {
            positions.push_back(it.GetCenter());
        }
    }
    return positions;
}

void Room::Draw(const GameContext& ctx, const Player& player) const {
    DrawBackgroundAndTiles(ctx, player);
    DrawProps(ctx, player);
    DrawEntities(ctx, player);
}

void Room::DrawBackgroundAndTiles(const GameContext& ctx, const Player& player) const {
    if (!ctx.renderer) return;

    // 1. Фон біому
    ctx.renderer->DrawBiomeBackground(biome, gridX, gridY);

    // 2. Золоті Двері Фінального Боса у кімнаті Entrance (4, 4)
    if (roomType == RoomType::Entrance && !isBossArenaActive) {
        Vector2 goldenDoorBottom = {
            Config::VIRTUAL_WIDTH * 0.5f,
            static_cast<float>((Config::ROOM_ROWS - 1) * Config::TILE_SIZE)
        };
        ctx.renderer->DrawGoldenBossDoor(goldenDoorBottom, ctx.bossDefeated, trapTimer);
    }

    // 3. Тайлова карта кімнати з процедурним автотайлінгом
    auto IsSolidTile = [this](int col, int row) -> bool {
        char ch = GetTileAt(col, row);
        return (ch == '#' || ch == 'X' || ch == 'I');
    };

    for (int r = 0; r < Config::ROOM_ROWS; ++r) {
        for (int c = 0; c < Config::ROOM_COLS; ++c) {
            char tc = tiles[static_cast<size_t>(r)][static_cast<size_t>(c)];
            if (tc != '.') {
                float illusionAlpha = 1.0f;
                if (tc == 'I') {
                    Rectangle tRec = GetTileRect(c, r);
                    Vector2 tcCenter = { tRec.x + tRec.width * 0.5f, tRec.y + tRec.height * 0.5f };
                    float d = std::hypot(player.GetCenter().x - tcCenter.x, player.GetCenter().y - tcCenter.y);
                    if (d < 120.0f) {
                        illusionAlpha = std::clamp((d - 36.0f) / 84.0f, 0.20f, 1.0f);
                    }
                }

                uint8_t nMask = 0;
                if (IsSolidTile(c, r - 1)) nMask |= 1; // North
                if (IsSolidTile(c + 1, r)) nMask |= 2; // East
                if (IsSolidTile(c, r + 1)) nMask |= 4; // South
                if (IsSolidTile(c - 1, r)) nMask |= 8; // West

                uint8_t dMask = 0;
                if (IsSolidTile(c - 1, r - 1)) dMask |= 1; // NW
                if (IsSolidTile(c + 1, r - 1)) dMask |= 2; // NE
                if (IsSolidTile(c - 1, r + 1)) dMask |= 4; // SW
                if (IsSolidTile(c + 1, r + 1)) dMask |= 8; // SE

                ctx.renderer->DrawTile(tc, c, r, biome, trapTimer, illusionAlpha, nMask, dMask);
            }
        }
    }

    // 4. Візуалізація відкритих дверей по краях кімнати
    if (!isBossArenaActive) {
        const auto& pal = GetPalette();
        if (doorMask & DoorMask::WEST) {
            DrawRectangle(Config::ROOM_OFFSET_X, 6 * Config::TILE_SIZE, 8, 3 * Config::TILE_SIZE, Fade(pal.fireGold[3], 0.28f));
        }
        if (doorMask & DoorMask::EAST) {
            DrawRectangle(Config::ROOM_OFFSET_X + Config::ROOM_PIXEL_WIDTH - 8, 6 * Config::TILE_SIZE, 8, 3 * Config::TILE_SIZE, Fade(pal.fireGold[3], 0.28f));
        }
        if (doorMask & DoorMask::NORTH) {
            DrawRectangle(Config::ROOM_OFFSET_X + 11 * Config::TILE_SIZE, 0, 3 * Config::TILE_SIZE, 8, Fade(pal.fireGold[3], 0.28f));
        }
        if (doorMask & DoorMask::SOUTH) {
            DrawRectangle(Config::ROOM_OFFSET_X + 11 * Config::TILE_SIZE, Config::ROOM_PIXEL_HEIGHT - 8, 3 * Config::TILE_SIZE, 8, Fade(pal.fireGold[3], 0.28f));
        }
    }
}

void Room::DrawProps(const GameContext& ctx, [[maybe_unused]] const Player& player) const {
    for (const auto& it : interactables) {
        it.Draw(ctx);
    }
}

void Room::DrawEntities(const GameContext& ctx, const Player& player) const {
    for (const auto& en : enemies) {
        if (en.hurtFlashTimer > 0.0f && ctx.shaders) {
            ctx.shaders->BeginWhiteFlash();
            en.Draw();
            ctx.shaders->EndWhiteFlash();
        } else {
            en.Draw();
        }
    }
    if (hasActiveBoss && activeBoss.active) {
        if (activeBoss.hurtFlashTimer > 0.0f && ctx.shaders) {
            ctx.shaders->BeginWhiteFlash();
            activeBoss.Draw();
            ctx.shaders->EndWhiteFlash();
        } else {
            activeBoss.Draw();
        }
    }
    if (player.hurtFlashTimer > 0.0f && ctx.shaders) {
        ctx.shaders->BeginWhiteFlash();
        player.Draw(ctx);
        ctx.shaders->EndWhiteFlash();
    } else {
        player.Draw(ctx);
    }
    for (const auto& proj : projectiles) {
        proj.Draw();
    }
}

void Room::DrawLights([[maybe_unused]] const GameContext& ctx, const Player& player) const {
    // 1. Смолоскипи кімнати: м'які золотисто-бурштинові радіальні градієнти світла
    for (const auto& it : interactables) {
        if (!it.active || it.kind != InteractableKind::Torch) continue;
        Vector2 c = it.GetCenter();
        float flicker = std::sin(it.animTimer * 14.0f + static_cast<float>(it.id) * 2.37f) * 6.5f;
        float r = 165.0f + flicker;
        DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y - 12.0f), r, Color{ 255, 195, 105, 235 }, BLANK);
        DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y - 12.0f), r * 0.45f, Color{ 255, 235, 165, 255 }, BLANK);
    }

    // 2. Двері та портали: світлові ареали
    if (roomType == RoomType::Entrance && !isBossArenaActive) {
        Vector2 doorCenter = { Config::VIRTUAL_WIDTH * 0.5f, static_cast<float>((Config::ROOM_ROWS - 1) * Config::TILE_SIZE - 40.0f) };
        DrawCircleGradient(static_cast<int>(doorCenter.x), static_cast<int>(doorCenter.y), 210.0f, Color{ 255, 215, 120, 210 }, BLANK);
    }
    for (const auto& it : interactables) {
        if (!it.active) continue;
        if (it.kind == InteractableKind::BossDoor) {
            DrawCircleGradient(static_cast<int>(it.pos.x), static_cast<int>(it.pos.y - 40.0f), 170.0f, Color{ 240, 85, 65, 200 }, BLANK);
        } else if (it.kind == InteractableKind::Teleporter) {
            DrawCircleGradient(static_cast<int>(it.pos.x), static_cast<int>(it.pos.y - 20.0f), 140.0f, Color{ 120, 205, 255, 210 }, BLANK);
        } else if (it.kind == InteractableKind::FairyChest) {
            DrawCircleGradient(static_cast<int>(it.pos.x), static_cast<int>(it.pos.y - 10.0f), 95.0f, Color{ 150, 225, 255, 185 }, BLANK);
        }
    }

    // 3. Аура навколо героя (гравець завжди читається у темних кутах)
    Vector2 pCenter = player.GetCenter();
    DrawCircleGradient(static_cast<int>(pCenter.x), static_cast<int>(pCenter.y), 125.0f, Color{ 220, 230, 255, 180 }, BLANK);

    // Шахтарський ліхтар героя
    if (player.headlampOn) {
        float dir = player.facingDir;
        DrawCircleGradient(static_cast<int>(pCenter.x + dir * 85.0f), static_cast<int>(pCenter.y), 195.0f, Color{ 255, 245, 180, 235 }, BLANK);
    }

    // Спалах світла від меча під час атаки
    if (player.isAttacking) {
        Rectangle swBox = player.GetSwordHitbox();
        Vector2 swCenter = { swBox.x + swBox.width * 0.5f, swBox.y + swBox.height * 0.5f };
        DrawCircleGradient(static_cast<int>(swCenter.x), static_cast<int>(swCenter.y), 85.0f, Color{ 235, 245, 255, 230 }, BLANK);
    }

    // 4. Світло від снарядів
    for (const auto& proj : projectiles) {
        if (!proj.active) continue;
        switch (proj.kind) {
            case ProjectileKind::PlayerFlameOrb:
            case ProjectileKind::PlayerDragonFireball:
            case ProjectileKind::BossFlameTrail: {
                float r = proj.radius * proj.scale * 3.8f;
                DrawCircleGradient(static_cast<int>(proj.pos.x), static_cast<int>(proj.pos.y), r, Color{ 255, 155, 45, 220 }, BLANK);
                break;
            }
            case ProjectileKind::EnemyFirePillar: {
                DrawCircleGradient(static_cast<int>(proj.pos.x), static_cast<int>(proj.pos.y - 45.0f), 150.0f * proj.scale, Color{ 255, 125, 30, 240 }, BLANK);
                break;
            }
            case ProjectileKind::PlayerConflux:
            case ProjectileKind::PlayerPaladinLightning:
            case ProjectileKind::PlayerAncestorBlade: {
                float r = proj.radius * proj.scale * 3.5f;
                DrawCircleGradient(static_cast<int>(proj.pos.x), static_cast<int>(proj.pos.y), r, Color{ 120, 210, 255, 220 }, BLANK);
                break;
            }
            case ProjectileKind::BossAcidDrop: {
                DrawCircleGradient(static_cast<int>(proj.pos.x), static_cast<int>(proj.pos.y), 70.0f, Color{ 130, 255, 90, 200 }, BLANK);
                break;
            }
            case ProjectileKind::EnemyEyeBolt:
            case ProjectileKind::BossSwordWave: {
                DrawCircleGradient(static_cast<int>(proj.pos.x), static_cast<int>(proj.pos.y), 65.0f, Color{ 245, 70, 90, 200 }, BLANK);
                break;
            }
            default:
                break;
        }
    }

    // 5. Світло від вогняних та магічних ворогів
    for (const auto& en : enemies) {
        if (!en.active) continue;
        if (en.type == EnemyType::FireMage) {
            DrawCircleGradient(static_cast<int>(en.pos.x), static_cast<int>(en.pos.y - 15.0f), 85.0f, Color{ 255, 140, 40, 185 }, BLANK);
        } else if (en.type == EnemyType::FloatingEye) {
            DrawCircleGradient(static_cast<int>(en.pos.x), static_cast<int>(en.pos.y), 65.0f, Color{ 220, 60, 90, 160 }, BLANK);
        }
    }

    // 6. Світло від активного боса
    if (hasActiveBoss && activeBoss.active) {
        DrawCircleGradient(static_cast<int>(activeBoss.pos.x), static_cast<int>(activeBoss.pos.y), 190.0f, Color{ 220, 160, 255, 180 }, BLANK);
    }
}

void Room::DrawEmissive([[maybe_unused]] const GameContext& ctx, const Player& player) const {
    const auto& pal = GetPalette();

    // 1. Полум'я смолоскипів та руни порталів
    for (const auto& it : interactables) {
        if (it.active && it.kind == InteractableKind::Torch) {
            Vector2 c = it.GetCenter();
            float flicker = std::sin(it.animTimer * 16.0f) * 2.5f;
            DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y - 12.0f), 28.0f + flicker, Fade(pal.fireGold[1], 0.35f), BLANK);
            float fH = 14.0f + std::sin(it.animTimer * 20.0f) * 3.0f;
            DrawTriangle({ c.x - 7.0f, c.y - 5.0f }, { c.x + 7.0f, c.y - 5.0f }, { c.x, c.y - 5.0f - fH }, pal.fireGold[0]);
            DrawTriangle({ c.x - 5.0f, c.y - 5.0f }, { c.x + 5.0f, c.y - 5.0f }, { c.x, c.y - 5.0f - fH * 0.75f }, pal.fireGold[2]);
            DrawTriangle({ c.x - 2.5f, c.y - 5.0f }, { c.x + 2.5f, c.y - 5.0f }, { c.x, c.y - 5.0f - fH * 0.45f }, pal.fireGold[4]);
            DrawCircleV({ c.x, c.y - 5.0f - fH * 0.25f }, 2.0f, pal.fireGold[4]);
        } else if (it.active && it.kind == InteractableKind::Teleporter) {
            Vector2 c = it.GetCenter();
            float pulse = std::sin(it.animTimer * 6.0f) * 3.0f;
            DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(it.pos.y - 20.0f), 28.0f + pulse, Fade(pal.moonMagic[2], 0.65f), BLANK);
        }
    }

    // 2. Очі ворогів та магічні сфери
    for (const auto& en : enemies) {
        if (!en.active) continue;
        if (en.type == EnemyType::FloatingEye) {
            Vector2 eyeCore = { en.pos.x + en.facingDir * 4.0f, en.pos.y - 1.0f };
            DrawCircleV(eyeCore, 3.5f, pal.danger[3]);
            DrawCircleV(eyeCore, 1.5f, Color{ 255, 255, 230, 255 });
        } else if (en.type == EnemyType::Skeleton) {
            DrawCircle(static_cast<int>(en.pos.x - 3.0f + en.facingDir * 2.0f), static_cast<int>(en.pos.y - 32.0f), 2.0f, pal.fireGold[3]);
            DrawCircle(static_cast<int>(en.pos.x + 3.0f + en.facingDir * 2.0f), static_cast<int>(en.pos.y - 32.0f), 2.0f, pal.fireGold[3]);
        } else if (en.type == EnemyType::FireMage) {
            Vector2 staffTip = { en.pos.x + en.facingDir * 16.0f, en.pos.y - 22.0f };
            DrawCircleV(staffTip, 7.0f, pal.fireGold[2]);
            DrawCircleV(staffTip, 3.5f, pal.fireGold[4]);
        }
    }

    // 3. Емісивні леза та магія героя під час атаки
    if (player.isAttacking) {
        Rectangle swBox = player.GetSwordHitbox();
        Vector2 swCenter = { swBox.x + swBox.width * 0.5f, swBox.y + swBox.height * 0.5f };
        DrawCircleGradient(static_cast<int>(swCenter.x), static_cast<int>(swCenter.y), 36.0f, Fade(pal.moonMagic[3], 0.75f), BLANK);
    }

    // 4. Сяючі ядра снарядів
    for (const auto& proj : projectiles) {
        if (!proj.active) continue;
        switch (proj.kind) {
            case ProjectileKind::PlayerFlameOrb:
            case ProjectileKind::PlayerDragonFireball:
            case ProjectileKind::BossFlameTrail: {
                DrawCircleV(proj.pos, proj.radius * proj.scale * 1.15f, Fade(pal.fireGold[3], 0.85f));
                DrawCircleV(proj.pos, proj.radius * proj.scale * 0.55f, pal.fireGold[4]);
                break;
            }
            case ProjectileKind::PlayerConflux:
            case ProjectileKind::PlayerPaladinLightning:
            case ProjectileKind::PlayerAncestorBlade: {
                DrawCircleV(proj.pos, proj.radius * proj.scale * 1.15f, Fade(pal.moonMagic[3], 0.85f));
                DrawCircleV(proj.pos, proj.radius * proj.scale * 0.55f, pal.moonMagic[4]);
                break;
            }
            case ProjectileKind::BossAcidDrop: {
                DrawCircleV(proj.pos, proj.radius * proj.scale * 0.95f, Fade(pal.poisonSlime[3], 0.8f));
                break;
            }
            default:
                break;
        }
    }
}

void Room::DrawWorldUi([[maybe_unused]] const GameContext& ctx, const Player& player, const Camera2D& cam) const {
    // 1. Підказка клавіші [W]/[E] біля скринь, порталів та дверей боса у повній роздільності 1080p
    for (const auto& it : interactables) {
        if (it.active && CheckCollisionRecs(it.GetBounds(), player.GetBounds())) {
            if ((it.kind == InteractableKind::Chest && !it.isOpened) ||
                (it.kind == InteractableKind::FairyChest && !it.isOpened && !it.isLocked) ||
                it.kind == InteractableKind::Teleporter ||
                it.kind == InteractableKind::BossDoor) {
                Vector2 hc = { it.GetCenter().x, it.pos.y - 24.0f };
                Vector2 sPos = GetWorldToScreen2D(hc, cam);
                int tw = Ui::Measure("[W / E]", 18);
                DrawRectangleRounded({ sPos.x - tw * 0.5f - 6.0f, sPos.y - 3.0f, static_cast<float>(tw + 12), 24.0f }, 0.35f, 4, Fade(BLACK, 0.75f));
                DrawRectangleRoundedLines({ sPos.x - tw * 0.5f - 6.0f, sPos.y - 3.0f, static_cast<float>(tw + 12), 24.0f }, 0.35f, 4, 1.5f, GOLD);
                Ui::Text("[W / E]", static_cast<int>(sPos.x - tw * 0.5f), static_cast<int>(sPos.y), 18, GOLD);
            }
        }
    }

    // 2. Банер умови Скрині Феї у повній роздільності 1080p
    if (roomType == RoomType::FairyChest && !chestOpened) {
        std::string banner = Registry::GetFairyConditionDesc(fairyCond);
        if (fairyCond == FairyCondition::TimeLimit && fairyTrialActive) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), " (%.1fs)", fairyTimer);
            banner += buf;
        }
        Color bCol = fairyTrialFailed ? RED : (fairyTrialCompleted ? LIME : SKYBLUE);
        int bw = Ui::Measure(banner, 20);
        int bx = (Config::RENDER_WIDTH - bw) / 2;
        DrawRectangle(bx - 16, 56, bw + 32, 34, Fade(BLACK, 0.85f));
        DrawRectangleLines(bx - 16, 56, bw + 32, 34, bCol);
        Ui::Text(banner, bx, 63, 20, bCol);
    }
}