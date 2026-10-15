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

    // Ініціалізація 2D-камери світу (1:1 масштаб без зазорів, 1920x1080)
    worldCam_.offset = { Config::RENDER_WIDTH * 0.5f, Config::RENDER_HEIGHT * 0.5f };
    worldCam_.zoom = static_cast<float>(Config::RENDER_WIDTH) / static_cast<float>(Config::ROOM_PIXEL_WIDTH);
    camTarget_ = { Config::ROOM_PIXEL_WIDTH * 0.5f, Config::ROOM_PIXEL_HEIGHT * 0.5f };
    worldCam_.target = camTarget_;

    Room& startRoom = castle_.GetRoom(curGx_, curGy_);
    startRoom.OnPlayerEnterRoom(ctx, player_);
    castle_.SyncRoomStateToContext(startRoom, ctx);

    bannerTimer_ = 2.2f;
    if (ctx.CurrentHeirHasTrait(TraitType::TheOne)) {
        bannerText_ = "JACKING IN...";
    } else if (ctx.CurrentHeirHasTrait(TraitType::Nostalgic)) {
        bannerText_ = "REMINISCING...";
    } else if (ctx.CurrentHeirHasTrait(TraitType::Baldness)) {
        bannerText_ = "BALDING...";
    } else {
        bannerText_ = "BUILDING CASTLE...";
    }

    if (ctx.CurrentHeirHasTrait(TraitType::Dyslexia)) {
        bannerText_ = Ui::ScrambleDyslexia(bannerText_);
    }
}

void CastlePlayState::OnExit(GameContext& ctx) {
    if (ctx.audio) ctx.audio->SetBossMusic(false);
    SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
}

void CastlePlayState::Update(float dt, GameContext& ctx) {
    if (bannerTimer_ > 0.0f) {
        bannerTimer_ -= dt;
    }

    if (ctx.particles) {
        ctx.particles->Update(dt);
    }

    if (settingsMenu_.IsOpen()) {
        settingsMenu_.Update(dt, ctx);
        return;
    }

    if (teleporterMapOpen_) {
        UpdateTeleporterOverlay(ctx);
        return;
    }

    // Пауза та меню налаштувань по Escape
    if (IsKeyPressed(KEY_ESCAPE)) {
        settingsMenu_.Open(true);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.9f, 0.6f);
        return;
    }

    // 1. Hit-stop: 3 кадри заморозки симуляції при влучанні
    if (ctx.hitStopFrames > 0) {
        ctx.hitStopFrames--;
        ctx.screenTrauma = std::max(0.0f, ctx.screenTrauma - 1.25f * dt);
        float maxShake = 24.0f;
        float shakeMag = maxShake * (ctx.screenTrauma * ctx.screenTrauma);
        float shakeAngle = static_cast<float>(std::rand() % 628) / 100.0f;
        worldCam_.target = { camTarget_.x + std::cos(shakeAngle) * shakeMag, camTarget_.y + std::sin(shakeAngle) * shakeMag };
        return;
    }

    Room& curRoom = castle_.GetRoom(curGx_, curGy_);
    curRoom.Update(dt, ctx, player_);
    castle_.SyncRoomStateToContext(curRoom, ctx);

    // Плавне стеження 2D-камери за гравцем (Smooth Lerp Camera)
    float targetX = player_.GetCenter().x;
    float targetY = player_.GetCenter().y;

    float minX = (Config::RENDER_WIDTH * 0.5f) / worldCam_.zoom;
    float maxX = static_cast<float>(Config::ROOM_PIXEL_WIDTH) - minX;
    if (minX >= maxX) {
        targetX = Config::ROOM_PIXEL_WIDTH * 0.5f;
    } else {
        targetX = std::clamp(targetX, minX, maxX);
    }

    float minY = (Config::RENDER_HEIGHT * 0.5f) / worldCam_.zoom;
    float maxY = static_cast<float>(Config::ROOM_PIXEL_HEIGHT) - minY;
    if (minY >= maxY) {
        targetY = Config::ROOM_PIXEL_HEIGHT * 0.5f;
    } else {
        targetY = std::clamp(targetY, minY, maxY);
    }

    camTarget_.x += (targetX - camTarget_.x) * std::min(1.0f, 10.0f * dt);
    camTarget_.y += (targetY - camTarget_.y) * std::min(1.0f, 10.0f * dt);

    // Screen shake за моделлю trauma: offset = maxShake * trauma², спадає лінійно
    ctx.screenTrauma = std::max(0.0f, ctx.screenTrauma - 1.25f * dt);
    float maxShake = 24.0f;
    float trauma2 = ctx.screenTrauma * ctx.screenTrauma;
    float shakeMag = maxShake * trauma2;
    float shakeAngle = static_cast<float>(std::rand() % 628) / 100.0f;
    worldCam_.target = { camTarget_.x + std::cos(shakeAngle) * shakeMag, camTarget_.y + std::sin(shakeAngle) * shakeMag };

    // 1. Перевірка відкриття карти Телепорту
    if (curRoom.requestOpenTeleporterMap || ctx.input.IsActionPressed(InputAction::OpenMap)) {
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
        player_.SetCenter({ Config::ROOM_PIXEL_WIDTH * 0.5f, 12.0f * Config::TILE_SIZE });
        camTarget_ = player_.GetCenter();
        worldCam_.target = camTarget_;
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
        newPlayerCenter = { Config::ROOM_PIXEL_WIDTH * 0.5f, roomBottom - 54.0f };
        player_.vel.y = -340.0f;
        transitioned = true;
    }
    // Вихід на Південь (SOUTH)
    else if (player_.GetBottom() > roomBottom - 8.0f && (curRoom.doorMask & DoorMask::SOUTH) && curGy_ < Config::CASTLE_GRID_SIZE - 1) {
        nextGy = curGy_ + 1;
        newPlayerCenter = { Config::ROOM_PIXEL_WIDTH * 0.5f, 42.0f };
        transitioned = true;
    }

    if (transitioned && castle_.IsRoomActive(nextGx, nextGy)) {
        // Видаляємо всі неподібрані монети, мішечки та зілля з кімнати при переході в іншу кімнату
        curRoom.interactables.erase(
            std::remove_if(curRoom.interactables.begin(), curRoom.interactables.end(),
                [](const Interactable& it) {
                    return it.kind == InteractableKind::DroppedCoin ||
                           it.kind == InteractableKind::MeatDrop ||
                           it.kind == InteractableKind::ManaDrop;
                }),
            curRoom.interactables.end()
        );
        curRoom.projectiles.clear();
        for (auto& en : curRoom.enemies) {
            if (en.active) {
                en.ResetToSpawn();
            }
        }

        castle_.SyncRoomStateToContext(curRoom, ctx);
        curGx_ = nextGx;
        curGy_ = nextGy;
        player_.SetCenter(newPlayerCenter);

        Room& nextRoom = castle_.GetRoom(curGx_, curGy_);
        nextRoom.OnPlayerEnterRoom(ctx, player_);
        castle_.SyncRoomStateToContext(nextRoom, ctx);

        // Миттєве фокусування 2D-камери
        camTarget_ = player_.GetCenter();
        worldCam_.target = camTarget_;
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
            Room& leavingRoom = castle_.GetRoom(curGx_, curGy_);
            leavingRoom.interactables.erase(
                std::remove_if(leavingRoom.interactables.begin(), leavingRoom.interactables.end(),
                    [](const Interactable& it) {
                        return it.kind == InteractableKind::DroppedCoin ||
                               it.kind == InteractableKind::MeatDrop ||
                               it.kind == InteractableKind::ManaDrop;
                    }),
                leavingRoom.interactables.end()
            );
            leavingRoom.projectiles.clear();
            for (auto& en : leavingRoom.enemies) {
                if (en.active) {
                    en.ResetToSpawn();
                }
            }
            castle_.SyncRoomStateToContext(leavingRoom, ctx);

            curGx_ = tpCursorGx_;
            curGy_ = tpCursorGy_;
            Room& destRoom = castle_.GetRoom(curGx_, curGy_);
            player_.SetCenter({ Config::ROOM_PIXEL_WIDTH * 0.5f, 12.0f * Config::TILE_SIZE });
            destRoom.OnPlayerEnterRoom(ctx, player_);
            teleporterMapOpen_ = false;

            camTarget_ = player_.GetCenter();
            worldCam_.target = camTarget_;

            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 1.25f);
        } else {
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
        }
    }
}

namespace {
    void DrawSpellIcon(SpellType type, int cx, int cy, float size) {
        float r = size * 0.5f;
        switch (type) {
            case SpellType::Dagger: {
                DrawLineEx({ cx - r * 0.5f, cy + r * 0.5f }, { cx + r * 0.5f, cy - r * 0.5f }, 3.5f, Color{ 220, 230, 245, 255 });
                DrawCircle(static_cast<int>(cx + r * 0.5f), static_cast<int>(cy - r * 0.5f), 2.0f, WHITE);
                DrawCircle(static_cast<int>(cx - r * 0.5f), static_cast<int>(cy + r * 0.5f), 2.5f, GOLD);
                break;
            }
            case SpellType::Axe: {
                DrawLineEx({ cx - r * 0.45f, cy + r * 0.5f }, { cx + r * 0.45f, cy - r * 0.5f }, 3.0f, Color{ 140, 95, 55, 255 });
                DrawTriangle({ static_cast<float>(cx + r * 0.5f), static_cast<float>(cy - r * 0.6f) },
                             { static_cast<float>(cx + r * 0.1f), static_cast<float>(cy - r * 0.2f) },
                             { static_cast<float>(cx + r * 0.65f), static_cast<float>(cy - r * 0.1f) },
                             Color{ 215, 225, 240, 255 });
                break;
            }
            case SpellType::Chakram: {
                DrawRing({ static_cast<float>(cx), static_cast<float>(cy) }, r * 0.45f, r * 0.75f, 0.0f, 360.0f, 24, Color{ 215, 225, 240, 255 });
                DrawCircle(cx, cy, 2.5f, GOLD);
                break;
            }
            case SpellType::FlameBarrier: {
                DrawCircle(cx, cy, r * 0.65f, Color{ 255, 75, 25, 255 });
                DrawCircle(cx, cy, r * 0.40f, Color{ 255, 215, 45, 255 });
                DrawCircle(cx, cy, r * 0.20f, WHITE);
                break;
            }
            case SpellType::Conflux: {
                DrawRing({ static_cast<float>(cx), static_cast<float>(cy) }, r * 0.25f, r * 0.65f, 0.0f, 360.0f, 16, Color{ 140, 230, 255, 255 });
                DrawCircle(cx, cy, 3.0f, WHITE);
                break;
            }
            case SpellType::Scythe: {
                DrawLineEx({ cx - r * 0.4f, cy + r * 0.6f }, { cx + r * 0.35f, cy - r * 0.55f }, 2.8f, Color{ 110, 80, 50, 255 });
                DrawRing({ static_cast<float>(cx + r * 0.35f), static_cast<float>(cy - r * 0.2f) }, r * 0.45f, r * 0.70f, 180.0f, 330.0f, 16, Color{ 220, 230, 245, 255 });
                break;
            }
            case SpellType::CrowStorm: {
                DrawTriangle({ static_cast<float>(cx), static_cast<float>(cy - r * 0.5f) },
                             { static_cast<float>(cx - r * 0.65f), static_cast<float>(cy + r * 0.4f) },
                             { static_cast<float>(cx + r * 0.65f), static_cast<float>(cy + r * 0.4f) },
                             Color{ 135, 45, 185, 255 });
                DrawCircle(cx, cy, 3.0f, GOLD);
                break;
            }
            case SpellType::TimeStop: {
                DrawTriangle({ static_cast<float>(cx - r * 0.5f), static_cast<float>(cy - r * 0.5f) },
                             { static_cast<float>(cx + r * 0.5f), static_cast<float>(cy - r * 0.5f) },
                             { static_cast<float>(cx), static_cast<float>(cy) },
                             Color{ 255, 215, 30, 255 });
                DrawTriangle({ static_cast<float>(cx - r * 0.5f), static_cast<float>(cy + r * 0.5f) },
                             { static_cast<float>(cx), static_cast<float>(cy) },
                             { static_cast<float>(cx + r * 0.5f), static_cast<float>(cy + r * 0.5f) },
                             Color{ 255, 215, 30, 255 });
                break;
            }
            default:
                DrawCircle(cx, cy, r * 0.5f, SKYBLUE);
                break;
        }
    }

    void DrawTalentIcon(ClassType type, int cx, int cy, float size) {
        float r = size * 0.5f;
        switch (type) {
            case ClassType::Knights_Base:
            case ClassType::Paladin_Upgraded: {
                Rectangle sRec = { cx - r * 0.45f, cy - r * 0.5f, r * 0.9f, r * 0.7f };
                DrawRectangleRounded(sRec, 0.3f, 4, Color{ 145, 155, 175, 255 });
                DrawLineEx({ cx - r * 0.35f, static_cast<float>(cy) }, { cx + r * 0.35f, static_cast<float>(cy) }, 2.0f, GOLD);
                DrawLineEx({ static_cast<float>(cx), cy - r * 0.4f }, { static_cast<float>(cx), cy + r * 0.4f }, 2.0f, GOLD);
                break;
            }
            case ClassType::Barbarian_Base:
            case ClassType::BarbarianKing_Upgraded: {
                DrawCircleLines(cx, cy, r * 0.35f, ORANGE);
                DrawCircleLines(cx, cy, r * 0.65f, RED);
                DrawCircle(cx, cy, 3.0f, WHITE);
                break;
            }
            case ClassType::Shinobi_Base:
            case ClassType::Hokage_Upgraded: {
                Rectangle logRec = { cx - r * 0.35f, cy - r * 0.5f, r * 0.7f, r * 1.0f };
                DrawRectangleRounded(logRec, 0.3f, 4, Color{ 120, 75, 40, 255 });
                DrawCircle(cx, cy - static_cast<int>(r * 0.3f), 3.0f, GREEN);
                break;
            }
            case ClassType::Miner_Base:
            case ClassType::Spelunker_Upgraded: {
                Rectangle lRec = { cx - r * 0.35f, cy - r * 0.35f, r * 0.7f, r * 0.7f };
                DrawRectangleRounded(lRec, 0.2f, 4, Color{ 75, 60, 45, 255 });
                DrawCircle(cx, cy, r * 0.25f, YELLOW);
                break;
            }
            case ClassType::Lich_Base:
            case ClassType::LichKing_Upgraded: {
                DrawCircle(cx - 3, cy, r * 0.35f, Fade(Color{ 180, 70, 240, 255 }, 0.85f));
                DrawCircle(cx + 3, cy - 2, r * 0.30f, Fade(Color{ 140, 40, 210, 255 }, 0.85f));
                break;
            }
            default: {
                DrawCircle(cx, cy, r * 0.5f, Color{ 180, 195, 220, 255 });
                DrawCircleLines(cx, cy, r * 0.5f, GOLD);
                break;
            }
        }
    }
} // anonymous namespace

void CastlePlayState::Draw(GameContext& ctx) {
    const Room& curRoom = castle_.GetRoom(curGx_, curGy_);

    if (ctx.shaders) {
        // Оновлюємо стан debug-клавіш F5-F10
        ctx.shaders->UpdateDebugHotkeys();

        // 1. Прохід сцени: фон біому, золоті двері, тайли, пропси
        ctx.shaders->BeginScenePass(worldCam_);
        curRoom.DrawBackgroundAndTiles(ctx, player_);
        curRoom.DrawProps(ctx, player_);
        ctx.shaders->EndScenePass();

        // 2. Прохід сутностей: вороги, боси, герой, снаряди у прозорий entityRT_
        ctx.shaders->BeginEntityPass(worldCam_);
        curRoom.DrawEntities(ctx, player_);
        ctx.shaders->EndEntityPass();

        // 3. Накладання сутностей із outline / silhouette шейдером на sceneRT_
        ctx.shaders->CompositeEntitiesToScene();

        // 4. Прохід освітлення: генерація карти світла
        ctx.shaders->BeginLightPass(worldCam_, curRoom.biome);
        curRoom.DrawLights(ctx, player_);
        ctx.shaders->EndLightPass();

        // 5. Композит sceneRT_ * lightRT_ у litRT_
        ctx.shaders->CompositeSceneAndLight();

        // 6. Емісивні ефекти (вогонь, очі, магія, частинки) у BLEND_ADDITIVE поверх litRT_
        ctx.shaders->BeginEmissivePass(worldCam_);
        curRoom.DrawEmissive(ctx, player_);
        if (ctx.particles) {
            ctx.particles->Draw();
        }
        if (player_.hurtFlashTimer > 0.0f && !ctx.CurrentHeirHasTrait(TraitType::CIP)) {
            DrawRectangle(0, 0, Config::ROOM_PIXEL_WIDTH, Config::ROOM_PIXEL_HEIGHT, Fade(RED, 0.28f));
        }
        ctx.shaders->EndEmissivePass();

        // 7. Bloom прохід (Pass-through на Фазі 1)
        ctx.shaders->ApplyBloom();

        // 8. Фінальний шейдер грейдингу (риси спадкоємців, grayscale F10) та вивід на екран з Point-фільтром
        std::vector<Vector2> worldTorches = curRoom.GetActiveTorchScreenPositions();
        std::vector<Vector2> screenTorches;
        screenTorches.reserve(worldTorches.size());
        for (const auto& tw : worldTorches) {
            Vector2 tScreen = GetWorldToScreen2D(tw, worldCam_);
            screenTorches.push_back({ tScreen.x / Config::SCALE_FACTOR, tScreen.y / Config::SCALE_FACTOR });
        }

        Vector2 pScreen = GetWorldToScreen2D(player_.GetCenter(), worldCam_);
        Vector2 pShaderPos = { pScreen.x / Config::SCALE_FACTOR, pScreen.y / Config::SCALE_FACTOR };

        bool isDarkBiome = (curRoom.biome == BiomeType::LandOfDarkness) || ctx.CurrentHeirHasTrait(TraitType::Glaucoma);
        ctx.shaders->PresentFinalScene(
            ctx,
            pShaderPos,
            isDarkBiome,
            player_.headlampOn,
            player_.facingDir,
            player_.IsTimeStopActive(),
            screenTorches
        );
    } else {
        curRoom.Draw(ctx, player_);
        if (ctx.particles) {
            ctx.particles->Draw();
        }
    }

    // 9. Автентичний HUD Rogue Legacy у повній роздільності 1080p
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

    if (settingsMenu_.IsOpen()) {
        settingsMenu_.Draw(ctx);
    }

    EndMode2D();

    // 10. Текст та підписи у світі через GetWorldToScreen2D (повна роздільність 1080p, без пікселізації)
    curRoom.DrawWorldUi(ctx, player_, worldCam_);
}

void CastlePlayState::DrawHud(const GameContext& ctx, [[maybe_unused]] const Room& curRoom) const {
    bool hasCip = ctx.CurrentHeirHasTrait(TraitType::CIP);

    int hudX = 24;
    int hudY = 16;

    // 1. Рівень гравця (Золотий автентичний текст "Lv 101" з тінню)
    int playerLevel = ctx.GetTotalManorLevels() + 1;
    Ui::Text("Lv", hudX, hudY + 4, 15, Color{ 245, 195, 30, 255 });
    int lvLabelW = Ui::Measure("Lv", 15);

    std::string lvlNum = std::to_string(playerLevel);
    Ui::Text(lvlNum, hudX + lvLabelW + 6 + 1, hudY + 1, 22, Color{ 15, 15, 20, 255 });
    Ui::Text(lvlNum, hudX + lvLabelW + 6, hudY, 22, Color{ 255, 215, 25, 255 });

    // 2. Смужка HP (Автентична зелена смужка з металевою рамкою)
    int barX = hudX;
    int barY = hudY + 28;
    int barW = 180;
    int barH = 16;

    // Сріблясто-металева рамка зі скосом
    DrawRectangle(barX - 2, barY - 2, barW + 4, barH + 4, Color{ 20, 24, 32, 255 });
    DrawRectangle(barX - 1, barY - 1, barW + 2, barH + 2, Color{ 145, 155, 175, 255 });
    DrawLine(barX - 1, barY - 1, barX + barW, barY - 1, Color{ 225, 235, 250, 255 });
    DrawLine(barX - 1, barY + barH, barX + barW, barY + barH, Color{ 70, 75, 90, 255 });
    DrawRectangle(barX, barY, barW, barH, Color{ 16, 20, 16, 255 });

    if (hasCip) {
        DrawRectangle(barX, barY, barW, barH, Color{ 85, 35, 105, 255 });
        const char* cipHp = "??? / ???";
        int chw = Ui::Measure(cipHp, 11);
        Ui::Text(cipHp, barX + (barW - chw) / 2 + 1, barY + 2 + 1, 11, BLACK);
        Ui::Text(cipHp, barX + (barW - chw) / 2, barY + 2, 11, WHITE);
    } else {
        float hpRatio = std::clamp(static_cast<float>(player_.hp) / static_cast<float>(std::max(1, player_.maxHp)), 0.0f, 1.0f);
        int fillW = static_cast<int>(barW * hpRatio);
        if (fillW > 0) {
            DrawRectangle(barX, barY, fillW, barH, Color{ 48, 218, 52, 255 });
            DrawRectangle(barX, barY, fillW, barH / 2, Color{ 115, 245, 115, 255 });
        }
        std::string hpStr = std::to_string(player_.hp) + "/" + std::to_string(player_.maxHp);
        int thw = Ui::Measure(hpStr, 11);
        Ui::Text(hpStr, barX + (barW - thw) / 2 + 1, barY + 2 + 1, 11, Color{ 15, 15, 20, 255 });
        Ui::Text(hpStr, barX + (barW - thw) / 2, barY + 2, 11, WHITE);
    }

    // 3. Смужка MP (Блакитна смужка мани з металевою рамкою)
    int mpBarY = barY + barH + 4;
    int mpBarH = 14;

    DrawRectangle(barX - 2, mpBarY - 2, barW + 4, mpBarH + 4, Color{ 20, 24, 32, 255 });
    DrawRectangle(barX - 1, mpBarY - 1, barW + 2, mpBarH + 2, Color{ 145, 155, 175, 255 });
    DrawLine(barX - 1, mpBarY - 1, barX + barW, mpBarY - 1, Color{ 225, 235, 250, 255 });
    DrawLine(barX - 1, mpBarY + mpBarH, barX + barW, mpBarY + mpBarH, Color{ 70, 75, 90, 255 });
    DrawRectangle(barX, mpBarY, barW, mpBarH, Color{ 14, 18, 28, 255 });

    float mpRatio = std::clamp(static_cast<float>(player_.mp) / static_cast<float>(std::max(1, player_.stats.maxMp)), 0.0f, 1.0f);
    int mpFillW = static_cast<int>(barW * mpRatio);
    if (mpFillW > 0) {
        DrawRectangle(barX, mpBarY, mpFillW, mpBarH, Color{ 45, 135, 250, 255 });
        DrawRectangle(barX, mpBarY, mpFillW, mpBarH / 2, Color{ 110, 185, 255, 255 });
    }
    std::string mpStr = std::to_string(player_.mp) + "/" + std::to_string(player_.stats.maxMp);
    int tmw = Ui::Measure(mpStr, 11);
    Ui::Text(mpStr, barX + (barW - tmw) / 2 + 1, mpBarY + 1 + 1, 11, Color{ 15, 15, 20, 255 });
    Ui::Text(mpStr, barX + (barW - tmw) / 2, mpBarY + 1, 11, WHITE);

    // 4. Лічильник Золота (Об'ємна золота монета + яскраве число)
    int goldY = mpBarY + mpBarH + 7;
    int coinCx = hudX + 9;
    int coinCy = goldY + 10;
    DrawCircle(coinCx, coinCy, 9.5f, Color{ 90, 65, 10, 255 });
    DrawCircle(coinCx, coinCy, 8.5f, Color{ 255, 210, 30, 255 });
    DrawCircle(coinCx, coinCy, 6.5f, Color{ 225, 175, 15, 255 });
    DrawCircle(coinCx, coinCy, 2.5f, Color{ 255, 235, 100, 255 });

    std::string goldStr = std::to_string(ctx.gold);
    Ui::Text(goldStr, hudX + 24 + 1, goldY + 1, 20, Color{ 15, 15, 20, 255 });
    Ui::Text(goldStr, hudX + 24, goldY, 20, Color{ 255, 225, 35, 255 });

    // 5. Дві металеві комірки здібностей (Spell [L] та Talent [Q])
    int slotY = goldY + 28;
    int slotSize = 34;

    const SpellDef& sp = Registry::GetSpell(player_.activeSpell);

    // Комірка 1: Активне закляття
    int slot1X = hudX;
    DrawRectangle(slot1X - 2, slotY - 2, slotSize + 4, slotSize + 4, Color{ 20, 24, 32, 255 });
    DrawRectangle(slot1X - 1, slotY - 1, slotSize + 2, slotSize + 2, Color{ 145, 155, 175, 255 });
    DrawLine(slot1X - 1, slotY - 1, slot1X + slotSize, slotY - 1, Color{ 225, 235, 250, 255 });
    DrawLine(slot1X - 1, slotY + slotSize, slot1X + slotSize, slotY + slotSize, Color{ 70, 75, 90, 255 });
    DrawRectangle(slot1X, slotY, slotSize, slotSize, Color{ 16, 20, 30, 255 });

    DrawSpellIcon(player_.activeSpell, slot1X + slotSize / 2, slotY + slotSize / 2, static_cast<float>(slotSize - 6));

    std::string costText = std::to_string(sp.manaCost) + " mp";
    int ctw = Ui::Measure(costText, 11);
    Ui::Text(costText, slot1X + (slotSize - ctw) / 2 + 1, slotY + slotSize + 3 + 1, 11, BLACK);
    Ui::Text(costText, slot1X + (slotSize - ctw) / 2, slotY + slotSize + 3, 11, Color{ 220, 230, 245, 255 });

    // Комірка 2: Класовий талант
    int slot2X = slot1X + slotSize + 8;
    DrawRectangle(slot2X - 2, slotY - 2, slotSize + 4, slotSize + 4, Color{ 20, 24, 32, 255 });
    DrawRectangle(slot2X - 1, slotY - 1, slotSize + 2, slotSize + 2, Color{ 145, 155, 175, 255 });
    DrawLine(slot2X - 1, slotY - 1, slot2X + slotSize, slotY - 1, Color{ 225, 235, 250, 255 });
    DrawLine(slot2X - 1, slotY + slotSize, slot2X + slotSize, slotY + slotSize, Color{ 70, 75, 90, 255 });
    DrawRectangle(slot2X, slotY, slotSize, slotSize, Color{ 16, 20, 30, 255 });

    DrawTalentIcon(ctx.activeHeir.classType, slot2X + slotSize / 2, slotY + slotSize / 2, static_cast<float>(slotSize - 6));

    const char* talentKey = "[Q]";
    int tkw = Ui::Measure(talentKey, 11);
    Ui::Text(talentKey, slot2X + (slotSize - tkw) / 2 + 1, slotY + slotSize + 3 + 1, 11, BLACK);
    Ui::Text(talentKey, slot2X + (slotSize - tkw) / 2, slotY + slotSize + 3, 11, Color{ 220, 230, 245, 255 });

    // 6. Castle entry / loading banner (Canonical priority: TheOne > Nostalgic > Baldness > Default)
    if (bannerTimer_ > 0.0f && !bannerText_.empty()) {
        float alpha = std::clamp(bannerTimer_ / 0.5f, 0.0f, 1.0f);
        int bw = Ui::Measure(bannerText_, 24);
        int bx = (Config::VIRTUAL_WIDTH - bw) / 2;
        int by = 36;
        DrawRectangle(bx - 18, by - 8, bw + 36, 42, Fade(Color{ 10, 12, 22, 230 }, alpha));
        DrawRectangleLines(bx - 18, by - 8, bw + 36, 42, Fade(GOLD, alpha));
        Ui::Text(bannerText_, bx + 1, by + 1, 24, Fade(BLACK, alpha));
        Ui::Text(bannerText_, bx, by, 24, Fade(Color{ 255, 230, 90, 255 }, alpha));
    }
}

void CastlePlayState::DrawMinimap(const GameContext& ctx, const Room& curRoom) const {
    int cell = 13;
    int mapSize = Config::CASTLE_GRID_SIZE * cell;
    int mapX = Config::VIRTUAL_WIDTH - mapSize - 20;
    int mapY = 14;

    DrawRectangle(mapX - 6, mapY - 6, mapSize + 12, mapSize + 32, Color{ 10, 12, 22, 220 });
    DrawRectangleLines(mapX - 6, mapY - 6, mapSize + 12, mapSize + 32, GOLD);

    bool hasEidetic = ctx.CurrentHeirHasTrait(TraitType::EideticMemory);
    bool isSpelunker = (ctx.activeHeir.classType == ClassType::Spelunker_Upgraded) ||
                       (ctx.activeHeir.classType == ClassType::Miner_Base && player_.headlampOn) ||
                       hasEidetic;

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

            // Eidetic Memory: точкові позиції активних ворогів
            if (hasEidetic && r.visited) {
                for (const auto& en : r.enemies) {
                    if (en.active) {
                        float relX = (en.pos.x - Config::ROOM_OFFSET_X) / static_cast<float>(Config::ROOM_COLS * Config::TILE_SIZE);
                        float relY = (en.pos.y - Config::ROOM_OFFSET_Y) / static_cast<float>(Config::ROOM_ROWS * Config::TILE_SIZE);
                        int ex = rx + 1 + static_cast<int>(std::clamp(relX, 0.1f, 0.9f) * (cell - 3));
                        int ey = ry + 1 + static_cast<int>(std::clamp(relY, 0.1f, 0.9f) * (cell - 3));
                        DrawPixel(ex, ey, RED);
                    }
                }
            }

            if (x == curGx_ && y == curGy_) {
                DrawRectangleLines(rx, ry, cell, cell, WHITE);
            }
        }
    }

    std::string bName = Registry::GetBiomeName(curRoom.biome);
    if (ctx.CurrentHeirHasTrait(TraitType::Dyslexia)) {
        bName = Ui::ScrambleDyslexia(bName);
    }
    int bw = Ui::Measure(bName, 13);
    int bx = mapX + (mapSize - bw) / 2;
    Ui::Text(bName, bx, mapY + mapSize + 8, 13, GOLD);
}

void CastlePlayState::DrawTeleporterOverlay(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.82f));

    int cell = 44;
    int mapSize = Config::CASTLE_GRID_SIZE * cell;
    int mx = (Config::VIRTUAL_WIDTH - mapSize) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mapSize) / 2 + 10;

    bool hasDyslexia = ctx.CurrentHeirHasTrait(TraitType::Dyslexia);
    bool hasAlzheimers = ctx.CurrentHeirHasTrait(TraitType::Alzheimers);
    bool hasEidetic = ctx.CurrentHeirHasTrait(TraitType::EideticMemory);

    std::string title = hasDyslexia ? Ui::ScrambleDyslexia("CASTLE TELEPORTER NETWORK") : "CASTLE TELEPORTER NETWORK";
    int tw = Ui::Measure(title.c_str(), 26);
    Ui::Text(title.c_str(), (Config::VIRTUAL_WIDTH - tw) / 2, my - 68, 26, SKYBLUE);

    std::string sub = hasDyslexia ? Ui::ScrambleDyslexia("Select a visited Teleporter (Cyan) or Boss Entrance (Red) with [WASD] and press [SPACE / ENTER] to Warp.")
                                  : "Select a visited Teleporter (Cyan) or Boss Entrance (Red) with [WASD] and press [SPACE / ENTER] to Warp.";
    int sw = Ui::Measure(sub.c_str(), 15);
    Ui::Text(sub.c_str(), (Config::VIRTUAL_WIDTH - sw) / 2, my - 32, 15, Color{ 220, 225, 235, 255 });

    DrawRectangle(mx - 10, my - 10, mapSize + 20, mapSize + 20, Color{ 16, 18, 30, 255 });
    DrawRectangleLines(mx - 10, my - 10, mapSize + 20, mapSize + 20, SKYBLUE);

    for (int y = 0; y < Config::CASTLE_GRID_SIZE; ++y) {
        for (int x = 0; x < Config::CASTLE_GRID_SIZE; ++x) {
            const Room& r = castle_.GetRoom(x, y);
            int rx = mx + x * cell;
            int ry = my + y * cell;

            if (hasAlzheimers && (x != curGx_ || y != curGy_)) {
                if (r.doorMask != 0) {
                    DrawRectangle(rx + 2, ry + 2, cell - 4, cell - 4, Color{ 28, 22, 40, 255 });
                    Ui::Text("?", rx + 16, ry + 12, 18, Color{ 165, 150, 195, 255 });
                } else {
                    DrawRectangle(rx + 2, ry + 2, cell - 4, cell - 4, Color{ 16, 18, 26, 255 });
                }
            } else if (r.doorMask != 0 && r.visited) {
                Color col = Color{ 55, 65, 92, 255 };
                if (r.roomType == RoomType::Entrance || r.roomType == RoomType::Teleporter) {
                    col = Color{ 35, 135, 195, 255 };
                } else if (r.roomType == RoomType::BossEntrance) {
                    col = Color{ 175, 42, 55, 255 };
                }
                DrawRectangle(rx + 2, ry + 2, cell - 4, cell - 4, col);

                if (hasEidetic) {
                    int enCount = 0;
                    for (const auto& en : r.enemies) {
                        if (en.active) enCount++;
                    }
                    if (enCount > 0) {
                        DrawCircle(rx + cell - 8, ry + 8, 3.5f, RED);
                    }
                }
            } else {
                DrawRectangle(rx + 2, ry + 2, cell - 4, cell - 4, Color{ 24, 26, 38, 255 });
            }

            if (x == tpCursorGx_ && y == tpCursorGy_) {
                DrawRectangleLinesEx({ static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(cell), static_cast<float>(cell) }, 3.0f, GOLD);
            }
        }
    }
}