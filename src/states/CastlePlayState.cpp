// === FILE: src/states/CastlePlayState.cpp ===
#include "states/CastlePlayState.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ShaderManager.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/UiText.hpp"
#include <raylib.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

void CastlePlayState::OnEnter(GameContext& ctx) {
    if (ctx.particles) ctx.particles->Clear();
    if (ctx.audio)     ctx.audio->SetBossMusic(false);

    castle_.GenerateOrRestoreCastle(ctx);
    curGx_ = Config::ENTRANCE_GRID_X;
    curGy_ = Config::ENTRANCE_GRID_Y;
    teleporterMapOpen_ = false;

    Vector2 startPos = {
        static_cast<float>(Config::ROOM_OFFSET_X + 6 * Config::TILE_SIZE),
        static_cast<float>(Config::ROOM_OFFSET_Y + 12 * Config::TILE_SIZE)
    };
    player_.InitFromContext(ctx, startPos);

    Room& startRoom = castle_.GetRoom(curGx_, curGy_);
    startRoom.OnPlayerEnterRoom(ctx, player_);
    castle_.SyncRoomStateToContext(startRoom, ctx);
}

void CastlePlayState::OnExit(GameContext& ctx) {
    if (ctx.audio) ctx.audio->SetBossMusic(false);
    SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
}

void CastlePlayState::Update(float dt, GameContext& ctx) {
    if (ctx.particles) {
        ctx.particles->Update(dt);
    }

    if (teleporterMapOpen_) {
        UpdateTeleporterOverlay(ctx);
        return;
    }

    Room& curRoom = castle_.GetRoom(curGx_, curGy_);
    curRoom.Update(dt, ctx, player_);
    castle_.SyncRoomStateToContext(curRoom, ctx);

    // 1. Перевірка відкриття карти Телепорту
    if (curRoom.requestOpenTeleporterMap || IsKeyPressed(KEY_M)) {
        curRoom.requestOpenTeleporterMap = false;
        if (!curRoom.isBossArenaActive) {
            teleporterMapOpen_ = true;
            tpCursorGx_ = curGx_;
            tpCursorGy_ = curGy_;
            return;
        }
    }

    // 2. Перевірка входу в Арену Боса
    if (curRoom.requestEnterBossArena) {
        curRoom.requestEnterBossArena = false;
        curRoom.EnterBossArena(curRoom.requestedBossArenaType, curRoom.roomLevel + 3, ctx, player_);
        return;
    }

    // 3. Перевірка виходу з Арени Боса після перемоги
    if (curRoom.requestExitBossArena) {
        curRoom.requestExitBossArena = false;
        const SavedRoomNode& saved = ctx.savedCastle[static_cast<size_t>(curGy_)][static_cast<size_t>(curGx_)];
        curRoom.BuildFromNode(curGx_, curGy_, saved, curRoom.roomLevel, ctx);
        player_.SetCenter({ Config::VIRTUAL_WIDTH * 0.5f, 12.0f * Config::TILE_SIZE });
        return;
    }

    // 4. Перевірка перемоги над Фінальним Босом (Johannes / The Fountain) -> Перехід у NG+!
    if (curRoom.requestFinalVictory) {
        curRoom.requestFinalVictory = false;
        int victoryBonus = static_cast<int>(std::round((1500 + ctx.ngPlusLevel * 1000) * player_.stats.goldGainMult));
        ctx.gold += victoryBonus;
        ctx.runGoldCollected += victoryBonus;

        ctx.ngPlusLevel++;
        ctx.bossDefeated.fill(false);
        ctx.isCastleLocked = false;
        ctx.hasSavedCastle = false;
        ctx.lastRunVictory = true;
        ctx.lastKillerName = "DEFEATED THE FOUNTAIN (NG+" + std::to_string(ctx.ngPlusLevel) + ")";

        SlainRecord rec;
        rec.heroName = ctx.activeHeir.name;
        rec.heroClass = ctx.activeHeir.classType;
        rec.killerName = "Triumphant Conqueror";
        rec.goldCollected = ctx.runGoldCollected;
        rec.enemiesSlain = ctx.runKills;
        rec.wasVictory = true;
        ctx.lineageHistory.push_back(rec);

        SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
        ctx.RequestStateChange(GameStateId::DeathSummary);
        return;
    }

    // 5. Перевірка смерті гравця
    if (player_.hp <= 0) {
        ctx.lastRunVictory = false;

        SlainRecord rec;
        rec.heroName = ctx.activeHeir.name;
        rec.heroClass = ctx.activeHeir.classType;
        rec.killerName = ctx.lastKillerName;
        rec.goldCollected = ctx.runGoldCollected;
        rec.enemiesSlain = ctx.runKills;
        rec.wasVictory = false;
        ctx.lineageHistory.push_back(rec);

        SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
        ctx.RequestStateChange(GameStateId::DeathSummary);
        return;
    }

    // 6. Переходи між сусідніми кімнатами замку 9x9
    if (!curRoom.isBossArenaActive) {
        CheckRoomDoorTransitions(ctx);
    }
}

void CastlePlayState::CheckRoomDoorTransitions(GameContext& ctx) {
    Room& curRoom = castle_.GetRoom(curGx_, curGy_);
    Rectangle pBounds = player_.GetBounds();

    int nextGx = curGx_;
    int nextGy = curGy_;
    Vector2 newPlayerCenter = player_.GetCenter();
    bool transitioned = false;

    const float roomLeft   = static_cast<float>(Config::ROOM_OFFSET_X);
    const float roomRight  = static_cast<float>(Config::ROOM_OFFSET_X + Config::ROOM_PIXEL_WIDTH);
    const float roomBottom = static_cast<float>(Config::ROOM_PIXEL_HEIGHT);

    // Вихід на Захід (WEST)
    if (pBounds.x < roomLeft && (curRoom.doorMask & DoorMask::WEST) && curGx_ > 0) {
        nextGx = curGx_ - 1;
        newPlayerCenter = { roomRight - 36.0f, 7.5f * Config::TILE_SIZE };
        transitioned = true;
    }
    // Вихід на Схід (EAST)
    else if (player_.GetRight() > roomRight && (curRoom.doorMask & DoorMask::EAST) && curGx_ < Config::CASTLE_GRID_SIZE - 1) {
        nextGx = curGx_ + 1;
        newPlayerCenter = { roomLeft + 36.0f, 7.5f * Config::TILE_SIZE };
        transitioned = true;
    }
    // Вихід на Північ (NORTH)
    else if (pBounds.y < 0.0f && (curRoom.doorMask & DoorMask::NORTH) && curGy_ > 0) {
        nextGy = curGy_ - 1;
        newPlayerCenter = { Config::VIRTUAL_WIDTH * 0.5f, roomBottom - 54.0f };
        player_.vel.y = -340.0f;
        transitioned = true;
    }
    // Вихід на Південь (SOUTH)
    else if (player_.GetBottom() > roomBottom - 8.0f && (curRoom.doorMask & DoorMask::SOUTH) && curGy_ < Config::CASTLE_GRID_SIZE - 1) {
        nextGy = curGy_ + 1;
        newPlayerCenter = { Config::VIRTUAL_WIDTH * 0.5f, 42.0f };
        transitioned = true;
    }

    if (transitioned && castle_.IsRoomActive(nextGx, nextGy)) {
        castle_.SyncRoomStateToContext(curRoom, ctx);
        curGx_ = nextGx;
        curGy_ = nextGy;
        player_.SetCenter(newPlayerCenter);

        Room& nextRoom = castle_.GetRoom(curGx_, curGy_);
        nextRoom.OnPlayerEnterRoom(ctx, player_);
        castle_.SyncRoomStateToContext(nextRoom, ctx);
    }
}

void CastlePlayState::UpdateTeleporterOverlay(GameContext& ctx) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_M) || IsKeyPressed(KEY_TAB)) {
        teleporterMapOpen_ = false;
        return;
    }

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  tpCursorGx_ = std::max(0, tpCursorGx_ - 1);
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) tpCursorGx_ = std::min(Config::CASTLE_GRID_SIZE - 1, tpCursorGx_ + 1);
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    tpCursorGy_ = std::max(0, tpCursorGy_ - 1);
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  tpCursorGy_ = std::min(Config::CASTLE_GRID_SIZE - 1, tpCursorGy_ + 1);

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        const Room& target = castle_.GetRoom(tpCursorGx_, tpCursorGy_);
        bool canTeleport = target.visited &&
            (target.roomType == RoomType::Entrance ||
             target.roomType == RoomType::Teleporter ||
             target.roomType == RoomType::BossEntrance);

        if (canTeleport) {
            curGx_ = tpCursorGx_;
            curGy_ = tpCursorGy_;
            Room& destRoom = castle_.GetRoom(curGx_, curGy_);
            player_.SetCenter({ Config::VIRTUAL_WIDTH * 0.5f, 12.0f * Config::TILE_SIZE });
            destRoom.OnPlayerEnterRoom(ctx, player_);
            teleporterMapOpen_ = false;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 1.25f);
        } else {
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
        }
    }
}

void CastlePlayState::Draw(GameContext& ctx) {
    const Room& curRoom = castle_.GetRoom(curGx_, curGy_);

    // 1. Захоплення сцени у RenderTexture2D та пост-обробка GLSL 330 шейдерами (Colorblind, Nostalgic, Myopia, Hyperopia, Darkness, Vertigo)
    if (ctx.shaders) {
        ctx.shaders->BeginSceneCapture();
    }

    curRoom.Draw(ctx, player_);
    if (ctx.particles) {
        ctx.particles->Draw();
    }

    // Червоний спалах по краях при пораненні (вимкнений, якщо у спадкоємця риса CIP!)
    if (player_.hurtFlashTimer > 0.0f && !ctx.CurrentHeirHasTrait(TraitType::CIP)) {
        DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(RED, 0.28f));
    }

    if (ctx.shaders) {
        std::vector<Vector2> torches = curRoom.GetActiveTorchScreenPositions();
        bool isDarkBiome = (curRoom.biome == BiomeType::LandOfDarkness);
        ctx.shaders->EndSceneAndPresent(
            ctx,
            player_.GetCenter(),
            isDarkBiome,
            player_.headlampOn,
            player_.facingDir,
            player_.IsTimeStopActive(),
            torches
        );
    }

    // 2. Рендер HUD поверх шейдерів (щоб при Vertigo світ був догори дриґом, але HUD залишався читабельним!)
    Camera2D hudCam{};
    hudCam.zoom = Config::SCALE_FACTOR;
    BeginMode2D(hudCam);

    DrawHud(ctx, curRoom);
    DrawMinimap(ctx, curRoom);

    if (curRoom.hasActiveBoss && curRoom.activeBoss.active) {
        curRoom.activeBoss.DrawHudHealthBar();
    }

    if (teleporterMapOpen_) {
        DrawTeleporterOverlay(ctx);
    }

    EndMode2D();
}

void CastlePlayState::DrawHud(const GameContext& ctx, const Room& curRoom) const {
    bool hasCip = ctx.CurrentHeirHasTrait(TraitType::CIP);

    int panelX = 16;
    int panelY = 14;
    int panelW = 390;
    int panelH = 112;
    DrawRectangle(panelX, panelY, panelW, panelH, Color{ 12, 14, 24, 215 });
    DrawRectangleLines(panelX, panelY, panelW, panelH, GOLD);

    // Ім'я героя, покоління та клас
    std::string titleLine = ctx.activeHeir.name + "  [" + Registry::GetClass(ctx.activeHeir.classType).name + "]";
    Ui::Text(titleLine, panelX + 12, panelY + 9, 16, GOLD);

    // Смужка HP (при рисі CIP прихована і відображає "??? / ???")
    int hpBarX = panelX + 12;
    int hpBarY = panelY + 34;
    int barW = 210;
    int barH = 18;
    DrawRectangle(hpBarX, hpBarY, barW, barH, BLACK);
    if (hasCip) {
        DrawRectangle(hpBarX, hpBarY, barW, barH, Color{ 95, 45, 115, 255 });
        Ui::Text("HP: ??? / ???", hpBarX + 44, hpBarY + 2, 13, WHITE);
    } else {
        float hpRatio = std::clamp(static_cast<float>(player_.hp) / static_cast<float>(std::max(1, player_.maxHp)), 0.0f, 1.0f);
        DrawRectangle(hpBarX, hpBarY, static_cast<int>(barW * hpRatio), barH, Color{ 205, 35, 45, 255 });
        std::string hpStr = "HP: " + std::to_string(player_.hp) + " / " + std::to_string(player_.maxHp);
        Ui::Text(hpStr, hpBarX + 10, hpBarY + 2, 13, WHITE);
    }
    DrawRectangleLines(hpBarX, hpBarY, barW, barH, LIGHTGRAY);

    // Смужка MP
    int mpBarY = panelY + 58;
    DrawRectangle(hpBarX, mpBarY, barW, barH, BLACK);
    float mpRatio = std::clamp(static_cast<float>(player_.mp) / static_cast<float>(std::max(1, player_.stats.maxMp)), 0.0f, 1.0f);
    DrawRectangle(hpBarX, mpBarY, static_cast<int>(barW * mpRatio), barH, Color{ 45, 130, 225, 255 });
    std::string mpStr = "MP: " + std::to_string(player_.mp) + " / " + std::to_string(player_.stats.maxMp);
    Ui::Text(mpStr, hpBarX + 10, mpBarY + 2, 13, WHITE);
    DrawRectangleLines(hpBarX, mpBarY, barW, barH, LIGHTGRAY);

    // Золото та рівень
    std::string goldStr = "GOLD: " + std::to_string(ctx.gold);
    Ui::Text(goldStr, panelX + 236, panelY + 34, 15, YELLOW);

    std::string ngStr = "NG+" + std::to_string(ctx.ngPlusLevel) + "  Lv." + std::to_string(curRoom.roomLevel);
    Ui::Text(ngStr, panelX + 236, panelY + 58, 14, SKYBLUE);

    // Активне закляття та талант
    const SpellDef& sp = Registry::GetSpell(player_.activeSpell);
    const ClassDef& cl = Registry::GetClass(ctx.activeHeir.classType);
    std::string abilLine = "[L] " + sp.name + " (" + std::to_string(sp.manaCost) + "MP)   [Q] " + cl.talentName;
    Ui::Text(abilLine, panelX + 12, panelY + 84, 13, Color{ 220, 225, 235, 255 });
}

void CastlePlayState::DrawMinimap(const GameContext& ctx, const Room& curRoom) const {
    int cell = 13;
    int mapSize = Config::CASTLE_GRID_SIZE * cell;
    int mapX = Config::VIRTUAL_WIDTH - mapSize - 20;
    int mapY = 14;

    DrawRectangle(mapX - 6, mapY - 6, mapSize + 12, mapSize + 32, Color{ 10, 12, 22, 220 });
    DrawRectangleLines(mapX - 6, mapY - 6, mapSize + 12, mapSize + 32, GOLD);

    bool isSpelunker = (ctx.activeHeir.classType == ClassType::Spelunker_Upgraded) ||
                       (ctx.activeHeir.classType == ClassType::Miner_Base && player_.headlampOn);

    for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
            const Room& r = castle_.GetRoom(x, y);
            if (r.doorMask == 0) continue;

            bool showRoom = r.visited || isSpelunker;
            if (!showRoom) {
                // Показуємо сусідні невідвідані кімнати тьмяним контуром
                const int dx[4] = { 0, 1, 0, -1 };
                const int dy[4] = { -1, 0, 1, 0 };
                for (int d = 0; d < 4; ++d) {
                    int nx = x + dx[d];
                    int ny = y + dy[d];
                    if (nx >= 0 && nx < Config::CASTLE_GRID_SIZE && ny >= 0 && ny < Config::CASTLE_GRID_SIZE) {
                        if (castle_.GetRoom(nx, ny).visited) {
                            showRoom = true;
                            break;
                        }
                    }
                }
            }
            if (!showRoom) continue;

            int rx = mapX + x * cell;
            int ry = mapY + y * cell;

            Color c = Color{ 60, 68, 88, 255 };
            switch (r.biome) {
                case BiomeType::CastleHamson:   c = Color{ 75, 110, 165, 255 }; break;
                case BiomeType::ForestAbkhazia: c = Color{ 55, 145, 78, 255 };  break;
                case BiomeType::TheMaya:        c = Color{ 185, 145, 65, 255 }; break;
                case BiomeType::LandOfDarkness: c = Color{ 125, 60, 165, 255 }; break;
                default: break;
            }
            if (!r.visited) {
                c = Fade(c, 0.38f);
            }

            DrawRectangle(rx + 1, ry + 1, cell - 2, cell - 2, c);

            // Іконки спеціальних кімнат
            if (r.roomType == RoomType::BossEntrance) {
                DrawCircle(rx + cell / 2, ry + cell / 2, 3.5f, RED);
            } else if (r.roomType == RoomType::Teleporter || r.roomType == RoomType::Entrance) {
                DrawCircle(rx + cell / 2, ry + cell / 2, 3.0f, SKYBLUE);
            } else if ((r.roomType == RoomType::FairyChest || r.roomType == RoomType::Treasure) && !r.chestOpened && isSpelunker) {
                DrawCircle(rx + cell / 2, ry + cell / 2, 3.0f, GOLD);
            }

            if (x == curGx_ && y == curGy_) {
                DrawRectangleLines(rx, ry, cell, cell, WHITE);
            }
        }
    }

    std::string bName = Registry::GetBiomeName(curRoom.biome);
    int bw = Ui::Measure(bName, 13);
    int bx = mapX + (mapSize - bw) / 2;
    Ui::Text(bName, bx, mapY + mapSize + 8, 13, GOLD);
}

void CastlePlayState::DrawTeleporterOverlay(const GameContext& /*ctx*/) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.82f));

    int cell = 44;
    int mapSize = Config::CASTLE_GRID_SIZE * cell;
    int mx = (Config::VIRTUAL_WIDTH - mapSize) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mapSize) / 2 + 10;

    const char* title = "CASTLE TELEPORTER NETWORK";
    int tw = Ui::Measure(title, 26);
    Ui::Text(title, (Config::VIRTUAL_WIDTH - tw) / 2, my - 68, 26, SKYBLUE);
    const char* sub = "Select a visited Teleporter (Cyan) or Boss Entrance (Red) with [WASD] and press [SPACE / ENTER] to Warp.";
    int sw = Ui::Measure(sub, 15);
    Ui::Text(sub, (Config::VIRTUAL_WIDTH - sw) / 2, my - 32, 15, Color{ 220, 225, 235, 255 });

    DrawRectangle(mx - 10, my - 10, mapSize + 20, mapSize + 20, Color{ 16, 18, 30, 255 });
    DrawRectangleLines(mx - 10, my - 10, mapSize + 20, mapSize + 20, SKYBLUE);

    for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
            const Room& r = castle_.GetRoom(x, y);
            int rx = mx + x * cell;
            int ry = my + y * cell;

            if (r.doorMask != 0 && r.visited) {
                Color col = Color{ 55, 65, 92, 255 };
                if (r.roomType == RoomType::Entrance || r.roomType == RoomType::Teleporter) {
                    col = Color{ 35, 135, 195, 255 };
                } else if (r.roomType == RoomType::BossEntrance) {
                    col = Color{ 175, 42, 55, 255 };
                }
                DrawRectangle(rx + 2, ry + 2, cell - 4, cell - 4, col);
            } else {
                DrawRectangle(rx + 2, ry + 2, cell - 4, cell - 4, Color{ 24, 26, 38, 255 });
            }

            if (x == tpCursorGx_ && y == tpCursorGy_) {
                DrawRectangleLinesEx({ static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(cell), static_cast<float>(cell) }, 3.0f, GOLD);
            }
        }
    }
}