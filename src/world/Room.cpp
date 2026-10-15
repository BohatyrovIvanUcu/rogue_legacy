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
    if (row < 0) {
        int rx = std::clamp(col / Config::CELL_COLS, 0, widthCells - 1);
        int localCol = col % Config::CELL_COLS;
        if (localCol < 0) localCol += Config::CELL_COLS;
        if ((subCellDoors[0][rx] & DoorMask::NORTH) && localCol >= 10 && localCol <= 14) return '.';
        return '#';
    }
    if (row >= totalRows) {
        int rx = std::clamp(col / Config::CELL_COLS, 0, widthCells - 1);
        int localCol = col % Config::CELL_COLS;
        if (localCol < 0) localCol += Config::CELL_COLS;
        if ((subCellDoors[heightCells - 1][rx] & DoorMask::SOUTH) && localCol >= 10 && localCol <= 14) return '.';
        return '#';
    }
    if (col < 0) {
        int ry = std::clamp(row / Config::CELL_ROWS, 0, heightCells - 1);
        int localRow = row % Config::CELL_ROWS;
        if (localRow < 0) localRow += Config::CELL_ROWS;
        if ((subCellDoors[ry][0] & DoorMask::WEST) && localRow >= 5 && localRow <= 9) return '.';
        return '#';
    }
    if (col >= totalCols) {
        int ry = std::clamp(row / Config::CELL_ROWS, 0, heightCells - 1);
        int localRow = row % Config::CELL_ROWS;
        if (localRow < 0) localRow += Config::CELL_ROWS;
        if ((subCellDoors[ry][widthCells - 1] & DoorMask::EAST) && localRow >= 5 && localRow <= 9) return '.';
        return '#';
    }

    if (static_cast<size_t>(row) < tiles.size() && static_cast<size_t>(col) < tiles[static_cast<size_t>(row)].size()) {
        return tiles[static_cast<size_t>(row)][static_cast<size_t>(col)];
    }
    return '#';
}

void Room::BuildFromNode(
    int gx,
    int gy,
    const SavedRoomNode& node,
    int calculatedRoomLevel,
    const GameContext& ctx
) {
    roomId = node.roomId;
    originGx = node.originGx != 0 ? node.originGx : gx;
    originGy = node.originGy != 0 ? node.originGy : gy;
    gridX = originGx;
    gridY = originGy;

    roomType = node.roomType;
    biome = node.biome;
    templateIdx = std::clamp(node.templateIdx, 0, static_cast<int>(RoomTemplates::TEMPLATES.size()) - 1);
    isReversed = node.isReversed;
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
    widthCells = tmpl.widthCells;
    heightCells = tmpl.heightCells;
    totalCols = widthCells * Config::CELL_COLS;
    totalRows = heightCells * Config::CELL_ROWS;

    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 2; ++x) {
            subCellDoors[y][x] = node.subCellDoors[y][x];
        }
    }
    doorMask = node.doorMask;
    if (doorMask == 0) {
        doorMask = subCellDoors[0][0];
    }

    tiles.resize(static_cast<size_t>(totalRows));
    for (int r = 0; r < totalRows; ++r) {
        tiles[static_cast<size_t>(r)].resize(static_cast<size_t>(totalCols), '.');
        if (r < static_cast<int>(tmpl.rows.size())) {
            std::string_view line = tmpl.rows[static_cast<size_t>(r)];
            for (int c = 0; c < totalCols; ++c) {
                int srcCol = isReversed ? (totalCols - 1 - c) : c;
                char ch = (srcCol >= 0 && srcCol < static_cast<int>(line.size())) ? line[static_cast<size_t>(srcCol)] : '.';
                if (isReversed) {
                    if (ch == '/') ch = '\\';
                    else if (ch == '\\') ch = '/';
                }
                tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = ch;
            }
        }
    }

    ApplyDoorMasksToTiles();
    SpawnEntitiesFromTemplate(ctx);
}

void Room::ApplyDoorMasksToTiles() {
    if (isBossArenaActive) return;

    for (int ry = 0; ry < heightCells; ++ry) {
        for (int rx = 0; rx < widthCells; ++rx) {
            uint8_t mask = subCellDoors[ry][rx];

            // 1. NORTH (рядок 0, тільки для верхнього ярусу ry == 0)
            if (ry == 0) {
                for (int c = rx * Config::CELL_COLS + 11; c <= rx * Config::CELL_COLS + 13; ++c) {
                    tiles[0][static_cast<size_t>(c)] = (mask & DoorMask::NORTH) ? '.' : '#';
                }
                if (mask & DoorMask::NORTH) {
                    for (int c = rx * Config::CELL_COLS + 10; c <= rx * Config::CELL_COLS + 14; ++c) {
                        if (tiles[3][static_cast<size_t>(c)] == '.') {
                            tiles[3][static_cast<size_t>(c)] = '=';
                        }
                    }
                }
            }

            // 2. SOUTH (рядок totalRows - 1, тільки для нижнього ярусу ry == heightCells - 1)
            if (ry == heightCells - 1) {
                for (int c = rx * Config::CELL_COLS + 11; c <= rx * Config::CELL_COLS + 13; ++c) {
                    tiles[static_cast<size_t>(totalRows - 1)][static_cast<size_t>(c)] = (mask & DoorMask::SOUTH) ? '=' : '#';
                    if (mask & DoorMask::SOUTH) {
                        if (tiles[static_cast<size_t>(totalRows - 2)][static_cast<size_t>(c)] == '^' ||
                            tiles[static_cast<size_t>(totalRows - 2)][static_cast<size_t>(c)] == '~') {
                            tiles[static_cast<size_t>(totalRows - 2)][static_cast<size_t>(c)] = '.';
                        }
                    }
                }
            }

            // 3. WEST (стовпець 0, тільки для лівого сектору rx == 0)
            if (rx == 0) {
                for (int r = ry * Config::CELL_ROWS + 6; r <= ry * Config::CELL_ROWS + 8; ++r) {
                    tiles[static_cast<size_t>(r)][0] = (mask & DoorMask::WEST) ? '.' : '#';
                }
            }

            // 4. EAST (стовпець totalCols - 1, тільки для правого сектору rx == widthCells - 1)
            if (rx == widthCells - 1) {
                for (int r = ry * Config::CELL_ROWS + 6; r <= ry * Config::CELL_ROWS + 8; ++r) {
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(totalCols - 1)] = (mask & DoorMask::EAST) ? '.' : '#';
                }
            }
        }
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

    for (int r = 0; r < totalRows; ++r) {
        for (int c = 0; c < totalCols; ++c) {
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
                case 'H': {
                    interactables.push_back(Interactable::MakeShrine(bottomCenter));
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    break;
                }
                case 'K': {
                    interactables.push_back(Interactable::MakeStatue(bottomCenter));
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    break;
                }
                case 'T': {
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    if (!cleared) {
                        Enemy turret;
                        turret.Init(EnemyType::WallTurret, enemyTier, roomLevel, center, false);
                        turret.facingDir = (c < totalCols / 2) ? 1.0f : -1.0f;
                        enemies.push_back(turret);
                    }
                    break;
                }
                case 'S': {
                    tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
                    if (!cleared) {
                        EnemyType eType = EnemyType::Skeleton;
                        int roll = (c * 13 + r * 7 + originGx * 5 + originGy * 3) % 5;
                        if (roll == 0)      eType = EnemyType::Skeleton;
                        else if (roll == 1) eType = EnemyType::FloatingEye;
                        else if (roll == 2) eType = EnemyType::ShieldKnight;
                        else if (roll == 3) eType = EnemyType::FireMage;
                        else                eType = EnemyType::Warg;

                        Enemy en;
                        en.Init(eType, enemyTier, roomLevel, { center.x, bottomCenter.y - 24.0f }, false);
                        if (isReversed) {
                            en.facingDir = -1.0f;
                        }
                        enemies.push_back(en);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    // У кімнаті BossEntrance гарантовано спавнимо:
    // 1. Двері до Боса біому (по центру)
    // 2. Портал швидкого переміщення (Teleporter) ліворуч, щоб гравець міг телепортуватися сюди
    // 3. Вівтар Молитви / Скриньку пожертви (Shrine / Donation Box) праворуч
    if (roomType == RoomType::BossEntrance && !isBossArenaActive) {
        float centerX = static_cast<float>(Config::ROOM_OFFSET_X) + static_cast<float>(totalCols) * static_cast<float>(Config::TILE_SIZE) * 0.5f;
        float bottomY = static_cast<float>(Config::ROOM_OFFSET_Y) + static_cast<float>(totalRows - 3) * static_cast<float>(Config::TILE_SIZE);
        Vector2 doorPos = { centerX, bottomY };
        interactables.push_back(Interactable::MakeBossDoor(doorPos, bossType));

        bool hasTeleporter = false;
        bool hasShrine = false;
        for (const auto& it : interactables) {
            if (it.kind == InteractableKind::Teleporter) hasTeleporter = true;
            if (it.kind == InteractableKind::Shrine)     hasShrine = true;
        }

        if (!hasTeleporter) {
            Vector2 telPos = { centerX - 7.0f * Config::TILE_SIZE, bottomY + Config::TILE_SIZE };
            interactables.push_back(Interactable::MakeTeleporter(telPos));
        }

        if (!hasShrine) {
            Vector2 shrinePos = { centerX + 7.0f * Config::TILE_SIZE, bottomY + Config::TILE_SIZE };
            interactables.push_back(Interactable::MakeShrine(shrinePos));
        }
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

    widthCells = 1;
    heightCells = 1;
    totalCols = Config::CELL_COLS;
    totalRows = Config::CELL_ROWS;
    subCellDoors[0][0] = 0; subCellDoors[0][1] = 0; subCellDoors[1][0] = 0; subCellDoors[1][1] = 0;
    doorMask = 0;

    int arenaTemplateIdx = 28 + std::clamp(static_cast<int>(targetBoss), 0, 4);
    const auto& tmpl = RoomTemplates::TEMPLATES[static_cast<size_t>(arenaTemplateIdx)];
    tiles.resize(static_cast<size_t>(Config::CELL_ROWS));
    for (int r = 0; r < Config::CELL_ROWS; ++r) {
        tiles[static_cast<size_t>(r)].resize(static_cast<size_t>(Config::CELL_COLS), '#');
        std::string_view line = tmpl.rows[static_cast<size_t>(r)];
        for (int c = 0; c < Config::CELL_COLS; ++c) {
            tiles[static_cast<size_t>(r)][static_cast<size_t>(c)] =
                (c < static_cast<int>(line.size())) ? line[static_cast<size_t>(c)] : '#';
        }
    }

    enemies.clear();
    interactables.clear();
    projectiles.clear();

    // Розставляємо факели/бочки з шаблону арени
    for (int r = 0; r < Config::CELL_ROWS; ++r) {
        for (int c = 0; c < Config::CELL_COLS; ++c) {
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
                float shoutKbMult = 1.0f;
                if (ctx.CurrentHeirHasTrait(TraitType::Hypergonadism)) shoutKbMult = 2.0f;
                else if (ctx.CurrentHeirHasTrait(TraitType::MuscleWk)) shoutKbMult = 0.0f;
                Vector2 kb = { (d.x / len) * shout.knockbackSpeed * shoutKbMult, -240.0f * (shoutKbMult > 0.0f ? 1.0f : 0.0f) };
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

    // Перевірка входу у Золоті Двері Фінального Боса у кімнаті Entrance
    if (roomType == RoomType::Entrance && !isBossArenaActive) {
        Rectangle goldenDoorRec = { GetPixelWidth() * 0.5f - 58.0f, 9.0f * Config::TILE_SIZE - 168.0f, 116.0f, 168.0f };
        if (CheckCollisionRecs(player.GetBounds(), goldenDoorRec)) {
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_E) || IsKeyPressed(KEY_UP)) {
                bool allFourSlain = ctx.bossDefeated[0] && ctx.bossDefeated[1] &&
                                    ctx.bossDefeated[2] && ctx.bossDefeated[3];
                if (allFourSlain) {
                    requestEnterBossArena = true;
                    requestedBossArenaType = BossType::JohannesTraitor;
                } else if (ctx.particles) {
                    ctx.particles->SpawnFloatingText(
                        { GetPixelWidth() * 0.5f, goldenDoorRec.y + 40.0f },
                        "DEFEAT ALL 4 GUARDIANS TO UNSEAL!",
                        GOLD, 20, false
                    );
                }
            }
        }
    }

    for (const auto& drop : newlySpawnedDrops) {
        if (drop.kind == InteractableKind::MeatDrop && ctx.CurrentHeirHasTrait(TraitType::Alektorophobia)) {
            Enemy chicken;
            chicken.Init(EnemyType::ChickenKentucky, 1, roomLevel, drop.GetCenter(), false);
            enemies.push_back(chicken);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChickenCluck, 1.0f, 0.85f);
        } else {
            interactables.push_back(drop);
        }
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

    // Риса Clumsy: будь-який тілесний контакт із розбивними предметами руйнує їх
    if (ctx.CurrentHeirHasTrait(TraitType::Clumsy)) {
        Rectangle pBounds = player.GetBounds();
        for (auto& it : interactables) {
            if (!it.active) continue;
            if (it.kind == InteractableKind::BreakableProp || it.kind == InteractableKind::Torch) {
                if (CheckCollisionRecs(pBounds, it.GetBounds())) {
                    it.OnHitByPlayer(ctx, player, combatDrops);
                }
            }
        }
    }

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

            float kbMult = 1.0f;
            if (ctx.CurrentHeirHasTrait(TraitType::Hypergonadism)) kbMult = 2.0f;
            else if (ctx.CurrentHeirHasTrait(TraitType::MuscleWk)) kbMult = 0.0f;

            float kbDir = (en.GetCenter().x >= player.GetCenter().x) ? 1.0f : -1.0f;
            Vector2 kbVel = { kbDir * 210.0f * kbMult, -150.0f * (kbMult > 0.0f ? 1.0f : 0.0f) };

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
                if (en.type == EnemyType::ChickenKentucky) {
                    Interactable meat;
                    meat.kind = InteractableKind::MeatDrop;
                    meat.width = 24.0f;
                    meat.height = 18.0f;
                    meat.SetCenter(en.GetCenter());
                    meat.vel = { 0.0f, -220.0f };
                    meat.pickupDelay = 0.18f;
                    combatDrops.push_back(meat);
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChickenCluck, 1.35f, 0.9f);
                }
                player.OnEnemyKilled(ctx, en.type);
                if (!en.isDementiaPhantom && en.type != EnemyType::ChickenKentucky) {
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

            float kbMult = 1.0f;
            if (ctx.CurrentHeirHasTrait(TraitType::Hypergonadism)) kbMult = 2.0f;
            else if (ctx.CurrentHeirHasTrait(TraitType::MuscleWk)) kbMult = 0.0f;

            float kbDir = (en.GetCenter().x >= proj.pos.x) ? 1.0f : -1.0f;
            bool killed = en.TakeDamage(proj.damage, { kbDir * 140.0f * kbMult, -100.0f * (kbMult > 0.0f ? 1.0f : 0.0f) }, false, ctx);
            if (killed) {
                if (en.type == EnemyType::ChickenKentucky) {
                    Interactable meat;
                    meat.kind = InteractableKind::MeatDrop;
                    meat.width = 24.0f;
                    meat.height = 18.0f;
                    meat.SetCenter(en.GetCenter());
                    meat.vel = { 0.0f, -220.0f };
                    meat.pickupDelay = 0.18f;
                    combatDrops.push_back(meat);
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChickenCluck, 1.35f, 0.9f);
                }
                player.OnEnemyKilled(ctx, en.type);
                if (!en.isDementiaPhantom && en.type != EnemyType::ChickenKentucky) {
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
        if (d.kind == InteractableKind::MeatDrop && ctx.CurrentHeirHasTrait(TraitType::Alektorophobia)) {
            Enemy chicken;
            chicken.Init(EnemyType::ChickenKentucky, 1, roomLevel, d.GetCenter(), false);
            enemies.push_back(chicken);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChickenCluck, 1.0f, 0.85f);
        } else {
            interactables.push_back(d);
        }
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

    // 1. Фон біому (з урахуванням розмірів багатоклітинних кімнат)
    ctx.renderer->DrawBiomeBackground(biome, originGx, originGy, GetPixelWidth(), GetPixelHeight());

    // 2. Золоті Двері Фінального Боса у кімнаті Entrance (на центральному постаменті)
    if (roomType == RoomType::Entrance && !isBossArenaActive) {
        Vector2 goldenDoorBottom = {
            GetPixelWidth() * 0.5f,
            static_cast<float>(9 * Config::TILE_SIZE)
        };
        ctx.renderer->DrawGoldenBossDoor(goldenDoorBottom, ctx.bossDefeated, trapTimer);

        // 2.5. Закрита зовнішня брама на лівому виступі (звідки герой прийшов у замок)
        float gateX = static_cast<float>(Config::ROOM_OFFSET_X);
        float gateY = static_cast<float>(Config::ROOM_OFFSET_Y + 9 * Config::TILE_SIZE);
        DrawRectangle(static_cast<int>(gateX), static_cast<int>(gateY - 180.0f), 24, 180, Color{ 20, 24, 30, 255 });
        for (int barX = static_cast<int>(gateX) + 4; barX < static_cast<int>(gateX) + 24; barX += 5) {
            DrawRectangle(barX, static_cast<int>(gateY - 176.0f), 3, 176, Color{ 90, 96, 110, 255 });
            DrawRectangle(barX + 1, static_cast<int>(gateY - 176.0f), 1, 176, Color{ 140, 148, 165, 255 });
        }
        for (float hy = gateY - 160.0f; hy < gateY; hy += 40.0f) {
            DrawRectangle(static_cast<int>(gateX), static_cast<int>(hy), 24, 6, Color{ 60, 65, 75, 255 });
            DrawRectangle(static_cast<int>(gateX + 2), static_cast<int>(hy + 1), 20, 2, Color{ 110, 118, 132, 255 });
            DrawCircle(static_cast<int>(gateX + 6), static_cast<int>(hy + 3), 2.0f, Color{ 180, 190, 205, 255 });
            DrawCircle(static_cast<int>(gateX + 18), static_cast<int>(hy + 3), 2.0f, Color{ 180, 190, 205, 255 });
        }
        // Замок, що блокує вихід назад
        DrawRectangle(static_cast<int>(gateX + 5), static_cast<int>(gateY - 95.0f), 14, 18, Color{ 185, 150, 45, 255 });
        DrawRectangleLines(static_cast<int>(gateX + 5), static_cast<int>(gateY - 95.0f), 14, 18, Color{ 95, 70, 20, 255 });
        DrawCircle(static_cast<int>(gateX + 12), static_cast<int>(gateY - 86.0f), 2.0f, Color{ 50, 35, 10, 255 });

        // 2.6. Басейн із водою у лівій ямі вхідної кімнати (колонки 10..15, рядок 13)
        float poolX = static_cast<float>(Config::ROOM_OFFSET_X + 10 * Config::TILE_SIZE);
        float poolY = static_cast<float>(Config::ROOM_OFFSET_Y + 13 * Config::TILE_SIZE + 18);
        float poolW = 5.0f * Config::TILE_SIZE;
        float poolH = 30.0f;
        DrawRectangle(static_cast<int>(poolX - 4.0f), static_cast<int>(poolY - 4.0f), static_cast<int>(poolW + 8.0f), static_cast<int>(poolH + 4.0f), Color{ 58, 62, 70, 255 });
        DrawRectangleLines(static_cast<int>(poolX - 4.0f), static_cast<int>(poolY - 4.0f), static_cast<int>(poolW + 8.0f), static_cast<int>(poolH + 4.0f), Color{ 110, 115, 128, 255 });
        DrawRectangle(static_cast<int>(poolX), static_cast<int>(poolY), static_cast<int>(poolW), static_cast<int>(poolH), Color{ 24, 115, 150, 180 });
        DrawRectangle(static_cast<int>(poolX), static_cast<int>(poolY + 12.0f), static_cast<int>(poolW), static_cast<int>(poolH - 12.0f), Color{ 14, 75, 110, 210 });
        for (float wx = poolX; wx < poolX + poolW - 8.0f; wx += 14.0f) {
            float waveY = poolY + 1.0f + std::sin(trapTimer * 4.0f + wx * 0.12f) * 2.2f;
            DrawLineEx({ wx, waveY }, { wx + 10.0f, waveY }, 1.5f, Color{ 160, 230, 255, 190 });
        }

        // Настінний геральдичний банер "CASTLE HAMSON" на задній стіні над басейном
        float banX = poolX + poolW * 0.5f;
        float banY = static_cast<float>(Config::ROOM_OFFSET_Y + 11 * Config::TILE_SIZE - 10.0f);
        DrawRectangle(static_cast<int>(banX - 95.0f), static_cast<int>(banY), 190, 26, Color{ 22, 28, 38, 230 });
        DrawRectangleLinesEx({ banX - 95.0f, banY, 190.0f, 26.0f }, 2.0f, Color{ 210, 175, 55, 255 });
        DrawCircle(static_cast<int>(banX - 93.0f), static_cast<int>(banY + 2.0f), 3.0f, Color{ 240, 205, 75, 255 });
        DrawCircle(static_cast<int>(banX + 93.0f), static_cast<int>(banY + 2.0f), 3.0f, Color{ 240, 205, 75, 255 });
        Ui::Text("CASTLE HAMSON", static_cast<int>(banX - 68.0f), static_cast<int>(banY + 6.0f), 14, Color{ 240, 215, 120, 255 });

        // 2.7. Кабінет картографа/дослідника у правому крилі стартової кімнати (колонки 33..41, рядок 12..13)
        float studyX = static_cast<float>(Config::ROOM_OFFSET_X + 33 * Config::TILE_SIZE);
        float studyY = static_cast<float>(Config::ROOM_OFFSET_Y + 13 * Config::TILE_SIZE);

        DrawRectangle(static_cast<int>(studyX + 10.0f), static_cast<int>(studyY - 130.0f), 150, 95, Color{ 65, 45, 30, 255 });
        DrawRectangleLinesEx({ studyX + 10.0f, studyY - 130.0f, 150.0f, 95.0f }, 3.0f, Color{ 145, 105, 55, 255 });
        DrawRectangle(static_cast<int>(studyX + 16.0f), static_cast<int>(studyY - 124.0f), 138, 83, Color{ 215, 198, 160, 240 });
        DrawRectangleLines(static_cast<int>(studyX + 24.0f), static_cast<int>(studyY - 116.0f), 45, 30, Color{ 110, 80, 50, 200 });
        DrawRectangleLines(static_cast<int>(studyX + 75.0f), static_cast<int>(studyY - 116.0f), 45, 30, Color{ 110, 80, 50, 200 });
        DrawRectangleLines(static_cast<int>(studyX + 50.0f), static_cast<int>(studyY - 80.0f), 45, 30, Color{ 110, 80, 50, 200 });
        DrawLine(static_cast<int>(studyX + 69.0f), static_cast<int>(studyY - 101.0f), static_cast<int>(studyX + 75.0f), static_cast<int>(studyY - 101.0f), Color{ 180, 40, 40, 255 });
        DrawCircle(static_cast<int>(studyX + 46.0f), static_cast<int>(studyY - 101.0f), 2.5f, Color{ 220, 30, 30, 255 });
        DrawCircle(static_cast<int>(studyX + 97.0f), static_cast<int>(studyY - 101.0f), 2.5f, Color{ 220, 30, 30, 255 });
        DrawCircle(static_cast<int>(studyX + 72.0f), static_cast<int>(studyY - 65.0f), 2.5f, Color{ 220, 30, 30, 255 });

        // Стіл картографа з фоліантами та пером
        DrawRectangle(static_cast<int>(studyX + 175.0f), static_cast<int>(studyY - 26.0f), 70, 8, Color{ 85, 55, 35, 255 });
        DrawRectangle(static_cast<int>(studyX + 180.0f), static_cast<int>(studyY - 18.0f), 6, 18, Color{ 65, 40, 25, 255 });
        DrawRectangle(static_cast<int>(studyX + 233.0f), static_cast<int>(studyY - 18.0f), 6, 18, Color{ 65, 40, 25, 255 });
        DrawRectangle(static_cast<int>(studyX + 184.0f), static_cast<int>(studyY - 33.0f), 22, 7, Color{ 140, 35, 35, 255 });
        DrawRectangle(static_cast<int>(studyX + 186.0f), static_cast<int>(studyY - 38.0f), 19, 5, Color{ 35, 95, 140, 255 });
        DrawRectangle(static_cast<int>(studyX + 212.0f), static_cast<int>(studyY - 30.0f), 24, 4, Color{ 225, 215, 180, 255 });
        DrawRectangle(static_cast<int>(studyX + 238.0f), static_cast<int>(studyY - 32.0f), 5, 6, Color{ 20, 20, 25, 255 });
        DrawLine(static_cast<int>(studyX + 240.0f), static_cast<int>(studyY - 32.0f), static_cast<int>(studyX + 245.0f), static_cast<int>(studyY - 42.0f), Color{ 230, 230, 235, 255 });
    }

    // 2.8. Багатий інтер'єрний декор кімнат (Bookcases, Fireplaces & Portraits, Chandeliers)
    if (!isBossArenaActive) {
        const auto& pal = GetPalette();
        for (int r = 1; r < totalRows - 1; ++r) {
            for (int c = 1; c < totalCols - 1; ++c) {
                char cur = tiles[static_cast<size_t>(r)][static_cast<size_t>(c)];
                if (cur != '.') continue;

                int wx = Config::ROOM_OFFSET_X + c * Config::TILE_SIZE;
                int wy = Config::ROOM_OFFSET_Y + r * Config::TILE_SIZE;
                uint32_t h = static_cast<uint32_t>((c + originGx) * 31 + (r + originGy) * 53);

                // 1) Підвісні ковані люстри зі свічками (Ceiling Chandeliers)
                if (tiles[static_cast<size_t>(r - 1)][static_cast<size_t>(c)] == '#' &&
                    tiles[static_cast<size_t>(r + 1)][static_cast<size_t>(c)] == '.' &&
                    (h % 13 == 0) && (roomType != RoomType::Entrance || c < 10 || c > 30)) {
                    float chX = static_cast<float>(wx + Config::TILE_SIZE / 2);
                    float chY = static_cast<float>(wy);
                    DrawLineEx({ chX, chY }, { chX, chY + 16.0f }, 2.0f, pal.metal[2]);
                    DrawLineEx({ chX - 16.0f, chY + 22.0f }, { chX + 16.0f, chY + 22.0f }, 3.0f, pal.metal[3]);
                    DrawCircle(static_cast<int>(chX), static_cast<int>(chY + 24.0f), 3.5f, pal.metal[4]);
                    for (int ci = -1; ci <= 1; ++ci) {
                        float cx = chX + ci * 12.0f;
                        DrawRectangle(static_cast<int>(cx - 1.5f), static_cast<int>(chY + 14.0f), 3, 8, pal.bone[4]);
                        float flk = std::sin(trapTimer * 8.0f + cx * 2.0f) * 1.5f;
                        DrawCircle(static_cast<int>(cx), static_cast<int>(chY + 12.0f), 2.5f + flk * 0.4f, pal.fireGold[3]);
                        DrawCircle(static_cast<int>(cx), static_cast<int>(chY + 12.0f), 1.2f, WHITE);
                    }
                }

                // 2) Книжкові шафи (Bookcases) на твердій підлозі біля стіни
                if (tiles[static_cast<size_t>(r + 1)][static_cast<size_t>(c)] == '#' &&
                    r >= 2 && tiles[static_cast<size_t>(r - 1)][static_cast<size_t>(c)] == '.' &&
                    (h % 17 == 0) && (roomType != RoomType::Entrance || c > 32)) {
                    float bkX = static_cast<float>(wx);
                    float bkY = static_cast<float>(wy + Config::TILE_SIZE - 74.0f);
                    DrawRectangle(static_cast<int>(bkX + 4.0f), static_cast<int>(bkY), 40, 74, Color{ 52, 34, 24, 255 });
                    DrawRectangleLines(static_cast<int>(bkX + 4.0f), static_cast<int>(bkY), 40, 74, Color{ 90, 60, 42, 255 });
                    for (int sh = 0; sh < 3; ++sh) {
                        float shY = bkY + 6.0f + sh * 22.0f;
                        DrawLine(static_cast<int>(bkX + 6.0f), static_cast<int>(shY + 18.0f), static_cast<int>(bkX + 42.0f), static_cast<int>(shY + 18.0f), Color{ 90, 60, 42, 255 });
                        for (int bk = 0; bk < 5; ++bk) {
                            Color bCol = (bk % 3 == 0) ? pal.bloodCloth[1] : ((bk % 3 == 1) ? Color{ 32, 90, 140, 255 } : Color{ 40, 120, 65, 255 });
                            DrawRectangle(static_cast<int>(bkX + 7.0f + bk * 7.0f), static_cast<int>(shY + 2.0f), 6, 16, bCol);
                            DrawLine(static_cast<int>(bkX + 7.0f + bk * 7.0f), static_cast<int>(shY + 4.0f), static_cast<int>(bkX + 13.0f + bk * 7.0f), static_cast<int>(shY + 4.0f), pal.fireGold[3]);
                        }
                    }
                }

                // 3) Кам'яний камін із тліючим вугіллям та королівським портретом у золотій рамі
                if (c + 1 < totalCols &&
                    tiles[static_cast<size_t>(r + 1)][static_cast<size_t>(c)] == '#' &&
                    tiles[static_cast<size_t>(r + 1)][static_cast<size_t>(c + 1)] == '#' &&
                    r >= 2 && tiles[static_cast<size_t>(r - 1)][static_cast<size_t>(c)] == '.' &&
                    (h % 29 == 0) && roomType != RoomType::Entrance) {
                    float fpX = static_cast<float>(wx);
                    float fpY = static_cast<float>(wy + Config::TILE_SIZE);
                    DrawRectangle(static_cast<int>(fpX + 4.0f), static_cast<int>(fpY - 48.0f), 68, 48, Color{ 74, 72, 70, 255 });
                    DrawRectangleLines(static_cast<int>(fpX + 4.0f), static_cast<int>(fpY - 48.0f), 68, 48, Color{ 110, 108, 105, 255 });
                    DrawRectangle(static_cast<int>(fpX + 16.0f), static_cast<int>(fpY - 32.0f), 44, 32, Color{ 18, 16, 18, 255 });
                    DrawCircle(static_cast<int>(fpX + 38.0f), static_cast<int>(fpY - 32.0f), 22.0f, Color{ 18, 16, 18, 255 });
                    float fireGlow = 0.75f + 0.25f * std::sin(trapTimer * 6.5f + fpX);
                    DrawCircle(static_cast<int>(fpX + 38.0f), static_cast<int>(fpY - 10.0f), 14.0f * fireGlow, Color{ 235, 95, 30, 210 });
                    DrawCircle(static_cast<int>(fpX + 38.0f), static_cast<int>(fpY - 10.0f), 7.0f, Color{ 255, 215, 80, 255 });

                    // Портрет предка у золотій рамі над каміном
                    DrawRectangle(static_cast<int>(fpX + 22.0f), static_cast<int>(fpY - 88.0f), 32, 36, Color{ 215, 175, 55, 255 });
                    DrawRectangleLines(static_cast<int>(fpX + 22.0f), static_cast<int>(fpY - 88.0f), 32, 36, Color{ 145, 110, 30, 255 });
                    DrawRectangle(static_cast<int>(fpX + 25.0f), static_cast<int>(fpY - 85.0f), 26, 30, Color{ 35, 25, 45, 255 });
                    DrawCircle(static_cast<int>(fpX + 38.0f), static_cast<int>(fpY - 74.0f), 6.5f, pal.fireGold[3]);
                    DrawTriangle({ fpX + 32.0f, fpY - 80.0f }, { fpX + 44.0f, fpY - 80.0f }, { fpX + 38.0f, fpY - 74.0f }, pal.fireGold[4]);
                    DrawRectangle(static_cast<int>(fpX + 33.0f), static_cast<int>(fpY - 67.0f), 10, 10, pal.metal[3]);
                }
            }
        }
    }

    // 3. Тайлова карта кімнати з процедурним автотайлінгом
    auto IsSolidTile = [this](int col, int row) -> bool {
        char ch = GetTileAt(col, row);
        return (ch == '#' || ch == 'X' || ch == 'I' || ch == '/' || ch == '\\');
    };

    for (int r = 0; r < totalRows; ++r) {
        for (int c = 0; c < totalCols; ++c) {
            char tc = tiles[static_cast<size_t>(r)][static_cast<size_t>(c)];
            if (tc == '.') {
                // Павутиння у внутрішніх кутах ніш стін (Cobwebs in concave corners)
                bool nSolid = IsSolidTile(c, r - 1);
                bool sSolid = IsSolidTile(c, r + 1);
                bool wSolid = IsSolidTile(c - 1, r);
                bool eSolid = IsSolidTile(c + 1, r);

                if ((nSolid && wSolid) || (nSolid && eSolid) || (sSolid && wSolid) || (sSolid && eSolid)) {
                    uint32_t wh = static_cast<uint32_t>(c * 37 + r * 59);
                    if ((wh % 3) == 0) {
                        int tx = Config::ROOM_OFFSET_X + c * Config::TILE_SIZE;
                        int ty = Config::ROOM_OFFSET_Y + r * Config::TILE_SIZE;
                        int ts = Config::TILE_SIZE;
                        Color webCol = ColorAlpha(GetPalette().bone[4], 0.25f);
                        if (nSolid && wSolid) {
                            DrawLine(tx, ty, tx + 16, ty + 3, webCol);
                            DrawLine(tx, ty, tx + 12, ty + 12, webCol);
                            DrawLine(tx, ty, tx + 3, ty + 16, webCol);
                            DrawLine(tx + 16, ty + 3, tx + 12, ty + 12, webCol);
                            DrawLine(tx + 12, ty + 12, tx + 3, ty + 16, webCol);
                        } else if (nSolid && eSolid) {
                            DrawLine(tx + ts, ty, tx + ts - 16, ty + 3, webCol);
                            DrawLine(tx + ts, ty, tx + ts - 12, ty + 12, webCol);
                            DrawLine(tx + ts, ty, tx + ts - 3, ty + 16, webCol);
                            DrawLine(tx + ts - 16, ty + 3, tx + ts - 12, ty + 12, webCol);
                            DrawLine(tx + ts - 12, ty + 12, tx + ts - 3, ty + 16, webCol);
                        }
                    }
                }
                continue;
            }

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

            bool hasEhs = ctx.CurrentHeirHasTrait(TraitType::EHS);
            ctx.renderer->DrawTile(tc, c, r, biome, trapTimer, illusionAlpha, nMask, dMask, hasEhs);
        }
    }

    // 4. Візуалізація відкритих дверей по зовнішніх краях багатоклітинної кімнати
    if (!isBossArenaActive) {
        const auto& pal = GetPalette();
        for (int ry = 0; ry < heightCells; ++ry) {
            for (int rx = 0; rx < widthCells; ++rx) {
                uint8_t mask = subCellDoors[ry][rx];
                if (rx == 0 && (mask & DoorMask::WEST)) {
                    DrawRectangle(Config::ROOM_OFFSET_X, (ry * Config::CELL_ROWS + 6) * Config::TILE_SIZE, 8, 3 * Config::TILE_SIZE, Fade(pal.fireGold[3], 0.28f));
                }
                if (rx == widthCells - 1 && (mask & DoorMask::EAST)) {
                    DrawRectangle(Config::ROOM_OFFSET_X + totalCols * Config::TILE_SIZE - 8, (ry * Config::CELL_ROWS + 6) * Config::TILE_SIZE, 8, 3 * Config::TILE_SIZE, Fade(pal.fireGold[3], 0.28f));
                }
                if (ry == 0 && (mask & DoorMask::NORTH)) {
                    DrawRectangle(Config::ROOM_OFFSET_X + (rx * Config::CELL_COLS + 11) * Config::TILE_SIZE, 0, 3 * Config::TILE_SIZE, 8, Fade(pal.fireGold[3], 0.28f));
                }
                if (ry == heightCells - 1 && (mask & DoorMask::SOUTH)) {
                    DrawRectangle(Config::ROOM_OFFSET_X + (rx * Config::CELL_COLS + 11) * Config::TILE_SIZE, totalRows * Config::TILE_SIZE - 8, 3 * Config::TILE_SIZE, 8, Fade(pal.fireGold[3], 0.28f));
                }
            }
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
        Vector2 doorCenter = { GetPixelWidth() * 0.5f, static_cast<float>(9 * Config::TILE_SIZE - 80.0f) };
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
        if (ctx.CurrentHeirHasTrait(TraitType::Dyslexia)) {
            banner = Ui::ScrambleDyslexia(banner);
        }
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