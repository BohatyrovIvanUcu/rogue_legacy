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

    // Hotkey and window resize processing
    void UpdateDebugHotkeys();
    void CheckWindowResize();

    // Pixel scale control (K = 1, 2, 3, 4)
    void SetPixelScale(int scale);
    int GetPixelScale() const { return pixelScale_; }

    // Debug flags
    bool IsLightEnabled() const { return debugLight_; }
    void SetLightEnabled(bool enabled) { debugLight_ = enabled; }
    bool IsBloomEnabled() const { return debugBloom_; }
    void SetBloomEnabled(bool enabled) { debugBloom_ = enabled; }
    bool IsSilhouetteEnabled() const { return debugSilhouette_; }
    void SetSilhouetteEnabled(bool enabled) { debugSilhouette_ = enabled; }
    bool IsOutlineEnabled() const { return debugOutline_; }
    void SetOutlineEnabled(bool enabled) { debugOutline_ = enabled; }
    bool IsGrayscaleEnabled() const { return debugGrayscale_; }
    void SetGrayscaleEnabled(bool enabled) { debugGrayscale_ = enabled; }

    // Presentation target (e.g. 1920x1080 canvas)
    void SetPresentTarget(RenderTexture2D* target) { presentTarget_ = target; }

    // Pass 1: Scene (Background, Tiles, Props) -> sceneRT_
    void BeginScenePass(const Camera2D& worldCam);
    void EndScenePass();

    // Pass 2: Entities (Enemies, Boss, Player, Projectiles) -> entityRT_ (transparent background)
    void BeginEntityPass(const Camera2D& worldCam);
    void EndEntityPass();

    // Hit flash pass (pure white silhouette for 80ms)
    void BeginWhiteFlash();
    void EndWhiteFlash();

    // Pass 3: Composite entities onto sceneRT_ (with optional outline or silhouette shader)
    void CompositeEntitiesToScene();

    // Pass 4: Light map generation -> lightRT_
    void BeginLightPass(const Camera2D& worldCam, BiomeType biome = BiomeType::CastleHamson);
    void EndLightPass();

    // Pass 5: Composite sceneRT_ * lightRT_ -> litRT_
    void CompositeSceneAndLight();

    // Pass 6: Emissive effects (drawn directly into litRT_ in BLEND_ADDITIVE)
    void BeginEmissivePass(const Camera2D& worldCam);
    void EndEmissivePass();

    // Pass 7: Bloom processing
    void ApplyBloom();

    // Pass 8: Final grading & presentation to presentTarget_ (with point filter & K-scale)
    void PresentFinalScene(
        const GameContext& ctx,
        Vector2 playerScreenPos,
        bool isDarknessBiome,
        bool headlampActive,
        float facingDir,
        bool timeStopActive,
        const std::vector<Vector2>& torchScreenPositions
    );

    // Legacy backwards-compatible helpers
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

    Camera2D GetInternalCamera(const Camera2D& worldCam) const;

private:
    void RecreateRenderTextures();

    bool initialized_ = false;
    RenderTexture2D* presentTarget_ = nullptr;

    int targetW_ = 1920;
    int targetH_ = 1080;
    int pixelScale_ = 2; // K = 2 default
    int internalW_ = 960;
    int internalH_ = 540;

    // Render targets for the pipeline
    RenderTexture2D sceneRT_{};
    RenderTexture2D entityRT_{};
    RenderTexture2D lightRT_{};
    RenderTexture2D litRT_{};
    RenderTexture2D bloomRT1_{};
    RenderTexture2D bloomRT2_{};
    RenderTexture2D pingTarget_{};
    RenderTexture2D pongTarget_{};

    // Shaders
    Shader shPassThrough_{};
    Shader shOutline_{};
    Shader shSilhouette_{};
    Shader shGrayscale_{};
    Shader shLightComposite_{};
    Shader shBloomExtract_{};
    Shader shBloomBlur_{};
    Shader shGrading_{};
    Shader whiteFlashShader_{};
    Shader shColorblind_{};
    Shader shNostalgic_{};
    Shader shMyopia_{};
    Shader shHyperopia_{};
    Shader shDarkness_{};

    // Uniform locations
    int locOutlineTexelSize_ = -1;
    int locOutlineEnabled_ = -1;
    int locLightTexture_ = -1;
    int locLightCompositeEnabled_ = -1;

    int locBloomThreshold_ = -1;
    int locBloomBlurDir_ = -1;

    int locGradingTime_ = -1;
    int locGradingNostalgic_ = -1;
    int locGradingGrayscale_ = -1;
    int locGradingSilhouette_ = -1;
    int locGradingTheOne_ = -1;

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

    // Debug states
    bool debugLight_ = true;
    bool debugBloom_ = true;
    bool debugSilhouette_ = false;
    bool debugOutline_ = true;
    bool debugGrayscale_ = false;

    Camera2D currentInternalCam_{};
};
