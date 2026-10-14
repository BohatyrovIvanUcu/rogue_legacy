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

Vector2 SettingsMenu::GetVirtualMousePos() {
    float sw = static_cast<float>(GetScreenWidth());
    float sh = static_cast<float>(GetScreenHeight());
    float scale = std::min(sw / Config::RENDER_WIDTH, sh / Config::RENDER_HEIGHT);
    if (scale <= 0.0001f) scale = 1.0f;
    float dw = Config::RENDER_WIDTH * scale;
    float dh = Config::RENDER_HEIGHT * scale;
    float ox = (sw - dw) * 0.5f;
    float oy = (sh - dh) * 0.5f;

    Vector2 raw = GetMousePosition();
    Vector2 canvasPos = { (raw.x - ox) / scale, (raw.y - oy) / scale };
    return { canvasPos.x / Config::SCALE_FACTOR, canvasPos.y / Config::SCALE_FACTOR };
}

extern "C" void glfwSwapInterval(int interval);

void SettingsMenu::ApplyFpsAndVsync(GameContext& ctx) {
    // Якщо вибрано більше 60 FPS або UNLIMITED — V-Sync обов'язково вимикається,
    // інакше частота кадрів заблокується частотою розгортки монітора (60 Гц)
    if (ctx.targetFps == 0 || ctx.targetFps > 60) {
        ctx.vsync = false;
    }

    if (ctx.vsync) {
        SetWindowState(FLAG_VSYNC_HINT);
        glfwSwapInterval(1);
    } else {
        ClearWindowState(FLAG_VSYNC_HINT);
        glfwSwapInterval(0);
    }
    SetTargetFPS(ctx.targetFps > 0 ? ctx.targetFps : 0);
}

SettingsMenu::DisplayMode SettingsMenu::GetCurrentDisplayMode() {
    if (IsWindowFullscreen()) {
        return DisplayMode::Fullscreen;
    }
    if (IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE)) {
        return DisplayMode::Borderless;
    }
    return DisplayMode::Windowed;
}

void SettingsMenu::SetDisplayMode(DisplayMode target) {
    DisplayMode current = GetCurrentDisplayMode();
    if (current == target) return;

    // 1. Спочатку скидаємо будь-який активний режим до вікна
    if (current == DisplayMode::Fullscreen) {
        ToggleFullscreen();
    } else if (current == DisplayMode::Borderless) {
        ToggleBorderlessWindowed();
    }

    // 2. Вмикаємо новий цільовий режим
    if (target == DisplayMode::Fullscreen) {
        ToggleFullscreen();
    } else if (target == DisplayMode::Borderless) {
        ToggleBorderlessWindowed();
    } else if (target == DisplayMode::Windowed) {
        int monitor = GetCurrentMonitor();
        int monW = GetMonitorWidth(monitor);
        int monH = GetMonitorHeight(monitor);
        int winW = (monW >= 1600) ? 1600 : (monW >= 1280 ? 1280 : 960);
        int winH = (winW * 9) / 16;
        SetWindowSize(winW, winH);
        SetWindowPosition((monW - winW) / 2, (monH - winH) / 2);
    }
}

void SettingsMenu::CycleDisplayMode(GameContext& ctx, int dir) {
    int cur = static_cast<int>(GetCurrentDisplayMode());
    int next = (cur + (dir > 0 ? 1 : 2)) % 3;
    SetDisplayMode(static_cast<DisplayMode>(next));
    ctx.displayMode = next;
    SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
}

void SettingsMenu::Open(bool inGamePause) {
    isOpen_ = true;
    inGamePause_ = inGamePause;
    isRebinding_ = false;
    animTimer_ = 0.0f;
    statusMessage_ = "";
    statusMessageTimer_ = 0.0f;
    focusArea_ = FocusArea::List;
    selectedActionIdx_ = 0;
    selectedSlot_ = 0;
    audioSelectedRow_ = 0;
    bottomButtonIdx_ = 1;
    ShowCursor();
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

    Vector2 mousePos = GetVirtualMousePos();

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

    // 2. ГАРЯЧІ КЛАВІШІ ПЕРЕМИКАННЯ ВКЛАДОК: TAB, Q, E
    if (IsKeyPressed(KEY_TAB)) {
        currentTab_ = 1 - currentTab_;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
    }
    if (IsKeyPressed(KEY_Q)) {
        if (currentTab_ != 0) {
            currentTab_ = 0;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
        }
    }
    if (IsKeyPressed(KEY_E)) {
        if (currentTab_ != 1) {
            currentTab_ = 1;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
        }
    }

    // Закриття налаштувань по Escape
    if (IsKeyPressed(KEY_ESCAPE)) {
        Close(&ctx);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
        return;
    }

    // Геометрія панелі для перевірки кліків миші
    int boxW = 860;
    int boxH = 580;
    int boxX = (Config::VIRTUAL_WIDTH - boxW) / 2;
    int boxY = 40;

    int tabW = 240;
    int tabH = 34;
    int tab1X = boxX + boxW / 2 - tabW - 12;
    int tab2X = boxX + boxW / 2 + 12;
    int tabY = boxY + 60;

    Rectangle tab1Rec = { static_cast<float>(tab1X), static_cast<float>(tabY), static_cast<float>(tabW), static_cast<float>(tabH) };
    Rectangle tab2Rec = { static_cast<float>(tab2X), static_cast<float>(tabY), static_cast<float>(tabW), static_cast<float>(tabH) };

    // Клік миші по вкладках
    if (CheckCollisionPointRec(mousePos, tab1Rec) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        currentTab_ = 0;
        focusArea_ = FocusArea::List;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
    }
    if (CheckCollisionPointRec(mousePos, tab2Rec) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        currentTab_ = 1;
        focusArea_ = FocusArea::List;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
    }

    // Клік миші по кнопці "BACK / APPLY"
    Rectangle backRec = { static_cast<float>(boxX + boxW - 220), static_cast<float>(boxY + boxH - 58), 170.0f, 36.0f };
    if (CheckCollisionPointRec(mousePos, backRec) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Close(&ctx);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
        return;
    }

    // Клік миші по кнопці "QUIT TO TITLE" (у паузі гри)
    if (inGamePause_) {
        Rectangle quitRec = { static_cast<float>(boxX + 270), static_cast<float>(boxY + boxH - 58), 200.0f, 36.0f };
        if (CheckCollisionPointRec(mousePos, quitRec) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Close(&ctx);
            ctx.RequestStateChange(GameStateId::Title);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
            return;
        }
    }

    // Клік миші по кнопці "RESET DEFAULTS"
    if (currentTab_ == 0) {
        Rectangle resetRec = { static_cast<float>(boxX + 48), static_cast<float>(boxY + boxH - 58), 200.0f, 36.0f };
        if (CheckCollisionPointRec(mousePos, resetRec) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ctx.input.SetDefaults();
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
            statusMessage_ = "DEFAULTS RESTORED";
            statusMessageTimer_ = 2.0f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 1.0f, 0.6f);
        }
    }

    // 3. ІЄРАРХІЧНА КЛАВІАТУРНА НАВІГАЦІЯ СТРІЛКАМИ
    // ЗОНА 0: ВКЛАДКИ (TABS)
    if (focusArea_ == FocusArea::Tabs) {
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            currentTab_ = 0;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.85f, 0.3f);
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            currentTab_ = 1;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.85f, 0.3f);
        }
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            focusArea_ = FocusArea::List;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.9f, 0.4f);
        }
    }
    // ЗОНА 1: СПИСОК ПАРАМЕТРІВ (LIST)
    else if (focusArea_ == FocusArea::List) {
        if (currentTab_ == 0) {
            // Вкладка "Керування"
            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                if (selectedActionIdx_ == 0) {
                    focusArea_ = FocusArea::Tabs;
                } else {
                    selectedActionIdx_--;
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                if (selectedActionIdx_ >= static_cast<int>(InputAction::COUNT) - 1) {
                    focusArea_ = FocusArea::Bottom;
                    bottomButtonIdx_ = 1; // Фокус на Back / Apply
                } else {
                    selectedActionIdx_++;
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
            }
            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
                selectedSlot_ = 1 - selectedSlot_;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.85f, 0.3f);
            }
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
                isRebinding_ = true;
                rebindPulseTimer_ = 0.0f;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.0f, 0.6f);
            }
        } else {
            // Вкладка "Аудіо та Екран"
            static const int FPS_PRESETS[] = { 60, 120, 144, 240, 0 };
            auto getFpsIdx = [&](int fps) {
                for (int f = 0; f < 5; ++f) {
                    if (fps == FPS_PRESETS[f]) return f;
                }
                return 0;
            };

            if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
                if (audioSelectedRow_ == 0) {
                    focusArea_ = FocusArea::Tabs;
                } else {
                    audioSelectedRow_--;
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
                if (audioSelectedRow_ >= 7) {
                    focusArea_ = FocusArea::Bottom;
                    bottomButtonIdx_ = 1; // Фокус на Back / Apply
                } else {
                    audioSelectedRow_++;
                }
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
                    CycleDisplayMode(ctx, -1);
                } else if (audioSelectedRow_ == 5) {
                    ctx.vsync = !ctx.vsync;
                    if (ctx.vsync && (ctx.targetFps == 0 || ctx.targetFps > 60)) {
                        ctx.targetFps = 60;
                    }
                    ApplyFpsAndVsync(ctx);
                } else if (audioSelectedRow_ == 6) {
                    ctx.showFps = !ctx.showFps;
                } else if (audioSelectedRow_ == 7) {
                    int idx = getFpsIdx(ctx.targetFps);
                    idx = (idx + 4) % 5;
                    ctx.targetFps = FPS_PRESETS[idx];
                    ApplyFpsAndVsync(ctx);
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
                    CycleDisplayMode(ctx, 1);
                } else if (audioSelectedRow_ == 5) {
                    ctx.vsync = !ctx.vsync;
                    if (ctx.vsync && (ctx.targetFps == 0 || ctx.targetFps > 60)) {
                        ctx.targetFps = 60;
                    }
                    ApplyFpsAndVsync(ctx);
                } else if (audioSelectedRow_ == 6) {
                    ctx.showFps = !ctx.showFps;
                } else if (audioSelectedRow_ == 7) {
                    int idx = getFpsIdx(ctx.targetFps);
                    idx = (idx + 1) % 5;
                    ctx.targetFps = FPS_PRESETS[idx];
                    ApplyFpsAndVsync(ctx);
                }
                SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.0f, 0.4f);
            }
        }
    }
    // ЗОНА 2: НИЖНІ КНОПКИ (BOTTOM BUTTONS)
    else if (focusArea_ == FocusArea::Bottom) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
            focusArea_ = FocusArea::List;
            if (currentTab_ == 0) {
                selectedActionIdx_ = static_cast<int>(InputAction::COUNT) - 1;
            } else {
                audioSelectedRow_ = 7;
            }
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f, 0.3f);
        }
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
            bottomButtonIdx_ = std::max(0, bottomButtonIdx_ - 1);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.85f, 0.3f);
        }
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
            bottomButtonIdx_ = std::min(inGamePause_ ? 2 : 1, bottomButtonIdx_ + 1);
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.85f, 0.3f);
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
            if (bottomButtonIdx_ == 0) {
                // Reset defaults
                if (currentTab_ == 0) {
                    ctx.input.SetDefaults();
                    SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
                    statusMessage_ = "DEFAULTS RESTORED";
                    statusMessageTimer_ = 2.0f;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 1.0f, 0.6f);
                }
            } else if (bottomButtonIdx_ == 1) {
                // Back / Apply
                Close(&ctx);
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
                return;
            } else if (bottomButtonIdx_ == 2 && inGamePause_) {
                // Quit to Title
                Close(&ctx);
                ctx.RequestStateChange(GameStateId::Title);
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f, 0.6f);
                return;
            }
        }
    }
}

void SettingsMenu::Draw(GameContext& ctx) {
    if (!isOpen_) return;

    Vector2 mousePos = GetVirtualMousePos();

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
    Ui::Text(title, boxX + (boxW - tw) / 2 + 2, boxY + 14 + 2, 26, BLACK);
    Ui::Text(title, boxX + (boxW - tw) / 2, boxY + 14, 26, GOLD);

    // Підказка керування вкладками
    const char* tabHint = "[Q / TAB] Controls Tab      [E / TAB] Audio & Display Tab";
    int thw = Ui::Measure(tabHint, 13);
    Ui::Text(tabHint, boxX + (boxW - thw) / 2, boxY + 44, 13, SKYBLUE);

    // 3. Кнопки вкладок (Tabs)
    int tabW = 240;
    int tabH = 34;
    int tab1X = boxX + boxW / 2 - tabW - 12;
    int tab2X = boxX + boxW / 2 + 12;
    int tabY = boxY + 64;

    Rectangle tab1Rec = { static_cast<float>(tab1X), static_cast<float>(tabY), static_cast<float>(tabW), static_cast<float>(tabH) };
    Rectangle tab2Rec = { static_cast<float>(tab2X), static_cast<float>(tabY), static_cast<float>(tabW), static_cast<float>(tabH) };

    bool hoverTab1 = CheckCollisionPointRec(mousePos, tab1Rec);
    bool hoverTab2 = CheckCollisionPointRec(mousePos, tab2Rec);
    bool tabsFocused = (focusArea_ == FocusArea::Tabs);

    // Відмальовка Вкладки 1
    bool tab1Active = (currentTab_ == 0);
    Color tab1Bg = tab1Active ? Color{ 70, 52, 20, 240 } : (hoverTab1 ? Color{ 35, 42, 60, 230 } : Color{ 25, 30, 45, 200 });
    DrawRectangleRounded(tab1Rec, 0.25f, 4, tab1Bg);
    Color tab1Border = tab1Active ? GOLD : (hoverTab1 ? SKYBLUE : Color{ 80, 90, 115, 255 });
    if (tabsFocused && tab1Active) {
        tab1Border = (std::fmod(animTimer_ * 4.0f, 1.0f) < 0.5f) ? GOLD : WHITE;
    }
    DrawRectangleRoundedLines(tab1Rec, 0.25f, 4, (tab1Active || hoverTab1 || (tabsFocused && tab1Active)) ? 2.5f : 1.5f, tab1Border);
    int t1W = Ui::Measure("[Q] CONTROLS / KEYS", 14);
    Ui::Text("[Q] CONTROLS / KEYS", tab1X + (tabW - t1W) / 2, tabY + 9, 14, tab1Active ? GOLD : RAYWHITE);

    // Відмальовка Вкладки 2
    bool tab2Active = (currentTab_ == 1);
    Color tab2Bg = tab2Active ? Color{ 70, 52, 20, 240 } : (hoverTab2 ? Color{ 35, 42, 60, 230 } : Color{ 25, 30, 45, 200 });
    DrawRectangleRounded(tab2Rec, 0.25f, 4, tab2Bg);
    Color tab2Border = tab2Active ? GOLD : (hoverTab2 ? SKYBLUE : Color{ 80, 90, 115, 255 });
    if (tabsFocused && tab2Active) {
        tab2Border = (std::fmod(animTimer_ * 4.0f, 1.0f) < 0.5f) ? GOLD : WHITE;
    }
    DrawRectangleRoundedLines(tab2Rec, 0.25f, 4, (tab2Active || hoverTab2 || (tabsFocused && tab2Active)) ? 2.5f : 1.5f, tab2Border);
    int t2W = Ui::Measure("AUDIO & DISPLAY [E]", 14);
    Ui::Text("AUDIO & DISPLAY [E]", tab2X + (tabW - t2W) / 2, tabY + 9, 14, tab2Active ? GOLD : RAYWHITE);

    // 4. КОНТЕНТ ВКЛАДКИ КЕРУВАННЯ (TAB 0)
    if (currentTab_ == 0) {
        int listStartY = boxY + 114;
        int rowH = 32;

        // Заголовки стовпців
        Ui::Text("ACTION NAME", boxX + 48, listStartY - 18, 13, SKYBLUE);
        Ui::Text("PRIMARY KEY", boxX + 440, listStartY - 18, 13, SKYBLUE);
        Ui::Text("SECONDARY KEY", boxX + 640, listStartY - 18, 13, SKYBLUE);

        DrawLine(boxX + 40, listStartY - 2, boxX + boxW - 40, listStartY - 2, Color{ 70, 80, 110, 255 });

        for (int i = 0; i < static_cast<int>(InputAction::COUNT); ++i) {
            InputAction act = static_cast<InputAction>(i);
            const auto& binding = ctx.input.bindings[static_cast<size_t>(i)];
            int ry = listStartY + i * rowH;

            bool isRowSelected = (focusArea_ == FocusArea::List && i == selectedActionIdx_);

            // Фон рядка
            Rectangle rowRec = { static_cast<float>(boxX + 36), static_cast<float>(ry - 2), static_cast<float>(boxW - 72), static_cast<float>(rowH - 4) };
            if (isRowSelected) {
                DrawRectangleRounded(rowRec, 0.2f, 4, Color{ 40, 45, 65, 220 });
            }

            // Назва дії
            Ui::Text(InputConfig::GetActionName(act), boxX + 48, ry + 4, 13, isRowSelected ? GOLD : RAYWHITE);

            // Кнопка Primary Key
            Rectangle primRec = { static_cast<float>(boxX + 430), static_cast<float>(ry), 150.0f, 24.0f };
            bool hoverPrim = CheckCollisionPointRec(mousePos, primRec);
            bool isRebindingPrim = (isRebinding_ && isRowSelected && selectedSlot_ == 0);

            if (hoverPrim && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !isRebinding_) {
                selectedActionIdx_ = i;
                selectedSlot_ = 0;
                focusArea_ = FocusArea::List;
                isRebinding_ = true;
                rebindPulseTimer_ = 0.0f;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.0f, 0.6f);
            }

            Color primBg = isRebindingPrim
                ? Color{ 140, 95, 20, 255 }
                : ((isRowSelected && selectedSlot_ == 0) ? Color{ 60, 50, 25, 255 } : (hoverPrim ? Color{ 35, 42, 60, 255 } : Color{ 22, 26, 40, 255 }));
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
                focusArea_ = FocusArea::List;
                isRebinding_ = true;
                rebindPulseTimer_ = 0.0f;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.0f, 0.6f);
            }

            Color secBg = isRebindingSec
                ? Color{ 140, 95, 20, 255 }
                : ((isRowSelected && selectedSlot_ == 1) ? Color{ 60, 50, 25, 255 } : (hoverSec ? Color{ 35, 42, 60, 255 } : Color{ 22, 26, 40, 255 }));
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
        bool focusedReset = (focusArea_ == FocusArea::Bottom && bottomButtonIdx_ == 0);

        DrawRectangleRounded(resetRec, 0.25f, 4, (hoverReset || focusedReset) ? Color{ 85, 40, 30, 240 } : Color{ 35, 30, 45, 220 });
        DrawRectangleRoundedLines(resetRec, 0.25f, 4, 2.0f, (hoverReset || focusedReset) ? RED : Color{ 120, 70, 70, 255 });
        int rw = Ui::Measure("RESET DEFAULTS", 14);
        Ui::Text("RESET DEFAULTS", resetRec.x + (resetRec.width - rw) * 0.5f, resetRec.y + 9, 14, (hoverReset || focusedReset) ? RED : Color{ 220, 160, 160, 255 });
    }
    // 5. КОНТЕНТ ВКЛАДКИ АУДІО ТА ЕКРАНУ (TAB 1)
    else {
        int listStartY = boxY + 110;
        int rowH = 43;

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

        DisplayMode curMode = GetCurrentDisplayMode();
        std::string modeStr = (curMode == DisplayMode::Fullscreen) ? "EXCLUSIVE FULLSCREEN [F11]"
                            : (curMode == DisplayMode::Borderless ? "BORDERLESS WINDOWED" : "WINDOWED (1600x900)");

        AudioRow rows[8] = {
            { "MUSIC SEQUENCER", ctx.musicMuted ? "MUTED [P]" : "ENABLED" },
            { "MUSIC VOLUME", musVolStr },
            { "SFX (SOUND EFFECTS)", ctx.sfxMuted ? "MUTED" : "ENABLED" },
            { "SFX VOLUME", sfxVolStr },
            { "DISPLAY MODE", modeStr },
            { "VERTICAL SYNC (V-SYNC)", ctx.vsync ? "ENABLED" : "DISABLED" },
            { "SHOW FPS COUNTER", ctx.showFps ? "ENABLED [F3]" : "DISABLED [F3]" },
            { "FPS LIMIT", fpsLimitStr }
        };

        for (int i = 0; i < 8; ++i) {
            int ry = listStartY + i * rowH;
            bool sel = (focusArea_ == FocusArea::List && i == audioSelectedRow_);

            Rectangle rowRec = { static_cast<float>(boxX + 50), static_cast<float>(ry), static_cast<float>(boxW - 100), 36.0f };
            bool hoverRow = CheckCollisionPointRec(mousePos, rowRec);

            if (hoverRow && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                audioSelectedRow_ = i;
                focusArea_ = FocusArea::List;
                if (i == 0) {
                    ctx.musicMuted = !ctx.musicMuted;
                } else if (i == 2) {
                    ctx.sfxMuted = !ctx.sfxMuted;
                    if (ctx.audio) ctx.audio->SetSfxMuted(ctx.sfxMuted);
                } else if (i == 4) {
                    CycleDisplayMode(ctx, 1);
                } else if (i == 5) {
                    ctx.vsync = !ctx.vsync;
                    if (ctx.vsync && (ctx.targetFps == 0 || ctx.targetFps > 60)) {
                        ctx.targetFps = 60;
                    }
                    ApplyFpsAndVsync(ctx);
                } else if (i == 6) {
                    ctx.showFps = !ctx.showFps;
                } else if (i == 7) {
                    int idx = getFpsIdx(ctx.targetFps);
                    idx = (idx + 1) % 5;
                    ctx.targetFps = FPS_PRESETS[idx];
                    ApplyFpsAndVsync(ctx);
                }
                SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.0f, 0.4f);
            }

            DrawRectangleRounded(rowRec, 0.2f, 4, sel ? Color{ 45, 52, 75, 230 } : (hoverRow ? Color{ 30, 36, 52, 210 } : Color{ 22, 26, 38, 200 }));
            DrawRectangleRoundedLines(rowRec, 0.2f, 4, 1.8f, sel ? GOLD : (hoverRow ? SKYBLUE : Color{ 60, 70, 95, 255 }));

            Ui::Text(rows[i].title.c_str(), rowRec.x + 20, rowRec.y + 9, 15, sel ? GOLD : RAYWHITE);

            // Кнопки регулювання гучності [-] / [+]
            if (i == 1 || i == 3) {
                float volVal = (i == 1) ? ctx.musicVolume : ctx.sfxVolume;

                // Смужка гучності
                Rectangle barRec = { rowRec.x + rowRec.width - 260.0f, rowRec.y + 8.0f, 150.0f, 20.0f };
                DrawRectangleRounded(barRec, 0.2f, 4, Color{ 15, 18, 28, 255 });
                DrawRectangleRoundedLines(barRec, 0.2f, 4, 1.0f, Color{ 80, 90, 115, 255 });

                Rectangle fillRec = { barRec.x + 2.0f, barRec.y + 2.0f, (barRec.width - 4.0f) * volVal, barRec.height - 4.0f };
                DrawRectangleRounded(fillRec, 0.2f, 4, (i == 1) ? SKYBLUE : GOLD);

                std::string vStr = rows[i].value;
                int vw = Ui::Measure(vStr.c_str(), 14);
                Ui::Text(vStr.c_str(), rowRec.x + rowRec.width - 90 + (80 - vw) / 2, rowRec.y + 10, 14, RAYWHITE);
            } else {
                int vw = Ui::Measure(rows[i].value.c_str(), 15);
                Color valCol = (i == 4) ? ((curMode == DisplayMode::Fullscreen) ? LIME : SKYBLUE)
                             : (i == 5) ? (ctx.vsync ? LIME : SKYBLUE)
                             : (i == 6) ? (ctx.showFps ? LIME : Color{ 140, 150, 175, 255 })
                             : (i == 7) ? (ctx.targetFps == 0 ? GOLD : SKYBLUE)
                             : (sel ? SKYBLUE : GOLD);
                Ui::Text(rows[i].value.c_str(), rowRec.x + rowRec.width - vw - 24, rowRec.y + 9, 15, valCol);
            }
        }
    }

    // 6. Кнопка "BACK / APPLY" у правому нижньому кутку
    Rectangle backRec = { static_cast<float>(boxX + boxW - 220), static_cast<float>(boxY + boxH - 58), 170.0f, 36.0f };
    bool hoverBack = CheckCollisionPointRec(mousePos, backRec);
    bool focusedBack = (focusArea_ == FocusArea::Bottom && bottomButtonIdx_ == 1);

    DrawRectangleRounded(backRec, 0.25f, 4, (hoverBack || focusedBack) ? Color{ 75, 58, 24, 250 } : Color{ 35, 40, 60, 220 });
    DrawRectangleRoundedLines(backRec, 0.25f, 4, 2.0f, (hoverBack || focusedBack) ? GOLD : SKYBLUE);
    int bw = Ui::Measure("BACK / APPLY", 14);
    Ui::Text("BACK / APPLY", backRec.x + (backRec.width - bw) * 0.5f, backRec.y + 9, 14, (hoverBack || focusedBack) ? GOLD : RAYWHITE);

    // Додаткові кнопки для паузи в грі: Quit to Title
    if (inGamePause_) {
        Rectangle quitRec = { static_cast<float>(boxX + 270), static_cast<float>(boxY + boxH - 58), 200.0f, 36.0f };
        bool hoverQuit = CheckCollisionPointRec(mousePos, quitRec);
        bool focusedQuit = (focusArea_ == FocusArea::Bottom && bottomButtonIdx_ == 2);

        DrawRectangleRounded(quitRec, 0.25f, 4, (hoverQuit || focusedQuit) ? Color{ 85, 30, 30, 240 } : Color{ 40, 28, 35, 220 });
        DrawRectangleRoundedLines(quitRec, 0.25f, 4, 2.0f, (hoverQuit || focusedQuit) ? RED : Color{ 140, 70, 70, 255 });
        int qw = Ui::Measure("QUIT TO TITLE", 14);
        Ui::Text("QUIT TO TITLE", quitRec.x + (quitRec.width - qw) * 0.5f, quitRec.y + 9, 14, (hoverQuit || focusedQuit) ? RED : Color{ 220, 170, 170, 255 });
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

    // 8. Малюємо стилізований золотий курсор миші
    DrawTriangle(mousePos, { mousePos.x, mousePos.y + 15.0f }, { mousePos.x + 11.0f, mousePos.y + 11.0f }, GOLD);
    DrawTriangleLines(mousePos, { mousePos.x, mousePos.y + 15.0f }, { mousePos.x + 11.0f, mousePos.y + 11.0f }, BLACK);
}
