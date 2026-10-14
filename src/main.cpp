// === FILE: src/main.cpp ===
#include "core/Config.hpp"
#include "core/Types.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ShaderManager.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/UiText.hpp"
#include "states/IGameState.hpp"
#include "states/TitleState.hpp"
#include "states/LineageState.hpp"
#include "states/ManorState.hpp"
#include "states/OutskirtsState.hpp"
#include "states/CastlePlayState.hpp"
#include "states/DeathSummaryState.hpp"
#include "states/SettingsMenu.hpp"

#include <raylib.h>
#include <memory>
#include <ctime>
#include <cstdlib>
#include <algorithm>

namespace {

    std::unique_ptr<IGameState> CreateStateById(GameStateId id) {
        switch (id) {
            case GameStateId::Title:        return std::make_unique<TitleState>();
            case GameStateId::Lineage:      return std::make_unique<LineageState>();
            case GameStateId::Manor:        return std::make_unique<ManorState>();
            case GameStateId::Outskirts:    return std::make_unique<OutskirtsState>();
            case GameStateId::CastlePlay:   return std::make_unique<CastlePlayState>();
            case GameStateId::DeathSummary: return std::make_unique<DeathSummaryState>();
            default:                        return std::make_unique<TitleState>();
        }
    }

} // anonymous namespace

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(Config::RENDER_WIDTH, Config::RENDER_HEIGHT, "Rogue Legacy C++20 - Procedural Gothic Action-Roguelite");
    SetTargetFPS(Config::TARGET_FPS);
    SetExitKey(KEY_NULL); // Вимикаємо закриття вікна по ESC, щоб ESC працював для закриття меню Коваля/Чарівниці/Телепорту

    // Полотно високої чіткості 1920x1080
    RenderTexture2D canvas = LoadRenderTexture(Config::RENDER_WIDTH, Config::RENDER_HEIGHT);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    ToggleBorderlessWindowed(); // Старт у повноекранному режимі (F11 — перемкнути)

    SoundSynth soundSynth;
    ShaderManager shaderManager;
    ParticleSystem particleSystem;
    ProceduralRenderer proceduralRenderer;

    soundSynth.Init();
    shaderManager.Init();
    Ui::Init();
    shaderManager.SetPresentTarget(&canvas);

    GameContext ctx;
    ctx.audio     = &soundSynth;
    ctx.shaders   = &shaderManager;
    ctx.particles = &particleSystem;
    ctx.renderer  = &proceduralRenderer;
    ctx.currentStateId = GameStateId::Title;

    if (SaveManager::SaveFileExists(Config::SAVE_FILE_PATH)) {
        SaveManager::LoadGame(ctx, Config::SAVE_FILE_PATH);
        soundSynth.SetSfxMuted(ctx.sfxMuted);
        soundSynth.SetMusicVolume(ctx.musicVolume);
        soundSynth.SetSfxVolume(ctx.sfxVolume);
    }
    SettingsMenu::ApplyFpsAndVsync(ctx);

    std::unique_ptr<IGameState> currentState = CreateStateById(GameStateId::Title);
    currentState->OnEnter(ctx);

    while (!WindowShouldClose() && !ctx.shouldQuit) {
        float dt = std::min(GetFrameTime(), 0.05f);

        // Глобальний перемикач музичного секвенсора по клавіші [P]
        if (IsKeyPressed(KEY_P)) {
            ctx.musicMuted = !ctx.musicMuted;
        }
        soundSynth.UpdateMusic(dt, ctx.musicMuted);

        // Обробка переходу між станами
        if (ctx.hasRequestedState) {
            ctx.hasRequestedState = false;
            if (currentState) {
                currentState->OnExit(ctx);
            }
            ctx.currentStateId = ctx.requestedState;
            currentState = CreateStateById(ctx.requestedState);
            currentState->OnEnter(ctx);
        }

        if (currentState) {
            currentState->Update(dt, ctx);
        }

        // Перемикач повноекранного режиму: F11
        if (IsKeyPressed(KEY_F11)) {
            ToggleBorderlessWindowed();
        }

        // Перемикач лічильника FPS: F3
        if (IsKeyPressed(KEY_F3)) {
            ctx.showFps = !ctx.showFps;
            SaveManager::SaveSettings(ctx, Config::SAVE_FILE_PATH);
        }

        // 1. Малюємо кадр у полотно 1920x1080
        BeginTextureMode(canvas);
        ClearBackground(BLACK);
        if (currentState) {
            if (ctx.currentStateId == GameStateId::CastlePlay) {
                // CastlePlayState самостійно проектує сцену через ShaderManager та HUD камеру
                currentState->Draw(ctx);
            } else {
                Camera2D renderCam{};
                renderCam.zoom = Config::SCALE_FACTOR;
                BeginMode2D(renderCam);
                currentState->Draw(ctx);
                EndMode2D();
            }
        }
        EndTextureMode();

        // 2. Виводимо полотно на екран (при 1920x1080 — 1:1 піксель-в-піксель)
        float sw = static_cast<float>(GetScreenWidth());
        float sh = static_cast<float>(GetScreenHeight());
        float scale = std::min(sw / Config::RENDER_WIDTH, sh / Config::RENDER_HEIGHT);
        float dw = Config::RENDER_WIDTH * scale;
        float dh = Config::RENDER_HEIGHT * scale;
        Rectangle srcRec = { 0.0f, 0.0f, static_cast<float>(Config::RENDER_WIDTH), -static_cast<float>(Config::RENDER_HEIGHT) };
        Rectangle dstRec = { (sw - dw) * 0.5f, (sh - dh) * 0.5f, dw, dh };

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexturePro(canvas.texture, srcRec, dstRec, { 0.0f, 0.0f }, 0.0f, WHITE);

        // Лічильник FPS (вмикається через F3 або в меню налаштувань)
        if (ctx.showFps) {
            int fps = GetFPS();
            float frameMs = GetFrameTime() * 1000.0f;
            const char* fpsStr = TextFormat("FPS: %d (%.1f ms)", fps, frameMs);
            int tw = MeasureText(fpsStr, 16);
            int px = static_cast<int>(sw) - tw - 24;
            int py = 12;
            Rectangle badge = { static_cast<float>(px - 8), static_cast<float>(py - 4), static_cast<float>(tw + 16), 26.0f };
            DrawRectangleRounded(badge, 0.35f, 4, Color{ 10, 14, 22, 225 });
            Color col = (fps >= 55) ? LIME : ((fps >= 30) ? GOLD : RED);
            DrawRectangleRoundedLines(badge, 0.35f, 4, 1.5f, col);
            DrawText(fpsStr, px, py, 16, col);
        }

        EndDrawing();
    }

    if (currentState) {
        currentState->OnExit(ctx);
    }

    shaderManager.SetPresentTarget(nullptr);
    shaderManager.Shutdown();
    soundSynth.Shutdown();
    Ui::Shutdown();
    UnloadRenderTexture(canvas);
    CloseWindow();

    return 0;
}