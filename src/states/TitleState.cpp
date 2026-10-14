// === FILE: src/states/TitleState.cpp ===
#include "states/TitleState.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/UiText.hpp"
#include <raylib.h>
#include <cmath>
#include <string>

void TitleState::OnEnter(GameContext& ctx) {
    hasSaveFile_ = SaveManager::SaveFileExists(Config::SAVE_FILE_PATH);
    selectedIndex_ = 0;
    animTimer_ = 0.0f;
    settingsMenu_.Close();
    if (ctx.audio) {
        ctx.audio->SetBossMusic(false);
    }
}

void TitleState::OnExit(GameContext& /*ctx*/) {
    settingsMenu_.Close();
}

void TitleState::Update(float dt, GameContext& ctx) {
    animTimer_ += dt;

    // Якщо відкрито меню налаштувань — передаємо керування йому
    if (settingsMenu_.IsOpen()) {
        settingsMenu_.Update(dt, ctx);
        return;
    }

    Vector2 mousePos = GetMousePosition();

    // Навігація клавіатурою
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
        selectedIndex_ = (selectedIndex_ + 3) % 4;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.5f);
    }
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
        selectedIndex_ = (selectedIndex_ + 1) % 4;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.5f);
    }

    // Швидкий перехід у Дерево маєтку по [M]
    if (IsKeyPressed(KEY_M)) {
        if (hasSaveFile_) {
            SaveManager::LoadGame(ctx, Config::SAVE_FILE_PATH);
        }
        ctx.RequestStateChange(GameStateId::Manor);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.0f, 0.8f);
        return;
    }

    // Перевірка кліків миші по пунктах меню
    int boxW = 500;
    int boxX = (Config::VIRTUAL_WIDTH - boxW) / 2;
    int boxY = 244;
    for (int i = 0; i < 4; ++i) {
        int oy = boxY + 20 + i * 46;
        Rectangle optRec = { static_cast<float>(boxX + 16), static_cast<float>(oy - 2), static_cast<float>(boxW - 32), 36.0f };
        if (CheckCollisionPointRec(mousePos, optRec)) {
            if (selectedIndex_ != i) {
                selectedIndex_ = i;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.3f);
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                // Активуємо обраний пункт меню
                goto ActivateSelection;
            }
        }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
ActivateSelection:
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 1.0f, 0.8f);

        if (selectedIndex_ == 0) {
            // Continue Lineage (або старт, якщо збереження ще не було)
            if (hasSaveFile_) {
                SaveManager::LoadGame(ctx, Config::SAVE_FILE_PATH);
            } else {
                ctx.ResetToDefaultNewGame();
                SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
            }
            ctx.RequestStateChange(GameStateId::Lineage);
        } else if (selectedIndex_ == 1) {
            // New Lineage (повний скид прогресії)
            ctx.ResetToDefaultNewGame();
            SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
            ctx.RequestStateChange(GameStateId::Lineage);
        } else if (selectedIndex_ == 2) {
            // Меню налаштувань та перепризначення клавіш
            settingsMenu_.Open(false);
        } else if (selectedIndex_ == 3) {
            ctx.shouldQuit = true;
        }
    }
}

void TitleState::Draw(GameContext& ctx) {
    if (ctx.renderer) {
        ctx.renderer->DrawManorCastle(ctx, animTimer_);
    } else {
        ClearBackground(Color{ 14, 16, 28, 255 });
    }

    // Якщо відкрито меню налаштувань — малюємо його поверх і виходимо
    if (settingsMenu_.IsOpen()) {
        settingsMenu_.Draw(ctx);
        return;
    }

    // М'який градієнт для читабельності логотипу та меню
    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, 220, Fade(BLACK, 0.65f), BLANK);

    const char* title = "ROGUE LEGACY C++20";
    int titleSize = 56;
    int titleW = Ui::Measure(title, titleSize);
    int titleX = (Config::VIRTUAL_WIDTH - titleW) / 2;
    int titleY = 56 + static_cast<int>(std::sin(animTimer_ * 2.5f) * 4.0f);

    // Золотий геральдичний банер під назвою гри
    Rectangle titleBanner = { static_cast<float>(titleX - 24), static_cast<float>(titleY - 8), static_cast<float>(titleW + 48), 68.0f };
    DrawRectangleRounded(titleBanner, 0.35f, 4, Color{ 16, 18, 30, 220 });
    DrawRectangleRoundedLines(titleBanner, 0.35f, 4, 2.5f, GOLD);

    Ui::Text(title, titleX + 3, titleY + 3, titleSize, BLACK);
    Ui::Text(title, titleX, titleY, titleSize, GOLD);

    const char* subtitle = "PROCEDURAL GOTHIC ACTION-ROGUELITE  *  ZERO EXTERNAL ASSETS";
    int subW = Ui::Measure(subtitle, 16);
    Ui::Text(subtitle, (Config::VIRTUAL_WIDTH - subW) / 2, titleY + 74, 16, SKYBLUE);

    // Панель головного меню
    int boxW = 500;
    int boxH = 224;
    int boxX = (Config::VIRTUAL_WIDTH - boxW) / 2;
    int boxY = 244;
    Rectangle menuRec = { static_cast<float>(boxX), static_cast<float>(boxY), static_cast<float>(boxW), static_cast<float>(boxH) };
    DrawRectangleRounded(menuRec, 0.08f, 4, Color{ 16, 18, 30, 240 });
    DrawRectangleRoundedLines(menuRec, 0.08f, 4, 3.0f, GOLD);

    // Золоті кутові акценти
    DrawLineEx({ menuRec.x, menuRec.y + 18.0f }, { menuRec.x, menuRec.y }, 3.5f, GOLD);
    DrawLineEx({ menuRec.x, menuRec.y }, { menuRec.x + 18.0f, menuRec.y }, 3.5f, GOLD);
    DrawLineEx({ menuRec.x + menuRec.width - 18.0f, menuRec.y }, { menuRec.x + menuRec.width, menuRec.y }, 3.5f, GOLD);
    DrawLineEx({ menuRec.x + menuRec.width, menuRec.y }, { menuRec.x + menuRec.width, menuRec.y + 18.0f }, 3.5f, GOLD);

    std::string opt0 = hasSaveFile_
        ? ("CONTINUE LINEAGE (GEN " + std::to_string(ctx.generationCount) + " / NG+" + std::to_string(ctx.ngPlusLevel) + ")")
        : "START LINEAGE";
    std::string opt1 = "NEW LINEAGE (RESET PROGRESS)";
    std::string opt2 = "SETTINGS & KEYBINDINGS";
    std::string opt3 = "EXIT TO DESKTOP";

    const std::string options[4] = { opt0, opt1, opt2, opt3 };
    for (int i = 0; i < 4; ++i) {
        bool sel = (i == selectedIndex_);
        int oy = boxY + 20 + i * 46;
        Rectangle optRec = { static_cast<float>(boxX + 16), static_cast<float>(oy - 2), static_cast<float>(boxW - 32), 36.0f };
        if (sel) {
            DrawRectangleRounded(optRec, 0.25f, 4, Color{ 75, 58, 24, 235 });
            DrawRectangleRoundedLines(optRec, 0.25f, 4, 2.0f, GOLD);
        }
        Color col = sel ? GOLD : RAYWHITE;
        std::string label = (sel ? "> " : "  ") + options[i];
        Ui::Text(label.c_str(), boxX + 28, oy + 6, 17, col);
    }

    // Акуратний лаконічний футер керування внизу екрана
    int footW = 860;
    int footX = (Config::VIRTUAL_WIDTH - footW) / 2;
    int footY = 610;
    Rectangle footRec = { static_cast<float>(footX), static_cast<float>(footY), static_cast<float>(footW), 36.0f };
    DrawRectangleRounded(footRec, 0.3f, 4, Color{ 14, 16, 26, 210 });
    DrawRectangleRoundedLines(footRec, 0.3f, 4, 1.5f, Color{ 80, 90, 115, 200 });

    const char* footText = "[ENTER / SPACE] Select       [W / S / ARROWS] Navigate       [M] Manor Tree       [ESC] Quit";
    int ftw = Ui::Measure(footText, 14);
    Ui::Text(footText, footX + (footW - ftw) / 2, footY + 10, 14, Color{ 200, 215, 240, 255 });
}