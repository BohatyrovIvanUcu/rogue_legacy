// === FILE: src/states/SettingsMenu.cpp ===
#include "states/SettingsMenu.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/UiText.hpp"
#include <raylib.h>
#include <cmath>
#include <algorithm>

void SettingsMenu::Open(bool inGamePause) {
    isOpen_ = true;
    inGamePause_ = inGamePause;
    isRebinding_ = false;
    animTimer_ = 0.0f;
    statusMessage_ = "";
    statusMessageTimer_ = 0.0f;
}

void SettingsMenu::Close(GameContext* ctx) {
    isOpen_ = false;
    isRebinding_ = false;
    if (ctx) {
        SaveManager::SaveSettings(*ctx, Config::SAVE_FILE_PATH);
    }
}

void SettingsMenu::Update(float dt, GameContext& ctx) {
    if (!isOpen_) return;

    animTimer_ += dt;
    if (statusMessageTimer_ > 0.0f) {
        statusMessageTimer_ -= dt;
        if (statusMessageTimer_ <= 0.0f) statusMessage_ = "";
    }

    // 1. РЕЖИМ ПЕРЕПРИЗНАЧЕННЯ КЛАВІШІ (REBIND MODE)
    if (isRebinding_) {
        rebindPulseTimer_ += dt;

        // Скасування по Escape
        if (IsKeyPressed(KEY_ESCAPE)) {
            isRebinding_ = false;
            statusMessage_ = "REBIND CANCELLED";
            statusMessageTimer_ = 1.5f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail, 0.9f, 0.5f);
            return;
        }

        // Очищення клавіші по Delete / Backspace
        if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
            auto& b = ctx.input.bindings[static_cast<size_t>(selectedActionIdx_)];
            if (selectedSlot_ == 0) b.primary = KEY_NULL;
            else b.secondary = KEY_NULL;

            isRebinding_ = false;
            statusMessage_ = "BINDING CLEARED";
            statusMessageTimer_ = 1.5f;
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.5f);
            return;
        }

        // Перевірка клавіатури
        int key = GetKeyPressed();
        if (key > 0 && key != KEY_ESCAPE) {
            auto& b = ctx.input.bindings[static_cast<size_t>(selectedActionIdx_)];
            if (selectedSlot_ == 0) b.primary = key;
            else b.secondary = key;

            isRebinding_ = false;
            statusMessage_ = std::string(InputConfig::GetActionName(static_cast<InputAction>(selectedActionIdx_))) +
                             " BOUND TO [" + InputConfig::GetKeyDisplayName(key) + "]";
            statusMessageTimer_ = 2.0f;
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.2f, 0.7f);
            return;
        }

        // Перевірка кнопок миші (ЛКМ, ПКМ, СКМ)
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int code = MOUSE_CODE_OFFSET + MOUSE_BUTTON_LEFT;
            auto& b = ctx.input.bindings[static_cast<size_t>(selectedActionIdx_)];
            if (selectedSlot_ == 0) b.primary = code;
            else b.secondary = code;

            isRebinding_ = false;
            statusMessage_ = "BOUND TO [LMB]";
            statusMessageTimer_ = 2.0f;
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.2f, 0.7f);
            return;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            int code = MOUSE_CODE_OFFSET + MOUSE_BUTTON_RIGHT;
            auto& b = ctx.input.bindings[static_cast<size_t>(selectedActionIdx_)];
            if (selectedSlot_ == 0) b.primary = code;
            else b.secondary = code;

            isRebinding_ = false;
            statusMessage_ = "BOUND TO [RMB]";
            statusMessageTimer_ = 2.0f;
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.2f, 0.7f);
            return;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) {
            int code = MOUSE_CODE_OFFSET + MOUSE_BUTTON_MIDDLE;
            auto& b = ctx.input.bindings[static_cast<size_t>(selectedActionIdx_)];
            if (selectedSlot_ == 0) b.primary = code;
            else b.secondary = code;

            isRebinding_ = false;
            statusMessage_ = "BOUND TO [MMB]";
            statusMessageTimer_ = 2.0f;
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.2f, 0.7f);
            return;
        }

        return; // У режимі перепризначення блокуємо іншу навігацію
    }

    // 2. ПЕРЕМИКАННЯ ВКЛАДОК (TAB / CLICK)
    if (IsKeyPressed(KEY_TAB)) {
        currentTab_ = 1 - currentTab_;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
    }

    // Закриття налаштувань по Escape
    if (IsKeyPressed(KEY_ESCAPE)) {
        Close(&ctx);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
        return;
    }

    // 3. ОБРОБКА ВКЛАДКИ КЕРУВАННЯ (TAB 0)
    if (currentTab_ == 0) {
        // Навігація стрілками або W/S/A/D
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            selectedActionIdx_ = (selectedActionIdx_ + static_cast<int>(InputAction::COUNT) - 1) % static_cast<int>(InputAction::COUNT);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            selectedActionIdx_ = (selectedActionIdx_ + 1) % static_cast<int>(InputAction::COUNT);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            selectedSlot_ = 1 - selectedSlot_;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.85f, 0.3f);
        }

        // Початок перепризначення по Enter або Space
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            isRebinding_ = true;
            rebindPulseTimer_ = 0.0f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.0f, 0.6f);
        }
    }
    // 4. ОБРОБКА ВКЛАДКИ АУДІО ТА ЕКРАНУ (TAB 1)
    else {
        static const int FPS_PRESETS[] = { 60, 120, 144, 240, 0 };
        auto getFpsIdx = [&](int fps) {
            for (int f = 0; f < 5; ++f) {
                if (fps == FPS_PRESETS[f]) return f;
            }
            return 0;
        };

        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            audioSelectedRow_ = (audioSelectedRow_ + 6) % 7;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
            audioSelectedRow_ = (audioSelectedRow_ + 1) % 7;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
        }

        // Зміна значень ліворуч / праворуч
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            if (audioSelectedRow_ == 0) {
                ctx.musicMuted = !ctx.musicMuted;
            } else if (audioSelectedRow_ == 1) {
                ctx.musicVolume = std::clamp(ctx.musicVolume - 0.1f, 0.0f, 1.0f);
                if (ctx.audio) ctx.audio->SetMusicVolume(ctx.musicVolume);
            } else if (audioSelectedRow_ == 2) {
                ctx.sfxMuted = !ctx.sfxMuted;
                if (ctx.audio) ctx.audio->SetSfxMuted(ctx.sfxMuted);
            } else if (audioSelectedRow_ == 3) {
                ctx.sfxVolume = std::clamp(ctx.sfxVolume - 0.1f, 0.0f, 1.0f);
                if (ctx.audio) ctx.audio->SetSfxVolume(ctx.sfxVolume);
            } else if (audioSelectedRow_ == 4) {
                ToggleBorderlessWindowed();
            } else if (audioSelectedRow_ == 5) {
                ctx.showFps = !ctx.showFps;
            } else if (audioSelectedRow_ == 6) {
                int idx = getFpsIdx(ctx.targetFps);
                idx = (idx + 4) % 5;
                ctx.targetFps = FPS_PRESETS[idx];
                SetTargetFPS(ctx.targetFps > 0 ? ctx.targetFps : 0);
            }
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (audioSelectedRow_ == 0) {
                ctx.musicMuted = !ctx.musicMuted;
            } else if (audioSelectedRow_ == 1) {
                ctx.musicVolume = std::clamp(ctx.musicVolume + 0.1f, 0.0f, 1.0f);
                if (ctx.audio) ctx.audio->SetMusicVolume(ctx.musicVolume);
            } else if (audioSelectedRow_ == 2) {
                ctx.sfxMuted = !ctx.sfxMuted;
                if (ctx.audio) ctx.audio->SetSfxMuted(ctx.sfxMuted);
            } else if (audioSelectedRow_ == 3) {
                ctx.sfxVolume = std::clamp(ctx.sfxVolume + 0.1f, 0.0f, 1.0f);
                if (ctx.audio) ctx.audio->SetSfxVolume(ctx.sfxVolume);
            } else if (audioSelectedRow_ == 4) {
                ToggleBorderlessWindowed();
            } else if (audioSelectedRow_ == 5) {
                ctx.showFps = !ctx.showFps;
            } else if (audioSelectedRow_ == 6) {
                int idx = getFpsIdx(ctx.targetFps);
                idx = (idx + 1) % 5;
                ctx.targetFps = FPS_PRESETS[idx];
                SetTargetFPS(ctx.targetFps > 0 ? ctx.targetFps : 0);
            }
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.0f, 0.4f);
        }
    }
}

void SettingsMenu::Draw(GameContext& ctx) {
    if (!isOpen_) return;

    Vector2 mousePos = GetMousePosition();

    // 1. Напівпрозорий затіняючий бекдроп
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Color{ 8, 10, 18, 235 });

    // 2. Головне вікно налаштувань
    int boxW = 860;
    int boxH = 580;
    int boxX = (Config::VIRTUAL_WIDTH - boxW) / 2;
    int boxY = 40;

    Rectangle panelRec = { static_cast<float>(boxX), static_cast<float>(boxY), static_cast<float>(boxW), static_cast<float>(boxH) };
    DrawRectangleRounded(panelRec, 0.04f, 4, Color{ 16, 20, 32, 250 });
    DrawRectangleRoundedLines(panelRec, 0.04f, 4, 3.0f, GOLD);

    // Золоті кути
    DrawLineEx({ panelRec.x, panelRec.y + 24.0f }, { panelRec.x, panelRec.y }, 4.0f, GOLD);
    DrawLineEx({ panelRec.x, panelRec.y }, { panelRec.x + 24.0f, panelRec.y }, 4.0f, GOLD);
    DrawLineEx({ panelRec.x + panelRec.width - 24.0f, panelRec.y }, { panelRec.x + panelRec.width, panelRec.y }, 4.0f, GOLD);
    DrawLineEx({ panelRec.x + panelRec.width, panelRec.y }, { panelRec.x + panelRec.width, panelRec.y + 24.0f }, 4.0f, GOLD);
    DrawLineEx({ panelRec.x, panelRec.y + panelRec.height - 24.0f }, { panelRec.x, panelRec.y + panelRec.height }, 4.0f, GOLD);
    DrawLineEx({ panelRec.x, panelRec.y + panelRec.height }, { panelRec.x + 24.0f, panelRec.y + panelRec.height }, 4.0f, GOLD);
    DrawLineEx({ panelRec.x + panelRec.width - 24.0f, panelRec.y + panelRec.height }, { panelRec.x + panelRec.width, panelRec.y + panelRec.height }, 4.0f, GOLD);
    DrawLineEx({ panelRec.x + panelRec.width, panelRec.y + panelRec.height - 24.0f }, { panelRec.x + panelRec.width, panelRec.y + panelRec.height }, 4.0f, GOLD);

    // Заголовок
    const char* title = inGamePause_ ? "GAME PAUSED - SETTINGS" : "SETTINGS & CONTROLS";
    int tw = Ui::Measure(title, 26);
    Ui::Text(title, boxX + (boxW - tw) / 2 + 2, boxY + 18 + 2, 26, BLACK);
    Ui::Text(title, boxX + (boxW - tw) / 2, boxY + 18, 26, GOLD);

    // 3. Кнопки вкладок (Tabs)
    int tabW = 240;
    int tabH = 34;
    int tab1X = boxX + boxW / 2 - tabW - 12;
    int tab2X = boxX + boxW / 2 + 12;
    int tabY = boxY + 60;

    Rectangle tab1Rec = { static_cast<float>(tab1X), static_cast<float>(tabY), static_cast<float>(tabW), static_cast<float>(tabH) };
    Rectangle tab2Rec = { static_cast<float>(tab2X), static_cast<float>(tabY), static_cast<float>(tabW), static_cast<float>(tabH) };

    bool hoverTab1 = CheckCollisionPointRec(mousePos, tab1Rec);
    bool hoverTab2 = CheckCollisionPointRec(mousePos, tab2Rec);

    if (hoverTab1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        currentTab_ = 0;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
    }
    if (hoverTab2 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        currentTab_ = 1;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
    }

    // Відмальовка Вкладки 1
    DrawRectangleRounded(tab1Rec, 0.25f, 4, (currentTab_ == 0) ? Color{ 70, 52, 20, 240 } : Color{ 25, 30, 45, 200 });
    DrawRectangleRoundedLines(tab1Rec, 0.25f, 4, 2.0f, (currentTab_ == 0) ? GOLD : (hoverTab1 ? SKYBLUE : Color{ 80, 90, 115, 255 }));
    int t1W = Ui::Measure("CONTROLS / KEYS", 15);
    Ui::Text("CONTROLS / KEYS", tab1X + (tabW - t1W) / 2, tabY + 8, 15, (currentTab_ == 0) ? GOLD : RAYWHITE);

    // Відмальовка Вкладки 2
    DrawRectangleRounded(tab2Rec, 0.25f, 4, (currentTab_ == 1) ? Color{ 70, 52, 20, 240 } : Color{ 25, 30, 45, 200 });
    DrawRectangleRoundedLines(tab2Rec, 0.25f, 4, 2.0f, (currentTab_ == 1) ? GOLD : (hoverTab2 ? SKYBLUE : Color{ 80, 90, 115, 255 }));
    int t2W = Ui::Measure("AUDIO & DISPLAY", 15);
    Ui::Text("AUDIO & DISPLAY", tab2X + (tabW - t2W) / 2, tabY + 8, 15, (currentTab_ == 1) ? GOLD : RAYWHITE);

    // 4. КОНТЕНТ ВКЛАДКИ КЕРУВАННЯ (TAB 0)
    if (currentTab_ == 0) {
        int listStartY = boxY + 112;
        int rowH = 32;

        // Заголовки стовпців
        Ui::Text("ACTION NAME", boxX + 48, listStartY - 20, 14, SKYBLUE);
        Ui::Text("PRIMARY KEY", boxX + 440, listStartY - 20, 14, SKYBLUE);
        Ui::Text("SECONDARY KEY", boxX + 640, listStartY - 20, 14, SKYBLUE);

        DrawLine(boxX + 40, listStartY - 4, boxX + boxW - 40, listStartY - 4, Color{ 70, 80, 110, 255 });

        for (int i = 0; i < static_cast<int>(InputAction::COUNT); ++i) {
            InputAction act = static_cast<InputAction>(i);
            const auto& binding = ctx.input.bindings[static_cast<size_t>(i)];
            int ry = listStartY + i * rowH;

            bool isRowSelected = (i == selectedActionIdx_);

            // Фон рядка
            Rectangle rowRec = { static_cast<float>(boxX + 36), static_cast<float>(ry - 2), static_cast<float>(boxW - 72), static_cast<float>(rowH - 4) };
            if (isRowSelected) {
                DrawRectangleRounded(rowRec, 0.2f, 4, Color{ 40, 45, 65, 220 });
            }

            // Назва дії
            Ui::Text(InputConfig::GetActionName(act), boxX + 48, ry + 3, 14, isRowSelected ? GOLD : RAYWHITE);

            // Кнопка Primary Key
            Rectangle primRec = { static_cast<float>(boxX + 430), static_cast<float>(ry), 150.0f, 24.0f };
            bool hoverPrim = CheckCollisionPointRec(mousePos, primRec);
            bool isRebindingPrim = (isRebinding_ && isRowSelected && selectedSlot_ == 0);

            if (hoverPrim && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isRebinding_) {
                selectedActionIdx_ = i;
                selectedSlot_ = 0;
                isRebinding_ = true;
                rebindPulseTimer_ = 0.0f;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.0f, 0.6f);
            }

            Color primBg = isRebindingPrim
                ? Color{ 140, 95, 20, 255 }
                : ((isRowSelected && selectedSlot_ == 0) ? Color{ 60, 50, 25, 255 } : Color{ 22, 26, 40, 255 });
            DrawRectangleRounded(primRec, 0.25f, 4, primBg);

            Color primBorder = isRebindingPrim ? (std::fmod(rebindPulseTimer_ * 5.0f, 1.0f) < 0.5f ? GOLD : SKYBLUE)
                : ((isRowSelected && selectedSlot_ == 0) ? GOLD : (hoverPrim ? SKYBLUE : Color{ 75, 85, 110, 255 }));
            DrawRectangleRoundedLines(primRec, 0.25f, 4, 1.5f, primBorder);

            std::string primText = isRebindingPrim ? "PRESS ANY..." : InputConfig::GetKeyDisplayName(binding.primary);
            int ptw = Ui::Measure(primText.c_str(), 13);
            Ui::Text(primText.c_str(), primRec.x + (primRec.width - ptw) * 0.5f, primRec.y + 4, 13, isRebindingPrim ? GOLD : RAYWHITE);

            // Кнопка Secondary Key
            Rectangle secRec = { static_cast<float>(boxX + 630), static_cast<float>(ry), 150.0f, 24.0f };
            bool hoverSec = CheckCollisionPointRec(mousePos, secRec);
            bool isRebindingSec = (isRebinding_ && isRowSelected && selectedSlot_ == 1);

            if (hoverSec && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isRebinding_) {
                selectedActionIdx_ = i;
                selectedSlot_ = 1;
                isRebinding_ = true;
                rebindPulseTimer_ = 0.0f;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.0f, 0.6f);
            }

            Color secBg = isRebindingSec
                ? Color{ 140, 95, 20, 255 }
                : ((isRowSelected && selectedSlot_ == 1) ? Color{ 60, 50, 25, 255 } : Color{ 22, 26, 40, 255 });
            DrawRectangleRounded(secRec, 0.25f, 4, secBg);

            Color secBorder = isRebindingSec ? (std::fmod(rebindPulseTimer_ * 5.0f, 1.0f) < 0.5f ? GOLD : SKYBLUE)
                : ((isRowSelected && selectedSlot_ == 1) ? GOLD : (hoverSec ? SKYBLUE : Color{ 75, 85, 110, 255 }));
            DrawRectangleRoundedLines(secRec, 0.25f, 4, 1.5f, secBorder);

            std::string secText = isRebindingSec ? "PRESS ANY..." : InputConfig::GetKeyDisplayName(binding.secondary);
            int stw = Ui::Measure(secText.c_str(), 13);
            Ui::Text(secText.c_str(), secRec.x + (secRec.width - stw) * 0.5f, secRec.y + 4, 13, isRebindingSec ? GOLD : RAYWHITE);
        }

        // Кнопка Reset to Defaults
        Rectangle resetRec = { static_cast<float>(boxX + 48), static_cast<float>(boxY + boxH - 58), 200.0f, 36.0f };
        bool hoverReset = CheckCollisionPointRec(mousePos, resetRec);
        if (hoverReset && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isRebinding_) {
            ctx.input.SetDefaults();
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            statusMessage_ = "DEFAULTS RESTORED";
            statusMessageTimer_ = 2.0f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 1.0f, 0.6f);
        }
        DrawRectangleRounded(resetRec, 0.25f, 4, hoverReset ? Color{ 85, 40, 30, 240 } : Color{ 35, 30, 45, 220 });
        DrawRectangleRoundedLines(resetRec, 0.25f, 4, 2.0f, hoverReset ? RED : Color{ 120, 70, 70, 255 });
        int rw = Ui::Measure("RESET DEFAULTS", 14);
        Ui::Text("RESET DEFAULTS", resetRec.x + (resetRec.width - rw) * 0.5f, resetRec.y + 9, 14, hoverReset ? RED : Color{ 220, 160, 160, 255 });
    }
    // 5. КОНТЕНТ ВКЛАДКИ АУДІО ТА ЕКРАНУ (TAB 1)
    else {
        int listStartY = boxY + 112;
        int rowH = 46;

        struct AudioRow {
            std::string title;
            std::string value;
        };

        static const int FPS_PRESETS[] = { 60, 120, 144, 240, 0 };
        auto getFpsIdx = [&](int fps) {
            for (int f = 0; f < 5; ++f) {
                if (fps == FPS_PRESETS[f]) return f;
            }
            return 0;
        };

        std::string musVolStr = std::to_string(static_cast<int>(std::round(ctx.musicVolume * 100.0f))) + "%";
        std::string sfxVolStr = std::to_string(static_cast<int>(std::round(ctx.sfxVolume * 100.0f))) + "%";
        std::string fpsLimitStr = (ctx.targetFps == 0) ? "UNLIMITED" : (std::to_string(ctx.targetFps) + " FPS");

        AudioRow rows[7] = {
            { "MUSIC SEQUENCER", ctx.musicMuted ? "MUTED [P]" : "ENABLED" },
            { "MUSIC VOLUME", musVolStr },
            { "SFX (SOUND EFFECTS)", ctx.sfxMuted ? "MUTED" : "ENABLED" },
            { "SFX VOLUME", sfxVolStr },
            { "DISPLAY MODE", "TOGGLE BORDERLESS FULLSCREEN [F11]" },
            { "SHOW FPS COUNTER", ctx.showFps ? "ENABLED [F3]" : "DISABLED [F3]" },
            { "FPS LIMIT", fpsLimitStr }
        };

        for (int i = 0; i < 7; ++i) {
            int ry = listStartY + i * rowH;
            bool sel = (i == audioSelectedRow_);

            Rectangle rowRec = { static_cast<float>(boxX + 50), static_cast<float>(ry), static_cast<float>(boxW - 100), 38.0f };
            bool hoverRow = CheckCollisionPointRec(mousePos, rowRec);
            if (hoverRow && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                audioSelectedRow_ = i;
                if (i == 0) {
                    ctx.musicMuted = !ctx.musicMuted;
                } else if (i == 2) {
                    ctx.sfxMuted = !ctx.sfxMuted;
                    if (ctx.audio) ctx.audio->SetSfxMuted(ctx.sfxMuted);
                } else if (i == 4) {
                    ToggleBorderlessWindowed();
                } else if (i == 5) {
                    ctx.showFps = !ctx.showFps;
                } else if (i == 6) {
                    int idx = getFpsIdx(ctx.targetFps);
                    idx = (idx + 1) % 5;
                    ctx.targetFps = FPS_PRESETS[idx];
                    SetTargetFPS(ctx.targetFps > 0 ? ctx.targetFps : 0);
                }
                SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.0f, 0.4f);
            }

            DrawRectangleRounded(rowRec, 0.2f, 4, sel ? Color{ 45, 52, 75, 230 } : Color{ 22, 26, 38, 200 });
            DrawRectangleRoundedLines(rowRec, 0.2f, 4, 1.8f, sel ? GOLD : (hoverRow ? SKYBLUE : Color{ 60, 70, 95, 255 }));

            Ui::Text(rows[i].title.c_str(), rowRec.x + 20, rowRec.y + 10, 15, sel ? GOLD : RAYWHITE);

            // Кнопки регулювання гучності [-] / [+]
            if (i == 1 || i == 3) {
                float volVal = (i == 1) ? ctx.musicVolume : ctx.sfxVolume;

                // Смужка гучності
                Rectangle barRec = { rowRec.x + rowRec.width - 260.0f, rowRec.y + 9.0f, 150.0f, 20.0f };
                DrawRectangleRounded(barRec, 0.2f, 4, Color{ 15, 18, 28, 255 });
                DrawRectangleRoundedLines(barRec, 0.2f, 4, 1.0f, Color{ 80, 90, 115, 255 });

                Rectangle fillRec = { barRec.x + 2.0f, barRec.y + 2.0f, (barRec.width - 4.0f) * volVal, barRec.height - 4.0f };
                DrawRectangleRounded(fillRec, 0.2f, 4, (i == 1) ? SKYBLUE : GOLD);

                std::string vStr = rows[i].value;
                int vw = Ui::Measure(vStr.c_str(), 14);
                Ui::Text(vStr.c_str(), rowRec.x + rowRec.width - 90 + (80 - vw) / 2, rowRec.y + 11, 14, RAYWHITE);
            } else {
                int vw = Ui::Measure(rows[i].value.c_str(), 15);
                Ui::Text(rows[i].value.c_str(), rowRec.x + rowRec.width - vw - 24, rowRec.y + 10, 15, sel ? SKYBLUE : GOLD);
            }
        }
    }

    // 6. Кнопка "BACK / APPLY" у правому нижньому кутку
    Rectangle backRec = { static_cast<float>(boxX + boxW - 220), static_cast<float>(boxY + boxH - 58), 170.0f, 36.0f };
    bool hoverBack = CheckCollisionPointRec(mousePos, backRec);
    if (hoverBack && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isRebinding_) {
        Close(&ctx);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
    }
    DrawRectangleRounded(backRec, 0.25f, 4, hoverBack ? Color{ 75, 58, 24, 250 } : Color{ 35, 40, 60, 220 });
    DrawRectangleRoundedLines(backRec, 0.25f, 4, 2.0f, hoverBack ? GOLD : SKYBLUE);
    int bw = Ui::Measure("BACK / APPLY", 14);
    Ui::Text("BACK / APPLY", backRec.x + (backRec.width - bw) * 0.5f, backRec.y + 9, 14, hoverBack ? GOLD : RAYWHITE);

    // Додаткові кнопки для паузи в грі: Quit to Title
    if (inGamePause_) {
        Rectangle quitRec = { static_cast<float>(boxX + 270), static_cast<float>(boxY + boxH - 58), 200.0f, 36.0f };
        bool hoverQuit = CheckCollisionPointRec(mousePos, quitRec);
        if (hoverQuit && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isRebinding_) {
            Close(&ctx);
            ctx.RequestStateChange(GameStateId::Title);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
        }
        DrawRectangleRounded(quitRec, 0.25f, 4, hoverQuit ? Color{ 85, 30, 30, 240 } : Color{ 40, 28, 35, 220 });
        DrawRectangleRoundedLines(quitRec, 0.25f, 4, 2.0f, hoverQuit ? RED : Color{ 140, 70, 70, 255 });
        int qw = Ui::Measure("QUIT TO TITLE", 14);
        Ui::Text("QUIT TO TITLE", quitRec.x + (quitRec.width - qw) * 0.5f, quitRec.y + 9, 14, hoverQuit ? RED : Color{ 220, 170, 170, 255 });
    }

    // 7. Повідомлення про статус (наприклад "REBIND CANCELLED" або "SAVED")
    if (!statusMessage_.empty()) {
        int smw = Ui::Measure(statusMessage_.c_str(), 16);
        int smx = boxX + (boxW - smw) / 2;
        int smy = boxY + boxH - 50;
        DrawRectangle(smx - 12, smy - 4, smw + 24, 26, Color{ 10, 14, 22, 230 });
        DrawRectangleLines(smx - 12, smy - 4, smw + 24, 26, GOLD);
        Ui::Text(statusMessage_.c_str(), smx, smy, 16, GOLD);
    }
}
