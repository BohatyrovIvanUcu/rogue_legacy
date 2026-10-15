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
    curGx_ = Config::ENTRANCE_GX;
    curGy_ = Config::ENTRANCE_GY;
    teleporterMapOpen_ = false;

    Room& startRoom = castle_.GetRoom(curGx_, curGy_);
    curRoomId_ = startRoom.roomId;

    Vector2 startPos = {
        static_cast<float>(Config::ROOM_OFFSET_X + 3.5f * Config::TILE_SIZE),
        static_cast<float>(Config::ROOM_OFFSET_Y + 8.0f * Config::TILE_SIZE - 40.0f)
    };
    player_.InitFromContext(ctx, startPos);

    // Ініціалізація 2D-камери світу (1:1 масштаб без зазорів, 1920x1080)
    worldCam_.offset = { Config::RENDER_WIDTH * 0.5f, Config::RENDER_HEIGHT * 0.5f };
    worldCam_.zoom = static_cast<float>(Config::RENDER_WIDTH) / static_cast<float>(Config::CELL_PIXEL_WIDTH);
    float halfViewW = (Config::RENDER_WIDTH * 0.5f) / worldCam_.zoom;
    float halfViewH = (Config::RENDER_HEIGHT * 0.5f) / worldCam_.zoom;
    camTarget_ = { halfViewW, halfViewH };
    worldCam_.target = camTarget_;

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

    // Стеження 2D-камери з мертвою зоною (Camera Deadzone Box)
    const float deadzoneW = 90.0f;
    const float deadzoneH = 65.0f;
    Vector2 pCenter = player_.GetCenter();

    if (pCenter.x > camTarget_.x + deadzoneW) {
        camTarget_.x += (pCenter.x - (camTarget_.x + deadzoneW)) * std::min(1.0f, 9.0f * dt);
    } else if (pCenter.x < camTarget_.x - deadzoneW) {
        camTarget_.x += (pCenter.x - (camTarget_.x - deadzoneW)) * std::min(1.0f, 9.0f * dt);
    }

    if (pCenter.y > camTarget_.y + deadzoneH) {
        camTarget_.y += (pCenter.y - (camTarget_.y + deadzoneH)) * std::min(1.0f, 9.0f * dt);
    } else if (pCenter.y < camTarget_.y - deadzoneH) {
        camTarget_.y += (pCenter.y - (camTarget_.y - deadzoneH)) * std::min(1.0f, 9.0f * dt);
    }

    // Затискання в межах поточної кімнати (з урахуванням multi-cell 1x1, 2x1, 1x2, 2x2)
    float halfViewW = (Config::RENDER_WIDTH * 0.5f) / worldCam_.zoom;
    float halfViewH = (Config::RENDER_HEIGHT * 0.5f) / worldCam_.zoom;
    float roomPxW = static_cast<float>(curRoom.GetPixelWidth());
    float roomPxH = static_cast<float>(curRoom.GetPixelHeight());

    if (roomPxW <= halfViewW * 2.0f) {
        camTarget_.x = roomPxW * 0.5f;
    } else {
        camTarget_.x = std::clamp(camTarget_.x, halfViewW, roomPxW - halfViewW);
    }

    if (roomPxH <= halfViewH * 2.0f) {
        camTarget_.y = roomPxH * 0.5f;
    } else {
        camTarget_.y = std::clamp(camTarget_.y, halfViewH, roomPxH - halfViewH);
    }

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
            tpIndex_ = 0;
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
        if (curRoom.roomId >= 0 && curRoom.roomId < static_cast<int>(ctx.savedRooms.size())) {
            const SavedRoomNode& saved = ctx.savedRooms[static_cast<size_t>(curRoom.roomId)];
            curRoom.BuildFromNode(curRoom.originGx, curRoom.originGy, saved, curRoom.roomLevel, ctx);
        }
        player_.SetCenter({ curRoom.GetPixelWidth() * 0.5f, curRoom.GetPixelHeight() - 3.0f * Config::TILE_SIZE });
        camTarget_ = player_.GetCenter();
        float halfViewW = (Config::RENDER_WIDTH * 0.5f) / worldCam_.zoom;
        float halfViewH = (Config::RENDER_HEIGHT * 0.5f) / worldCam_.zoom;
        float rW = static_cast<float>(curRoom.GetPixelWidth());
        float rH = static_cast<float>(curRoom.GetPixelHeight());
        if (rW <= halfViewW * 2.0f) camTarget_.x = rW * 0.5f;
        else camTarget_.x = std::clamp(camTarget_.x, halfViewW, rW - halfViewW);
        if (rH <= halfViewH * 2.0f) camTarget_.y = rH * 0.5f;
        else camTarget_.y = std::clamp(camTarget_.y, halfViewH, rH - halfViewH);
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
    int nextRoomId = curRoom.roomId;
    Vector2 newPlayerCenter = player_.GetCenter();
    bool transitioned = false;

    float roomPxW = static_cast<float>(curRoom.GetPixelWidth());
    float roomPxH = static_cast<float>(curRoom.GetPixelHeight());

    int relX = std::clamp(static_cast<int>(player_.GetCenter().x / Config::CELL_PIXEL_WIDTH), 0, curRoom.widthCells - 1);
    int relY = std::clamp(static_cast<int>(player_.GetCenter().y / Config::CELL_PIXEL_HEIGHT), 0, curRoom.heightCells - 1);

    // Вихід на Захід (WEST)
    if (pBounds.x < 0.0f && (curRoom.subCellDoors[relY][0] & DoorMask::WEST)) {
        int destGx = curRoom.originGx - 1;
        int destGy = curRoom.originGy + relY;
        const GridCell& cell = castle_.GetGridCell(destGx, destGy);
        if (cell.state == CellState::Filled && cell.roomId >= 0) {
            nextRoomId = cell.roomId;
            Room* destRoom = castle_.GetRoomById(nextRoomId);
            if (destRoom) {
                nextGx = destRoom->originGx;
                nextGy = destRoom->originGy;
                newPlayerCenter.x = (cell.relX + 1) * Config::CELL_PIXEL_WIDTH - 36.0f;
                newPlayerCenter.y = cell.relY * Config::CELL_PIXEL_HEIGHT + 7.5f * Config::TILE_SIZE;
                transitioned = true;
            }
        }
    }
    // Вихід на Схід (EAST)
    else if (player_.GetRight() > roomPxW && (curRoom.subCellDoors[relY][curRoom.widthCells - 1] & DoorMask::EAST)) {
        int destGx = curRoom.originGx + curRoom.widthCells;
        int destGy = curRoom.originGy + relY;
        const GridCell& cell = castle_.GetGridCell(destGx, destGy);
        if (cell.state == CellState::Filled && cell.roomId >= 0) {
            nextRoomId = cell.roomId;
            Room* destRoom = castle_.GetRoomById(nextRoomId);
            if (destRoom) {
                nextGx = destRoom->originGx;
                nextGy = destRoom->originGy;
                newPlayerCenter.x = cell.relX * Config::CELL_PIXEL_WIDTH + 36.0f;
                newPlayerCenter.y = cell.relY * Config::CELL_PIXEL_HEIGHT + 7.5f * Config::TILE_SIZE;
                transitioned = true;
            }
        }
    }
    // Вихід на Північ (NORTH)
    else if (pBounds.y < 0.0f && (curRoom.subCellDoors[0][relX] & DoorMask::NORTH)) {
        int destGx = curRoom.originGx + relX;
        int destGy = curRoom.originGy - 1;
        const GridCell& cell = castle_.GetGridCell(destGx, destGy);
        if (cell.state == CellState::Filled && cell.roomId >= 0) {
            nextRoomId = cell.roomId;
            Room* destRoom = castle_.GetRoomById(nextRoomId);
            if (destRoom) {
                nextGx = destRoom->originGx;
                nextGy = destRoom->originGy;
                newPlayerCenter.x = cell.relX * Config::CELL_PIXEL_WIDTH + Config::CELL_PIXEL_WIDTH * 0.5f;
                newPlayerCenter.y = (cell.relY + 1) * Config::CELL_PIXEL_HEIGHT - 54.0f;
                player_.vel.y = -340.0f;
                transitioned = true;
            }
        }
    }
    // Вихід на Південь (SOUTH)
    else if (player_.GetBottom() > roomPxH - 8.0f && (curRoom.subCellDoors[curRoom.heightCells - 1][relX] & DoorMask::SOUTH)) {
        int destGx = curRoom.originGx + relX;
        int destGy = curRoom.originGy + curRoom.heightCells;
        const GridCell& cell = castle_.GetGridCell(destGx, destGy);
        if (cell.state == CellState::Filled && cell.roomId >= 0) {
            nextRoomId = cell.roomId;
            Room* destRoom = castle_.GetRoomById(nextRoomId);
            if (destRoom) {
                nextGx = destRoom->originGx;
                nextGy = destRoom->originGy;
                newPlayerCenter.x = cell.relX * Config::CELL_PIXEL_WIDTH + Config::CELL_PIXEL_WIDTH * 0.5f;
                newPlayerCenter.y = cell.relY * Config::CELL_PIXEL_HEIGHT + 42.0f;
                transitioned = true;
            }
        }
    }

    if (transitioned) {
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
        curRoomId_ = nextRoomId;
        player_.SetCenter(newPlayerCenter);

        Room& nextRoom = castle_.GetRoom(curGx_, curGy_);
        nextRoom.OnPlayerEnterRoom(ctx, player_);
        castle_.SyncRoomStateToContext(nextRoom, ctx);

        // Миттєве фокусування 2D-камери
        camTarget_ = player_.GetCenter();
        float halfViewW = (Config::RENDER_WIDTH * 0.5f) / worldCam_.zoom;
        float halfViewH = (Config::RENDER_HEIGHT * 0.5f) / worldCam_.zoom;
        float rW = static_cast<float>(nextRoom.GetPixelWidth());
        float rH = static_cast<float>(nextRoom.GetPixelHeight());
        if (rW <= halfViewW * 2.0f) camTarget_.x = rW * 0.5f;
        else camTarget_.x = std::clamp(camTarget_.x, halfViewW, rW - halfViewW);
        if (rH <= halfViewH * 2.0f) camTarget_.y = rH * 0.5f;
        else camTarget_.y = std::clamp(camTarget_.y, halfViewH, rH - halfViewH);
        worldCam_.target = camTarget_;
    }
}

void CastlePlayState::UpdateTeleporterOverlay(GameContext& ctx) {
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_M) || IsKeyPressed(KEY_TAB)) {
        teleporterMapOpen_ = false;
        return;
    }

    // Збираємо всі відвідані телепортери, входи та двері босів
    std::vector<const Room*> tpRooms;
    for (const auto& r : castle_.GetAllRooms()) {
        if (r.visited && (r.roomType == RoomType::Entrance ||
                          r.roomType == RoomType::Teleporter ||
                          r.roomType == RoomType::BossEntrance)) {
            tpRooms.push_back(&r);
        }
    }

    if (tpRooms.empty()) {
        teleporterMapOpen_ = false;
        return;
    }

    if (tpIndex_ < 0 || tpIndex_ >= static_cast<int>(tpRooms.size())) {
        tpIndex_ = 0;
    }

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        tpIndex_ = (tpIndex_ - 1 + static_cast<int>(tpRooms.size())) % static_cast<int>(tpRooms.size());
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.5f, 0.4f);
    }
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
        tpIndex_ = (tpIndex_ + 1) % static_cast<int>(tpRooms.size());
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.5f, 0.4f);
    }

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        const Room* target = tpRooms[static_cast<size_t>(tpIndex_)];
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

        curGx_ = target->originGx;
        curGy_ = target->originGy;
        curRoomId_ = target->roomId;
        Room& destRoom = castle_.GetRoom(curGx_, curGy_);
        player_.SetCenter({ destRoom.GetPixelWidth() * 0.5f, destRoom.GetPixelHeight() - 3.0f * Config::TILE_SIZE });
        destRoom.OnPlayerEnterRoom(ctx, player_);
        teleporterMapOpen_ = false;

        camTarget_ = player_.GetCenter();
        float halfViewW = (Config::RENDER_WIDTH * 0.5f) / worldCam_.zoom;
        float halfViewH = (Config::RENDER_HEIGHT * 0.5f) / worldCam_.zoom;
        float rW = static_cast<float>(destRoom.GetPixelWidth());
        float rH = static_cast<float>(destRoom.GetPixelHeight());
        if (rW <= halfViewW * 2.0f) camTarget_.x = rW * 0.5f;
        else camTarget_.x = std::clamp(camTarget_.x, halfViewW, rW - halfViewW);
        if (rH <= halfViewH * 2.0f) camTarget_.y = rH * 0.5f;
        else camTarget_.y = std::clamp(camTarget_.y, halfViewH, rH - halfViewH);
        worldCam_.target = camTarget_;

        if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 1.25f);
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
            DrawRectangle(0, 0, curRoom.GetPixelWidth(), curRoom.GetPixelHeight(), Fade(RED, 0.28f));
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

void CastlePlayState::DrawHud(const GameContext& ctx, const Room& curRoom) const {
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

    // 7. Попереджувальні індикатори снарядів поза межами екрана (Off-Screen Projectile Warnings)
    // Пригнічуються, якщо активна риса TunnelVision!
    if (!ctx.CurrentHeirHasTrait(TraitType::TunnelVision)) {
        for (const auto& proj : curRoom.projectiles) {
            if (!proj.active || proj.fromPlayer) continue;

            Vector2 scr = GetWorldToScreen2D(proj.pos, worldCam_);
            bool offScreen = (scr.x < -6.0f || scr.x > Config::VIRTUAL_WIDTH + 6.0f ||
                              scr.y < -6.0f || scr.y > Config::VIRTUAL_HEIGHT + 6.0f);
            bool nearby = (scr.x >= -360.0f && scr.x <= Config::VIRTUAL_WIDTH + 360.0f &&
                           scr.y >= -360.0f && scr.y <= Config::VIRTUAL_HEIGHT + 360.0f);

            if (offScreen && nearby) {
                float edgeX = std::clamp(scr.x, 32.0f, static_cast<float>(Config::VIRTUAL_WIDTH - 32));
                float edgeY = std::clamp(scr.y, 32.0f, static_cast<float>(Config::VIRTUAL_HEIGHT - 32));

                float pulse = 0.65f + 0.35f * std::sin(static_cast<float>(GetTime()) * 14.0f);
                Color indCol = Fade(RED, pulse);

                DrawCircle(static_cast<int>(edgeX), static_cast<int>(edgeY), 12.0f, Fade(Color{ 30, 10, 15, 240 }, pulse));
                DrawCircleLines(static_cast<int>(edgeX), static_cast<int>(edgeY), 12.0f, indCol);
                Ui::Text("!", static_cast<int>(edgeX - 3.0f), static_cast<int>(edgeY - 9.0f), 18, indCol);
            }
        }
    }
}

void CastlePlayState::DrawMinimap(const GameContext& ctx, const Room& curRoom) const {
    const int cell = 12; // 12 px per subcell
    const int vpCells = 15; // 15x15 cell viewport centered on player
    const int mapPxSize = vpCells * cell; // 180 px
    const int mapX = Config::VIRTUAL_WIDTH - mapPxSize - 20;
    const int mapY = 14;

    DrawRectangle(mapX - 6, mapY - 6, mapPxSize + 12, mapPxSize + 32, Color{ 10, 12, 22, 220 });
    DrawRectangleLines(mapX - 6, mapY - 6, mapPxSize + 12, mapPxSize + 32, GOLD);

    bool hasAlzheimers = ctx.CurrentHeirHasTrait(TraitType::Alzheimers);
    bool hasEidetic = ctx.CurrentHeirHasTrait(TraitType::EideticMemory);
    bool isSpelunker = (ctx.activeHeir.classType == ClassType::Spelunker_Upgraded) ||
                       (ctx.activeHeir.classType == ClassType::Miner_Base && player_.headlampOn);

    // Player's current global cell coordinates
    int pRelX = std::clamp(static_cast<int>(player_.GetCenter().x / Config::CELL_PIXEL_WIDTH), 0, curRoom.widthCells - 1);
    int pRelY = std::clamp(static_cast<int>(player_.GetCenter().y / Config::CELL_PIXEL_HEIGHT), 0, curRoom.heightCells - 1);
    int playerGx = curRoom.originGx + pRelX;
    int playerGy = curRoom.originGy + pRelY;

    int viewMinGx = playerGx - vpCells / 2;
    int viewMinGy = playerGy - vpCells / 2;

    int scX = static_cast<int>(mapX * Config::SCALE_FACTOR);
    int scY = static_cast<int>(mapY * Config::SCALE_FACTOR);
    int scW = static_cast<int>(mapPxSize * Config::SCALE_FACTOR);
    int scH = static_cast<int>(mapPxSize * Config::SCALE_FACTOR);

    BeginScissorMode(scX, scY, scW, scH);

    for (const auto& r : castle_.GetAllRooms()) {
        if (r.doorMask == 0) continue;

        // Хвороба Альцгеймера: показуємо тільки поточну кімнату гравця
        if (hasAlzheimers && r.roomId != curRoom.roomId) {
            continue;
        }

        // На карті видно ВИКЛЮЧНО ті кімнати, в яких гравець реально побував
        if (!r.visited) {
            // Якщо активовано ліхтар Spelunker, показуємо лише мітки скринь
            if (isSpelunker && (r.roomType == RoomType::FairyChest || r.roomType == RoomType::Treasure) && !r.chestOpened) {
                int rx = mapX + (r.originGx - viewMinGx) * cell;
                int ry = mapY + (r.originGy - viewMinGy) * cell;
                DrawCircle(rx + cell / 2, ry + cell / 2, 2.5f, GOLD);
            }
            continue;
        }

        int rx = mapX + (r.originGx - viewMinGx) * cell;
        int ry = mapY + (r.originGy - viewMinGy) * cell;
        int rw = r.widthCells * cell;
        int rh = r.heightCells * cell;

        // Автентичний темно-синій напівпрозорий прямокутник відвіданої кімнати
        Color fillCol = Color{ 18, 42, 88, 210 };
        Color frameCol = Color{ 140, 200, 255, 255 };

        // Кімната малюється єдиним контуром БЕЗ внутрішніх роздільних ліній
        DrawRectangle(rx + 1, ry + 1, rw - 2, rh - 2, fillCol);
        DrawRectangleLinesEx({ static_cast<float>(rx), static_cast<float>(ry), static_cast<float>(rw), static_cast<float>(rh) }, 1.5f, frameCol);

        // Білі засічки дверей (door notches) на зовнішніх стінах кімнати
        for (int sy = 0; sy < r.heightCells; ++sy) {
            for (int sx = 0; sx < r.widthCells; ++sx) {
                uint8_t dMask = r.subCellDoors[sy][sx];
                if (sy == 0 && (dMask & DoorMask::NORTH)) {
                    int dX = rx + sx * cell + cell / 2;
                    DrawRectangle(dX - 2, ry - 1, 4, 3, WHITE);
                }
                if (sy == r.heightCells - 1 && (dMask & DoorMask::SOUTH)) {
                    int dX = rx + sx * cell + cell / 2;
                    DrawRectangle(dX - 2, ry + rh - 2, 4, 3, WHITE);
                }
                if (sx == 0 && (dMask & DoorMask::WEST)) {
                    int dY = ry + sy * cell + cell / 2;
                    DrawRectangle(rx - 1, dY - 2, 3, 4, WHITE);
                }
                if (sx == r.widthCells - 1 && (dMask & DoorMask::EAST)) {
                    int dY = ry + sy * cell + cell / 2;
                    DrawRectangle(rx + rw - 2, dY - 2, 3, 4, WHITE);
                }
            }
        }

        // Іконки спеціальних кімнат по центру всієї кімнати
        int icx = rx + rw / 2;
        int icy = ry + rh / 2;
        if (r.roomType == RoomType::BossEntrance) {
            // Червоний маркер-череп боса
            DrawCircle(icx, icy - 1, 4.0f, Color{ 220, 38, 38, 255 });
            DrawRectangle(icx - 2, icy + 1, 4, 2, Color{ 180, 25, 25, 255 });
            DrawPixel(icx - 1, icy - 1, Color{ 30, 0, 0, 255 });
            DrawPixel(icx + 1, icy - 1, Color{ 30, 0, 0, 255 });
            DrawPixel(icx - 1, icy + 2, Color{ 30, 0, 0, 255 });
            DrawPixel(icx + 1, icy + 2, Color{ 30, 0, 0, 255 });
        } else if (r.roomType == RoomType::Teleporter || r.roomType == RoomType::Entrance) {
            // Блакитно-біле сяюче кільце порталу
            DrawCircle(icx, icy, 3.8f, Color{ 0, 215, 255, 255 });
            DrawCircle(icx, icy, 2.2f, Color{ 12, 45, 80, 255 });
            DrawCircle(icx, icy, 1.0f, WHITE);
        } else if ((r.roomType == RoomType::FairyChest || r.roomType == RoomType::Treasure) && !r.chestOpened) {
            // Фіолетове коло з чітким білим знаком '?'
            DrawCircle(icx, icy, 4.2f, Color{ 145, 55, 200, 255 });
            DrawCircleLines(icx, icy, 4.2f, Color{ 195, 105, 245, 255 });
            DrawPixel(icx - 1, icy - 2, WHITE);
            DrawPixel(icx,     icy - 2, WHITE);
            DrawPixel(icx + 1, icy - 1, WHITE);
            DrawPixel(icx,     icy,     WHITE);
            DrawPixel(icx,     icy + 2, WHITE);
        } else if (r.roomType == RoomType::Secret && !r.chestOpened) {
            // Маджентовий діамант
            DrawPoly({ static_cast<float>(icx), static_cast<float>(icy) }, 4, 3.5f, 45.0f, Color{ 230, 60, 180, 255 });
            DrawPixel(icx, icy, WHITE);
        }

        // Eidetic Memory: точкові позиції активних ворогів
        if (hasEidetic) {
            for (const auto& en : r.enemies) {
                if (en.active) {
                    float relXFrac = en.pos.x / static_cast<float>(r.GetPixelWidth());
                    float relYFrac = en.pos.y / static_cast<float>(r.GetPixelHeight());
                    int ex = rx + 1 + static_cast<int>(std::clamp(relXFrac, 0.05f, 0.95f) * (rw - 2));
                    int ey = ry + 1 + static_cast<int>(std::clamp(relYFrac, 0.05f, 0.95f) * (rh - 2));
                    DrawPixel(ex, ey, RED);
                }
            }
        }
    }

    // Точна позиція гравця (player dot) за дробовими координатами всередині поточної кімнати
    int curRoomMinimapX = mapX + (curRoom.originGx - viewMinGx) * cell;
    int curRoomMinimapY = mapY + (curRoom.originGy - viewMinGy) * cell;
    float fracX = (player_.GetCenter().x - static_cast<float>(Config::ROOM_OFFSET_X)) / static_cast<float>(curRoom.GetPixelWidth());
    float fracY = (player_.GetCenter().y - static_cast<float>(Config::ROOM_OFFSET_Y)) / static_cast<float>(curRoom.GetPixelHeight());
    float pDotX = curRoomMinimapX + std::clamp(fracX, 0.05f, 0.95f) * (curRoom.widthCells * cell);
    float pDotY = curRoomMinimapY + std::clamp(fracY, 0.05f, 0.95f) * (curRoom.heightCells * cell);

    DrawCircle(static_cast<int>(pDotX), static_cast<int>(pDotY), 3.2f, WHITE);
    DrawCircle(static_cast<int>(pDotX), static_cast<int>(pDotY), 2.0f, Color{ 255, 230, 80, 255 });

    EndScissorMode();

    std::string bName = Registry::GetBiomeName(curRoom.biome);
    if (ctx.CurrentHeirHasTrait(TraitType::Dyslexia)) {
        bName = Ui::ScrambleDyslexia(bName);
    }
    int bw = Ui::Measure(bName, 13);
    int bx = mapX + (mapPxSize - bw) / 2;
    Ui::Text(bName, bx, mapY + mapPxSize + 8, 13, GOLD);
}

void CastlePlayState::DrawTeleporterOverlay(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.85f));

    std::vector<const Room*> tpRooms;
    for (const auto& r : castle_.GetAllRooms()) {
        if (r.visited && (r.roomType == RoomType::Entrance ||
                          r.roomType == RoomType::Teleporter ||
                          r.roomType == RoomType::BossEntrance)) {
            tpRooms.push_back(&r);
        }
    }

    bool hasDyslexia = ctx.CurrentHeirHasTrait(TraitType::Dyslexia);

    std::string title = hasDyslexia ? Ui::ScrambleDyslexia("CASTLE TELEPORTER NETWORK") : "CASTLE TELEPORTER NETWORK";
    int tw = Ui::Measure(title.c_str(), 26);
    Ui::Text(title.c_str(), (Config::VIRTUAL_WIDTH - tw) / 2, 70, 26, SKYBLUE);

    std::string sub = hasDyslexia ? Ui::ScrambleDyslexia("Choose a destination with [WASD / ARROWS] and press [SPACE / ENTER] to Warp. [ESC] to Exit.")
                                  : "Choose a destination with [WASD / ARROWS] and press [SPACE / ENTER] to Warp. [ESC] to Exit.";
    int sw = Ui::Measure(sub.c_str(), 15);
    Ui::Text(sub.c_str(), (Config::VIRTUAL_WIDTH - sw) / 2, 106, 15, Color{ 220, 225, 235, 255 });

    int panelW = 680;
    int panelH = 480;
    int panelX = (Config::VIRTUAL_WIDTH - panelW) / 2;
    int panelY = 140;

    DrawRectangle(panelX, panelY, panelW, panelH, Color{ 16, 18, 30, 245 });
    DrawRectangleLines(panelX, panelY, panelW, panelH, SKYBLUE);

    if (tpRooms.empty()) {
        std::string emptyMsg = "No teleporters discovered yet!";
        int ew = Ui::Measure(emptyMsg.c_str(), 18);
        Ui::Text(emptyMsg.c_str(), panelX + (panelW - ew) / 2, panelY + panelH / 2 - 10, 18, GRAY);
        return;
    }

    int itemH = 46;
    int startY = panelY + 16;

    for (size_t i = 0; i < tpRooms.size(); ++i) {
        const Room* r = tpRooms[i];
        int itemY = startY + static_cast<int>(i) * (itemH + 8);
        if (itemY + itemH > panelY + panelH - 10) break;

        bool isSelected = (static_cast<int>(i) == tpIndex_);
        bool isCurrent = (r->originGx == curGx_ && r->originGy == curGy_);

        Color bgCol = isSelected ? Color{ 40, 55, 90, 255 } : Color{ 22, 26, 42, 255 };
        DrawRectangle(panelX + 16, itemY, panelW - 32, itemH, bgCol);
        if (isSelected) {
            DrawRectangleLinesEx({ static_cast<float>(panelX + 16), static_cast<float>(itemY), static_cast<float>(panelW - 32), static_cast<float>(itemH) }, 2.0f, GOLD);
        } else {
            DrawRectangleLines(panelX + 16, itemY, panelW - 32, itemH, Color{ 45, 55, 75, 255 });
        }

        // Icon
        int tpx = panelX + 38;
        int tpy = itemY + itemH / 2;
        if (r->roomType == RoomType::BossEntrance) {
            DrawCircle(tpx, tpy - 2, 8.5f, Color{ 220, 38, 38, 255 });
            DrawCircleLines(tpx, tpy - 2, 8.5f, Color{ 255, 120, 120, 255 });
            DrawRectangle(tpx - 4, tpy + 2, 9, 6, Color{ 180, 25, 25, 255 });
            DrawRectangleLines(tpx - 4, tpy + 2, 9, 6, Color{ 255, 120, 120, 255 });
            DrawCircle(tpx - 3, tpy - 3, 2.0f, Color{ 25, 0, 0, 255 });
            DrawCircle(tpx + 3, tpy - 3, 2.0f, Color{ 25, 0, 0, 255 });
            DrawPixel(tpx - 2, tpy + 5, BLACK);
            DrawPixel(tpx,     tpy + 5, BLACK);
            DrawPixel(tpx + 2, tpy + 5, BLACK);
        } else {
            // Cyan-white portal ring
            DrawCircle(tpx, tpy, 9.0f, Color{ 0, 215, 255, 255 });
            DrawCircleLines(tpx, tpy, 9.0f, Color{ 180, 245, 255, 255 });
            DrawCircle(tpx, tpy, 5.0f, Color{ 12, 45, 80, 255 });
            DrawCircle(tpx, tpy, 2.5f, WHITE);
        }

        // Name
        std::string name;
        if (r->roomType == RoomType::Entrance) {
            name = "Castle Entrance (Gate of Johannes)";
        } else if (r->roomType == RoomType::BossEntrance) {
            switch (r->bossType) {
                case BossType::Khidr:           name = "Boss Lair: Khidr"; break;
                case BossType::Alexander:       name = "Boss Lair: Alexander"; break;
                case BossType::PonceDeLeon:     name = "Boss Lair: Ponce de Leon"; break;
                case BossType::Herodotus:       name = "Boss Lair: Herodotus"; break;
                case BossType::JohannesTraitor: name = "Boss Lair: Johannes the Traitor"; break;
                default:                        name = "Boss Lair"; break;
            }
        } else {
            name = Registry::GetBiomeName(r->biome) + " Teleporter";
        }

        if (hasDyslexia) {
            name = Ui::ScrambleDyslexia(name);
        }

        Color textCol = isSelected ? GOLD : WHITE;
        Ui::Text(name.c_str(), panelX + 56, itemY + 14, 16, textCol);

        if (isCurrent) {
            std::string curText = "[CURRENT]";
            int ctw = Ui::Measure(curText.c_str(), 13);
            Ui::Text(curText.c_str(), panelX + panelW - 32 - ctw - 12, itemY + 16, 13, GREEN);
        }
    }
}