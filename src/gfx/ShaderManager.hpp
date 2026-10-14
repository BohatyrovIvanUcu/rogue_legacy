#pragma once
#include "core/Types.hpp"
#include <raylib.h>
#include <vector>

class GameContext;

class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager();

    void Init();
    void Shutdown();

    void BeginSceneCapture();
    void BeginSceneCapture(const Camera2D& cam);
    void EndSceneAndPresent(
        const GameContext& ctx,
        Vector2 playerScreenPos,
        bool isDarknessBiome,
        bool headlampActive,
        float facingDir,
        bool timeStopActive,
        const std::vector<Vector2>& torchScreenPositions
    );

    // Віртуальне полотно 1280x720, у яке малюється кадр (для масштабування на весь екран)
    void SetPresentTarget(RenderTexture2D* target) { presentTarget_ = target; }

private:
    bool initialized_ = false;
    RenderTexture2D* presentTarget_ = nullptr;
    RenderTexture2D pingTarget_{};
    RenderTexture2D pongTarget_{};

    Shader shColorblind_{};
    Shader shNostalgic_{};
    Shader shMyopia_{};
    Shader shHyperopia_{};
    Shader shDarkness_{};

    int locNostalgicTime_ = -1;

    int locMyopiaPlayerPos_ = -1;
    int locMyopiaResolution_ = -1;

    int locHyperopiaPlayerPos_ = -1;
    int locHyperopiaResolution_ = -1;

    int locDarkPlayerPos_ = -1;
    int locDarkResolution_ = -1;
    int locDarkHeadlamp_ = -1;
    int locDarkFacing_ = -1;
    int locDarkTimeStop_ = -1;
    int locDarkIsDarkBiome_ = -1;
    int locDarkTorchCount_ = -1;
    int locDarkTorches_ = -1;
};
