#include "gfx/ShaderManager.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include <algorithm>

namespace {

    const char* FS_COLORBLIND = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;

void main() {
    vec4 texColor = texture(texture0, fragTexCoord) * fragColor;
    float lum = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
    finalColor = vec4(vec3(lum), texColor.a);
}
)";

    const char* FS_NOSTALGIC = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float u_time;

void main() {
    vec2 uv = fragTexCoord;
    vec4 texColor = texture(texture0, uv) * fragColor;

    vec3 sepia;
    sepia.r = dot(texColor.rgb, vec3(0.393, 0.769, 0.189));
    sepia.g = dot(texColor.rgb, vec3(0.349, 0.686, 0.168));
    sepia.b = dot(texColor.rgb, vec3(0.272, 0.534, 0.131));

    float grain = fract(sin(dot(uv * (u_time + 1.37), vec2(12.9898, 78.233))) * 43758.5453);
    sepia += (grain - 0.5) * 0.085;

    float scratchSeed = floor(uv.x * 380.0 + sin(u_time * 7.0) * 15.0);
    float scratchHash = fract(sin(scratchSeed * 91.7 + floor(u_time * 11.0) * 17.3) * 43758.5453);
    if (scratchHash > 0.993) {
        sepia += vec3(0.22);
    }

    vec2 centered = uv - vec2(0.5);
    float vignette = 1.0 - dot(centered, centered) * 1.15;
    sepia *= clamp(vignette, 0.25, 1.0);

    finalColor = vec4(clamp(sepia, 0.0, 1.0), texColor.a);
}
)";

    const char* FS_MYOPIA = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 u_playerPos;
uniform vec2 u_resolution;

void main() {
    vec2 fragPos = vec2(fragTexCoord.x * u_resolution.x, (1.0 - fragTexCoord.y) * u_resolution.y);
    float dist = distance(fragPos, u_playerPos);

    if (dist <= 315.0) {
        finalColor = texture(texture0, fragTexCoord) * fragColor;
        return;
    }

    float factor = clamp((dist - 315.0) / 390.0, 0.0, 1.0);
    vec2 stepUv = (vec2(2.25 + factor * 7.5) / u_resolution);

    vec4 sum = vec4(0.0);
    sum += texture(texture0, fragTexCoord + vec2(-stepUv.x, -stepUv.y)) * 0.0625;
    sum += texture(texture0, fragTexCoord + vec2( 0.0,      -stepUv.y)) * 0.1250;
    sum += texture(texture0, fragTexCoord + vec2( stepUv.x, -stepUv.y)) * 0.0625;

    sum += texture(texture0, fragTexCoord + vec2(-stepUv.x,  0.0))      * 0.1250;
    sum += texture(texture0, fragTexCoord)                              * 0.2500;
    sum += texture(texture0, fragTexCoord + vec2( stepUv.x,  0.0))      * 0.1250;

    sum += texture(texture0, fragTexCoord + vec2(-stepUv.x,  stepUv.y)) * 0.0625;
    sum += texture(texture0, fragTexCoord + vec2( 0.0,       stepUv.y)) * 0.1250;
    sum += texture(texture0, fragTexCoord + vec2( stepUv.x,  stepUv.y)) * 0.0625;

    sum.rgb *= (1.0 - factor * 0.20);
    finalColor = sum * fragColor;
}
)";

    const char* FS_HYPEROPIA = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 u_playerPos;
uniform vec2 u_resolution;

void main() {
    vec2 fragPos = vec2(fragTexCoord.x * u_resolution.x, (1.0 - fragTexCoord.y) * u_resolution.y);
    float dist = distance(fragPos, u_playerPos);

    if (dist >= 202.5) {
        finalColor = texture(texture0, fragTexCoord) * fragColor;
        return;
    }

    float factor = clamp(1.0 - (dist / 202.5), 0.0, 1.0);
    vec2 stepUv = (vec2(3.0 + factor * 8.0) / u_resolution);

    vec4 sum = vec4(0.0);
    sum += texture(texture0, fragTexCoord + vec2(-stepUv.x, -stepUv.y)) * 0.0625;
    sum += texture(texture0, fragTexCoord + vec2( 0.0,      -stepUv.y)) * 0.1250;
    sum += texture(texture0, fragTexCoord + vec2( stepUv.x, -stepUv.y)) * 0.0625;

    sum += texture(texture0, fragTexCoord + vec2(-stepUv.x,  0.0))      * 0.1250;
    sum += texture(texture0, fragTexCoord)                              * 0.2500;
    sum += texture(texture0, fragTexCoord + vec2( stepUv.x,  0.0))      * 0.1250;

    sum += texture(texture0, fragTexCoord + vec2(-stepUv.x,  stepUv.y)) * 0.0625;
    sum += texture(texture0, fragTexCoord + vec2( 0.0,       stepUv.y)) * 0.1250;
    sum += texture(texture0, fragTexCoord + vec2( stepUv.x,  stepUv.y)) * 0.0625;

    finalColor = sum * fragColor;
}
)";

    const char* FS_DARKNESS = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;

uniform vec2 u_playerPos;
uniform vec2 u_resolution;
uniform int u_headlamp;
uniform float u_facing;
uniform int u_timeStop;
uniform int u_isDarkBiome;
uniform int u_torchCount;
uniform vec2 u_torches[8];

void main() {
    vec4 texColor = texture(texture0, fragTexCoord) * fragColor;
    vec2 fragPos = vec2(fragTexCoord.x * u_resolution.x, (1.0 - fragTexCoord.y) * u_resolution.y);

    if (u_isDarkBiome == 1) {
        float distPlayer = distance(fragPos, u_playerPos);
        float light = 1.0 - smoothstep(142.5, 337.5, distPlayer);

        if (u_headlamp == 1) {
            vec2 toFrag = fragPos - u_playerPos;
            float d = length(toFrag);
            if (d > 1.0 && d < 810.0) {
                vec2 dir = toFrag / d;
                vec2 lookDir = normalize(vec2(u_facing, 0.05));
                float coneDot = dot(dir, lookDir);
                if (coneDot > 0.78) {
                    float coneIntensity = smoothstep(0.78, 0.94, coneDot) * (1.0 - d / 810.0);
                    light = max(light, coneIntensity * 1.15);
                }
            }
            light = max(light, 1.0 - smoothstep(210.0, 435.0, distPlayer));
        }

        for (int i = 0; i < u_torchCount; ++i) {
            float td = distance(fragPos, u_torches[i]);
            float torchLight = (1.0 - smoothstep(67.5, 262.5, td)) * 0.85;
            light = max(light, torchLight);
        }

        float ambient = 0.14;
        texColor.rgb *= clamp(max(light, ambient), 0.0, 1.15);
    }

    if (u_timeStop == 1) {
        float lum = dot(texColor.rgb, vec3(0.299, 0.587, 0.114));
        vec3 chronoTint = vec3(lum * 0.75, lum * 0.92, min(1.0, lum * 1.35 + 0.12));
        texColor.rgb = mix(texColor.rgb, chronoTint, 0.75);
    }

    finalColor = texColor;
}
)";

    const char* FS_PASSTHROUGH = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;

void main() {
    finalColor = texture(texture0, fragTexCoord) * fragColor;
}
)";

    const char* FS_OUTLINE = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 u_texelSize;
uniform int u_enabled;

void main() {
    vec4 c = texture(texture0, fragTexCoord);
    if (u_enabled == 0) {
        finalColor = c * fragColor;
        return;
    }

    if (c.a < 0.1) {
        // Прозорий піксель: оцінюємо 8 сусідів на 1 внутрішній піксель
        float aN  = texture(texture0, fragTexCoord + vec2( 0.0,          -u_texelSize.y)).a;
        float aS  = texture(texture0, fragTexCoord + vec2( 0.0,           u_texelSize.y)).a;
        float aW  = texture(texture0, fragTexCoord + vec2(-u_texelSize.x, 0.0)).a;
        float aE  = texture(texture0, fragTexCoord + vec2( u_texelSize.x, 0.0)).a;
        float aNW = texture(texture0, fragTexCoord + vec2(-u_texelSize.x, -u_texelSize.y)).a;
        float aNE = texture(texture0, fragTexCoord + vec2( u_texelSize.x, -u_texelSize.y)).a;
        float aSW = texture(texture0, fragTexCoord + vec2(-u_texelSize.x,  u_texelSize.y)).a;
        float aSE = texture(texture0, fragTexCoord + vec2( u_texelSize.x,  u_texelSize.y)).a;

        float maxA = max(max(max(aN, aS), max(aW, aE)), max(max(aNW, aNE), max(aSW, aSE)));
        if (maxA >= 0.1) {
            // Темний готичний контур #0C0A12
            finalColor = vec4(0.04706, 0.03922, 0.07059, 1.0) * fragColor;
            return;
        }
        finalColor = vec4(0.0);
    } else {
        // Непрозорий піксель: перевіряємо, чи є прозорий сусід зверху-ліворуч (NW)
        float aNW = texture(texture0, fragTexCoord + vec2(-u_texelSize.x, -u_texelSize.y)).a;
        float aN  = texture(texture0, fragTexCoord + vec2( 0.0,          -u_texelSize.y)).a;
        float aW  = texture(texture0, fragTexCoord + vec2(-u_texelSize.x, 0.0)).a;

        if (aNW < 0.1 || aN < 0.1 || aW < 0.1) {
            // Rim light: +20% яскравості
            vec3 rim = min(c.rgb * 1.20 + vec3(0.04, 0.04, 0.06), vec3(1.0));
            finalColor = vec4(rim, c.a) * fragColor;
        } else {
            finalColor = c * fragColor;
        }
    }
}
)";

    const char* FS_SILHOUETTE = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;

void main() {
    vec4 c = texture(texture0, fragTexCoord);
    if (c.a > 0.08) {
        // Плоский чорний силует #0C0A12
        finalColor = vec4(0.04706, 0.03922, 0.07059, 1.0) * fragColor;
    } else {
        finalColor = vec4(0.0);
    }
}
)";

    const char* FS_WHITE_FLASH = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;

void main() {
    vec4 c = texture(texture0, fragTexCoord);
    float a = c.a * fragColor.a;
    if (a > 0.05) {
        finalColor = vec4(1.0, 1.0, 1.0, a);
    } else {
        finalColor = vec4(0.0);
    }
}
)";

    const char* FS_GRAYSCALE = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;

void main() {
    vec4 c = texture(texture0, fragTexCoord) * fragColor;
    float lum = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    finalColor = vec4(vec3(lum), c.a);
}
)";

    const char* FS_LIGHT_COMPOSITE = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D u_lightTexture;
uniform int u_lightEnabled;

void main() {
    vec4 sceneCol = texture(texture0, fragTexCoord) * fragColor;
    if (u_lightEnabled == 0) {
        finalColor = sceneCol;
        return;
    }
    vec4 lightCol = texture(u_lightTexture, fragTexCoord);
    vec3 effectiveLight = max(lightCol.rgb, vec3(0.20));
    vec3 lit = sceneCol.rgb * effectiveLight * 1.30;
    finalColor = vec4(clamp(lit, 0.0, 1.0), sceneCol.a);
}
)";

    const char* FS_BLOOM_EXTRACT = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float u_threshold;

void main() {
    vec4 c = texture(texture0, fragTexCoord) * fragColor;
    float lum = dot(c.rgb, vec3(0.299, 0.587, 0.114));
    float factor = clamp((lum - u_threshold) / max(0.001, 1.0 - u_threshold), 0.0, 1.0);
    factor = factor * factor;
    finalColor = vec4(c.rgb * factor, c.a);
}
)";

    const char* FS_BLOOM_BLUR = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 u_dir;

void main() {
    vec4 sum = texture(texture0, fragTexCoord) * 0.227027;
    sum += texture(texture0, fragTexCoord + u_dir * 1.384615) * 0.316216;
    sum += texture(texture0, fragTexCoord - u_dir * 1.384615) * 0.316216;
    sum += texture(texture0, fragTexCoord + u_dir * 3.230769) * 0.070270;
    sum += texture(texture0, fragTexCoord - u_dir * 3.230769) * 0.070270;
    finalColor = sum * fragColor;
}
)";

    const char* FS_FINAL_GRADING = 
R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;

uniform float u_time;
uniform int u_isNostalgic;
uniform int u_isGrayscale;
uniform int u_isSilhouette;

void main() {
    vec2 uv = fragTexCoord;
    vec4 c = texture(texture0, uv) * fragColor;

    if (u_isSilhouette == 1) {
        vec3 silCol = vec3(0.04706, 0.03922, 0.07059);
        if (distance(c.rgb, silCol) < 0.045) {
            finalColor = vec4(silCol, 1.0);
        } else {
            finalColor = vec4(0.51, 0.53, 0.60, 1.0);
        }
        return;
    }

    vec3 col = c.rgb;

    if (u_isNostalgic == 1) {
        vec3 sepia;
        sepia.r = dot(col, vec3(0.393, 0.769, 0.189));
        sepia.g = dot(col, vec3(0.349, 0.686, 0.168));
        sepia.b = dot(col, vec3(0.272, 0.534, 0.131));

        float grain = fract(sin(dot(uv * (u_time + 1.37), vec2(12.9898, 78.233))) * 43758.5453);
        sepia += (grain - 0.5) * 0.085;

        float scratchSeed = floor(uv.x * 380.0 + sin(u_time * 7.0) * 15.0);
        float scratchHash = fract(sin(scratchSeed * 91.7 + floor(u_time * 11.0) * 17.3) * 43758.5453);
        if (scratchHash > 0.993) {
            sepia += vec3(0.22);
        }

        vec2 centered = uv - vec2(0.5);
        float vig = 1.0 - dot(centered, centered) * 1.15;
        col = clamp(sepia * clamp(vig, 0.25, 1.0), 0.0, 1.0);
    } else {
        float grain = fract(sin(dot(uv * (u_time * 0.35 + 1.2), vec2(12.9898, 78.233))) * 43758.5453);
        col += (grain - 0.5) * 0.022;

        vec2 centered = uv - vec2(0.5);
        float vig = 1.0 - dot(centered, centered) * 0.62;
        col *= clamp(vig, 0.50, 1.0);
    }

    if (u_isGrayscale == 1) {
        float lum = dot(col, vec3(0.299, 0.587, 0.114));
        col = vec3(lum);
    }

    finalColor = vec4(clamp(col, 0.0, 1.0), c.a);
}
)";

} // anonymous namespace

ShaderManager::~ShaderManager() {
    Shutdown();
}

void ShaderManager::RecreateRenderTextures() {
    if (!initialized_) return;

    if (sceneRT_.id != 0)  UnloadRenderTexture(sceneRT_);
    if (entityRT_.id != 0) UnloadRenderTexture(entityRT_);
    if (lightRT_.id != 0)  UnloadRenderTexture(lightRT_);
    if (litRT_.id != 0)    UnloadRenderTexture(litRT_);
    if (bloomRT1_.id != 0) UnloadRenderTexture(bloomRT1_);
    if (bloomRT2_.id != 0) UnloadRenderTexture(bloomRT2_);
    if (pingTarget_.id != 0) UnloadRenderTexture(pingTarget_);
    if (pongTarget_.id != 0) UnloadRenderTexture(pongTarget_);

    targetW_ = presentTarget_ ? presentTarget_->texture.width : Config::RENDER_WIDTH;
    targetH_ = presentTarget_ ? presentTarget_->texture.height : Config::RENDER_HEIGHT;
    if (targetW_ <= 0) targetW_ = Config::RENDER_WIDTH;
    if (targetH_ <= 0) targetH_ = Config::RENDER_HEIGHT;

    internalW_ = (targetW_ + pixelScale_ - 1) / pixelScale_;
    internalH_ = (targetH_ + pixelScale_ - 1) / pixelScale_;

    sceneRT_  = LoadRenderTexture(internalW_, internalH_);
    entityRT_ = LoadRenderTexture(internalW_, internalH_);
    lightRT_  = LoadRenderTexture(std::max(1, internalW_ / 2), std::max(1, internalH_ / 2));
    litRT_    = LoadRenderTexture(internalW_, internalH_);
    bloomRT1_ = LoadRenderTexture(std::max(1, internalW_ / 4), std::max(1, internalH_ / 4));
    bloomRT2_ = LoadRenderTexture(std::max(1, internalW_ / 4), std::max(1, internalH_ / 4));

    pingTarget_ = LoadRenderTexture(targetW_, targetH_);
    pongTarget_ = LoadRenderTexture(targetW_, targetH_);

    SetTextureFilter(sceneRT_.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(entityRT_.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(lightRT_.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(litRT_.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(bloomRT1_.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(bloomRT2_.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(pingTarget_.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(pongTarget_.texture, TEXTURE_FILTER_POINT);
}

void ShaderManager::Init() {
    if (initialized_) return;

    targetW_ = Config::RENDER_WIDTH;
    targetH_ = Config::RENDER_HEIGHT;
    pixelScale_ = 2;
    internalW_ = targetW_ / pixelScale_;
    internalH_ = targetH_ / pixelScale_;

    shPassThrough_    = LoadShaderFromMemory(nullptr, FS_PASSTHROUGH);
    shOutline_        = LoadShaderFromMemory(nullptr, FS_OUTLINE);
    shSilhouette_     = LoadShaderFromMemory(nullptr, FS_SILHOUETTE);
    shGrayscale_      = LoadShaderFromMemory(nullptr, FS_GRAYSCALE);
    shLightComposite_ = LoadShaderFromMemory(nullptr, FS_LIGHT_COMPOSITE);
    shBloomExtract_   = LoadShaderFromMemory(nullptr, FS_BLOOM_EXTRACT);
    shBloomBlur_      = LoadShaderFromMemory(nullptr, FS_BLOOM_BLUR);
    shGrading_        = LoadShaderFromMemory(nullptr, FS_FINAL_GRADING);
    whiteFlashShader_ = LoadShaderFromMemory(nullptr, FS_WHITE_FLASH);

    shColorblind_ = LoadShaderFromMemory(nullptr, FS_COLORBLIND);
    shNostalgic_  = LoadShaderFromMemory(nullptr, FS_NOSTALGIC);
    shMyopia_     = LoadShaderFromMemory(nullptr, FS_MYOPIA);
    shHyperopia_  = LoadShaderFromMemory(nullptr, FS_HYPEROPIA);
    shDarkness_   = LoadShaderFromMemory(nullptr, FS_DARKNESS);

    locOutlineTexelSize_      = GetShaderLocation(shOutline_, "u_texelSize");
    locOutlineEnabled_        = GetShaderLocation(shOutline_, "u_enabled");
    locLightTexture_          = GetShaderLocation(shLightComposite_, "u_lightTexture");
    locLightCompositeEnabled_ = GetShaderLocation(shLightComposite_, "u_lightEnabled");

    locBloomThreshold_ = GetShaderLocation(shBloomExtract_, "u_threshold");
    locBloomBlurDir_   = GetShaderLocation(shBloomBlur_, "u_dir");

    locGradingTime_       = GetShaderLocation(shGrading_, "u_time");
    locGradingNostalgic_  = GetShaderLocation(shGrading_, "u_isNostalgic");
    locGradingGrayscale_  = GetShaderLocation(shGrading_, "u_isGrayscale");
    locGradingSilhouette_ = GetShaderLocation(shGrading_, "u_isSilhouette");

    locNostalgicTime_       = GetShaderLocation(shNostalgic_, "u_time");

    locMyopiaPlayerPos_     = GetShaderLocation(shMyopia_, "u_playerPos");
    locMyopiaResolution_    = GetShaderLocation(shMyopia_, "u_resolution");

    locHyperopiaPlayerPos_  = GetShaderLocation(shHyperopia_, "u_playerPos");
    locHyperopiaResolution_ = GetShaderLocation(shHyperopia_, "u_resolution");

    locDarkPlayerPos_       = GetShaderLocation(shDarkness_, "u_playerPos");
    locDarkResolution_      = GetShaderLocation(shDarkness_, "u_resolution");
    locDarkHeadlamp_        = GetShaderLocation(shDarkness_, "u_headlamp");
    locDarkFacing_          = GetShaderLocation(shDarkness_, "u_facing");
    locDarkTimeStop_        = GetShaderLocation(shDarkness_, "u_timeStop");
    locDarkIsDarkBiome_     = GetShaderLocation(shDarkness_, "u_isDarkBiome");
    locDarkTorchCount_      = GetShaderLocation(shDarkness_, "u_torchCount");
    locDarkTorches_         = GetShaderLocation(shDarkness_, "u_torches");

    initialized_ = true;
    RecreateRenderTextures();
}

void ShaderManager::Shutdown() {
    if (!initialized_) return;

    if (sceneRT_.id != 0)  UnloadRenderTexture(sceneRT_);
    if (entityRT_.id != 0) UnloadRenderTexture(entityRT_);
    if (lightRT_.id != 0)  UnloadRenderTexture(lightRT_);
    if (litRT_.id != 0)    UnloadRenderTexture(litRT_);
    if (bloomRT1_.id != 0) UnloadRenderTexture(bloomRT1_);
    if (bloomRT2_.id != 0) UnloadRenderTexture(bloomRT2_);
    if (pingTarget_.id != 0) UnloadRenderTexture(pingTarget_);
    if (pongTarget_.id != 0) UnloadRenderTexture(pongTarget_);

    UnloadShader(shPassThrough_);
    UnloadShader(shOutline_);
    UnloadShader(shSilhouette_);
    UnloadShader(shGrayscale_);
    UnloadShader(shLightComposite_);
    UnloadShader(shBloomExtract_);
    UnloadShader(shBloomBlur_);
    UnloadShader(shGrading_);
    UnloadShader(whiteFlashShader_);
    UnloadShader(shColorblind_);
    UnloadShader(shNostalgic_);
    UnloadShader(shMyopia_);
    UnloadShader(shHyperopia_);
    UnloadShader(shDarkness_);

    initialized_ = false;
}

void ShaderManager::UpdateDebugHotkeys() {
    if (IsKeyPressed(KEY_F5)) {
        debugLight_ = !debugLight_;
        TraceLog(LOG_INFO, "DEBUG: Light %s", debugLight_ ? "ON" : "OFF");
    }
    if (IsKeyPressed(KEY_F6)) {
        int nextScale = (pixelScale_ % 4) + 1;
        SetPixelScale(nextScale);
        TraceLog(LOG_INFO, "DEBUG: Pixel Scale set to %dx", pixelScale_);
    }
    if (IsKeyPressed(KEY_F7)) {
        debugBloom_ = !debugBloom_;
        TraceLog(LOG_INFO, "DEBUG: Bloom %s", debugBloom_ ? "ON" : "OFF");
    }
    if (IsKeyPressed(KEY_F8)) {
        debugSilhouette_ = !debugSilhouette_;
        TraceLog(LOG_INFO, "DEBUG: Silhouette Test %s", debugSilhouette_ ? "ON" : "OFF");
    }
    if (IsKeyPressed(KEY_F9)) {
        debugOutline_ = !debugOutline_;
        TraceLog(LOG_INFO, "DEBUG: Outline %s", debugOutline_ ? "ON" : "OFF");
    }
    if (IsKeyPressed(KEY_F10)) {
        debugGrayscale_ = !debugGrayscale_;
        TraceLog(LOG_INFO, "DEBUG: Grayscale Mode %s", debugGrayscale_ ? "ON" : "OFF");
    }
}

void ShaderManager::CheckWindowResize() {
    int curW = presentTarget_ ? presentTarget_->texture.width : GetScreenWidth();
    int curH = presentTarget_ ? presentTarget_->texture.height : GetScreenHeight();
    if (curW <= 0) curW = Config::RENDER_WIDTH;
    if (curH <= 0) curH = Config::RENDER_HEIGHT;

    if (curW != targetW_ || curH != targetH_ || IsWindowResized()) {
        targetW_ = curW;
        targetH_ = curH;
        RecreateRenderTextures();
    }
}

void ShaderManager::SetPixelScale(int scale) {
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    if (scale != pixelScale_) {
        pixelScale_ = scale;
        RecreateRenderTextures();
    }
}

Camera2D ShaderManager::GetInternalCamera(const Camera2D& worldCam) const {
    Camera2D internalCam = worldCam;
    float k = static_cast<float>(pixelScale_);
    float internalZoom = worldCam.zoom / k;
    internalCam.zoom = internalZoom;
    internalCam.offset = {
        std::floor(worldCam.offset.x / k),
        std::floor(worldCam.offset.y / k)
    };
    // Округлення target до цілого внутрішнього пікселя
    internalCam.target = {
        std::round(worldCam.target.x * internalZoom) / internalZoom,
        std::round(worldCam.target.y * internalZoom) / internalZoom
    };
    return internalCam;
}

void ShaderManager::BeginScenePass(const Camera2D& worldCam) {
    CheckWindowResize();
    currentInternalCam_ = GetInternalCamera(worldCam);
    BeginTextureMode(sceneRT_);
    ClearBackground(BLACK);
    BeginMode2D(currentInternalCam_);
}

void ShaderManager::EndScenePass() {
    EndMode2D();
    EndTextureMode();
}

void ShaderManager::BeginEntityPass(const Camera2D& worldCam) {
    currentInternalCam_ = GetInternalCamera(worldCam);
    BeginTextureMode(entityRT_);
    ClearBackground(BLANK); // Прозорий фон
    BeginMode2D(currentInternalCam_);
}

void ShaderManager::EndEntityPass() {
    EndMode2D();
    EndTextureMode();
}

void ShaderManager::BeginWhiteFlash() {
    if (whiteFlashShader_.id != 0) {
        BeginShaderMode(whiteFlashShader_);
    }
}

void ShaderManager::EndWhiteFlash() {
    if (whiteFlashShader_.id != 0) {
        EndShaderMode();
    }
}

void ShaderManager::CompositeEntitiesToScene() {
    BeginTextureMode(sceneRT_);
    if (debugSilhouette_) {
        BeginShaderMode(shSilhouette_);
    } else if (debugOutline_) {
        BeginShaderMode(shOutline_);
        float texelSize[2] = {
            1.0f / static_cast<float>(entityRT_.texture.width),
            1.0f / static_cast<float>(entityRT_.texture.height)
        };
        int enabled = 1;
        SetShaderValue(shOutline_, locOutlineTexelSize_, texelSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shOutline_, locOutlineEnabled_, &enabled, SHADER_UNIFORM_INT);
    }

    Rectangle srcRec = {
        0.0f, 0.0f,
        static_cast<float>(entityRT_.texture.width),
        -static_cast<float>(entityRT_.texture.height)
    };
    DrawTextureRec(entityRT_.texture, srcRec, { 0.0f, 0.0f }, WHITE);

    if (debugSilhouette_ || debugOutline_) {
        EndShaderMode();
    }
    EndTextureMode();
}

void ShaderManager::BeginLightPass(const Camera2D& worldCam, BiomeType biome) {
    currentInternalCam_ = GetInternalCamera(worldCam);
    BeginTextureMode(lightRT_);
    if (!debugLight_) {
        ClearBackground(WHITE);
    } else {
        Color ambient;
        switch (biome) {
            case BiomeType::CastleHamson:    ambient = Color{ 85, 100, 145, 255 }; break; // Темно-синій замок
            case BiomeType::ForestAbkhazia:  ambient = Color{ 75, 110, 85, 255 };  break; // Брудно-зелений сад
            case BiomeType::TheMaya:         ambient = Color{ 105, 80, 130, 255 }; break; // Пурпурова вежа
            case BiomeType::LandOfDarkness:  ambient = Color{ 48, 52, 85, 255 };   break; // Глибокий індиго
            default:                         ambient = Color{ 90, 95, 120, 255 };  break;
        }
        ClearBackground(ambient);
    }
    Camera2D lightCam = currentInternalCam_;
    lightCam.offset = { lightCam.offset.x * 0.5f, lightCam.offset.y * 0.5f };
    lightCam.zoom = lightCam.zoom * 0.5f;
    BeginMode2D(lightCam);
    if (debugLight_) {
        BeginBlendMode(BLEND_ADDITIVE);
    }
}

void ShaderManager::EndLightPass() {
    if (debugLight_) {
        EndBlendMode();
    }
    EndMode2D();
    EndTextureMode();
}

void ShaderManager::CompositeSceneAndLight() {
    BeginTextureMode(litRT_);
    ClearBackground(BLACK);
    if (debugLight_) {
        BeginShaderMode(shLightComposite_);
        int lightFlag = 1;
        SetShaderValue(shLightComposite_, locLightCompositeEnabled_, &lightFlag, SHADER_UNIFORM_INT);
        SetShaderValueTexture(shLightComposite_, locLightTexture_, lightRT_.texture);
    }

    Rectangle srcRec = {
        0.0f, 0.0f,
        static_cast<float>(sceneRT_.texture.width),
        -static_cast<float>(sceneRT_.texture.height)
    };
    DrawTextureRec(sceneRT_.texture, srcRec, { 0.0f, 0.0f }, WHITE);

    if (debugLight_) {
        EndShaderMode();
    }
    EndTextureMode();
}

void ShaderManager::BeginEmissivePass(const Camera2D& worldCam) {
    currentInternalCam_ = GetInternalCamera(worldCam);
    BeginTextureMode(litRT_);
    BeginBlendMode(BLEND_ADDITIVE);
    BeginMode2D(currentInternalCam_);
}

void ShaderManager::EndEmissivePass() {
    EndMode2D();
    EndBlendMode();
    EndTextureMode();
}

void ShaderManager::ApplyBloom() {
    if (!debugBloom_) return;

    // 1. Витягуємо яскраві пікселі з litRT_ у bloomRT1_ (1/4 роздільності)
    BeginTextureMode(bloomRT1_);
    ClearBackground(BLANK);
    BeginShaderMode(shBloomExtract_);
    float threshold = 0.60f;
    SetShaderValue(shBloomExtract_, locBloomThreshold_, &threshold, SHADER_UNIFORM_FLOAT);
    Rectangle srcRec = {
        0.0f, 0.0f,
        static_cast<float>(litRT_.texture.width),
        -static_cast<float>(litRT_.texture.height)
    };
    Rectangle dstRec = {
        0.0f, 0.0f,
        static_cast<float>(bloomRT1_.texture.width),
        static_cast<float>(bloomRT1_.texture.height)
    };
    DrawTexturePro(litRT_.texture, srcRec, dstRec, { 0.0f, 0.0f }, 0.0f, WHITE);
    EndShaderMode();
    EndTextureMode();

    // 2. Горизонтальне гаусівське розмиття: bloomRT1_ -> bloomRT2_
    BeginTextureMode(bloomRT2_);
    ClearBackground(BLANK);
    BeginShaderMode(shBloomBlur_);
    float dirH[2] = { 1.85f / static_cast<float>(bloomRT1_.texture.width), 0.0f };
    SetShaderValue(shBloomBlur_, locBloomBlurDir_, dirH, SHADER_UNIFORM_VEC2);
    Rectangle bRec = {
        0.0f, 0.0f,
        static_cast<float>(bloomRT1_.texture.width),
        -static_cast<float>(bloomRT1_.texture.height)
    };
    DrawTextureRec(bloomRT1_.texture, bRec, { 0.0f, 0.0f }, WHITE);
    EndShaderMode();
    EndTextureMode();

    // 3. Вертикальне гаусівське розмиття: bloomRT2_ -> bloomRT1_
    BeginTextureMode(bloomRT1_);
    ClearBackground(BLANK);
    BeginShaderMode(shBloomBlur_);
    float dirV[2] = { 0.0f, 1.85f / static_cast<float>(bloomRT2_.texture.height) };
    SetShaderValue(shBloomBlur_, locBloomBlurDir_, dirV, SHADER_UNIFORM_VEC2);
    Rectangle bRec2 = {
        0.0f, 0.0f,
        static_cast<float>(bloomRT2_.texture.width),
        -static_cast<float>(bloomRT2_.texture.height)
    };
    DrawTextureRec(bloomRT2_.texture, bRec2, { 0.0f, 0.0f }, WHITE);
    EndShaderMode();
    EndTextureMode();

    // 4. М'яке накладання Bloom адитивно на litRT_
    BeginTextureMode(litRT_);
    BeginBlendMode(BLEND_ADDITIVE);
    Rectangle bSrcRec = {
        0.0f, 0.0f,
        static_cast<float>(bloomRT1_.texture.width),
        -static_cast<float>(bloomRT1_.texture.height)
    };
    Rectangle bDstRec = {
        0.0f, 0.0f,
        static_cast<float>(litRT_.texture.width),
        static_cast<float>(litRT_.texture.height)
    };
    DrawTexturePro(bloomRT1_.texture, bSrcRec, bDstRec, { 0.0f, 0.0f }, 0.0f, Color{ 105, 105, 105, 255 });
    EndBlendMode();
    EndTextureMode();
}

void ShaderManager::PresentFinalScene(
    const GameContext& ctx,
    Vector2 playerScreenPos,
    bool isDarknessBiome,
    bool headlampActive,
    float facingDir,
    bool timeStopActive,
    const std::vector<Vector2>& torchScreenPositions
) {
    // 1. Копіюємо низькороздільний litRT_ у pingTarget_ з Point-фільтрацією
    BeginTextureMode(pingTarget_);
    ClearBackground(BLACK);
    Rectangle srcRec = {
        0.0f, 0.0f,
        static_cast<float>(litRT_.texture.width),
        -static_cast<float>(litRT_.texture.height)
    };
    Rectangle dstRec = {
        0.0f, 0.0f,
        static_cast<float>(targetW_),
        static_cast<float>(targetH_)
    };
    DrawTexturePro(litRT_.texture, srcRec, dstRec, { 0.0f, 0.0f }, 0.0f, WHITE);
    EndTextureMode();

    RenderTexture2D* src = &pingTarget_;
    RenderTexture2D* dst = &pongTarget_;

    auto ApplyPass = [&](Shader& shader) {
        BeginTextureMode(*dst);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
        Rectangle passSrcRec = {
            0.0f, 0.0f,
            static_cast<float>(src->texture.width),
            -static_cast<float>(src->texture.height)
        };
        DrawTextureRec(src->texture, passSrcRec, { 0.0f, 0.0f }, WHITE);
        EndShaderMode();
        EndTextureMode();
        std::swap(src, dst);
    };

    float res[2] = {
        static_cast<float>(targetW_),
        static_cast<float>(targetH_)
    };
    float pPos[2] = {
        playerScreenPos.x * Config::SCALE_FACTOR,
        playerScreenPos.y * Config::SCALE_FACTOR
    };

    if (isDarknessBiome || timeStopActive) {
        int darkFlag = isDarknessBiome ? 1 : 0;
        int lampFlag = headlampActive ? 1 : 0;
        int tsFlag   = timeStopActive ? 1 : 0;
        int torchCount = std::min<int>(8, static_cast<int>(torchScreenPositions.size()));
        float torchData[16]{};
        for (int i = 0; i < torchCount; ++i) {
            torchData[i * 2 + 0] = torchScreenPositions[static_cast<size_t>(i)].x * Config::SCALE_FACTOR;
            torchData[i * 2 + 1] = torchScreenPositions[static_cast<size_t>(i)].y * Config::SCALE_FACTOR;
        }

        SetShaderValue(shDarkness_, locDarkPlayerPos_, pPos, SHADER_UNIFORM_VEC2);
        SetShaderValue(shDarkness_, locDarkResolution_, res, SHADER_UNIFORM_VEC2);
        SetShaderValue(shDarkness_, locDarkHeadlamp_, &lampFlag, SHADER_UNIFORM_INT);
        SetShaderValue(shDarkness_, locDarkFacing_, &facingDir, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shDarkness_, locDarkTimeStop_, &tsFlag, SHADER_UNIFORM_INT);
        SetShaderValue(shDarkness_, locDarkIsDarkBiome_, &darkFlag, SHADER_UNIFORM_INT);
        SetShaderValue(shDarkness_, locDarkTorchCount_, &torchCount, SHADER_UNIFORM_INT);
        if (torchCount > 0) {
            SetShaderValueV(shDarkness_, locDarkTorches_, torchData, SHADER_UNIFORM_VEC2, torchCount);
        }
        ApplyPass(shDarkness_);
    }

    if (ctx.CurrentHeirHasTrait(TraitType::Myopia)) {
        SetShaderValue(shMyopia_, locMyopiaPlayerPos_, pPos, SHADER_UNIFORM_VEC2);
        SetShaderValue(shMyopia_, locMyopiaResolution_, res, SHADER_UNIFORM_VEC2);
        ApplyPass(shMyopia_);
    } else if (ctx.CurrentHeirHasTrait(TraitType::Hyperopia)) {
        SetShaderValue(shHyperopia_, locHyperopiaPlayerPos_, pPos, SHADER_UNIFORM_VEC2);
        SetShaderValue(shHyperopia_, locHyperopiaResolution_, res, SHADER_UNIFORM_VEC2);
        ApplyPass(shHyperopia_);
    }

    // Фінальний грейдинг-шейдер (віньєтка, зернистість, Nostalgic sepia/scratches, F10 grayscale, F8 silhouette)
    float curTime = static_cast<float>(GetTime());
    int isNostalgic = ctx.CurrentHeirHasTrait(TraitType::Nostalgic) ? 1 : 0;
    int isGrayscale = (debugGrayscale_ || ctx.CurrentHeirHasTrait(TraitType::Colorblind)) ? 1 : 0;
    int isSilhouette = debugSilhouette_ ? 1 : 0;

    SetShaderValue(shGrading_, locGradingTime_, &curTime, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shGrading_, locGradingNostalgic_, &isNostalgic, SHADER_UNIFORM_INT);
    SetShaderValue(shGrading_, locGradingGrayscale_, &isGrayscale, SHADER_UNIFORM_INT);
    SetShaderValue(shGrading_, locGradingSilhouette_, &isSilhouette, SHADER_UNIFORM_INT);
    ApplyPass(shGrading_);

    bool hasVertigo = ctx.CurrentHeirHasTrait(TraitType::Vertigo);
    Rectangle finalSrcRec = {
        0.0f, 0.0f,
        static_cast<float>(src->texture.width),
        hasVertigo ? static_cast<float>(src->texture.height)
                   : -static_cast<float>(src->texture.height)
    };

    float outW = static_cast<float>(GetScreenWidth());
    float outH = static_cast<float>(GetScreenHeight());
    if (presentTarget_) {
        BeginTextureMode(*presentTarget_);
        outW = static_cast<float>(presentTarget_->texture.width);
        outH = static_cast<float>(presentTarget_->texture.height);
    }
    Rectangle finalDstRec = { 0.0f, 0.0f, outW, outH };
    DrawTexturePro(src->texture, finalSrcRec, finalDstRec, { 0.0f, 0.0f }, 0.0f, WHITE);
}

void ShaderManager::BeginSceneCapture() {
    Camera2D cam{};
    cam.zoom = Config::SCALE_FACTOR;
    BeginSceneCapture(cam);
}

void ShaderManager::BeginSceneCapture(const Camera2D& cam) {
    BeginScenePass(cam);
}

void ShaderManager::EndSceneAndPresent(
    const GameContext& ctx,
    Vector2 playerScreenPos,
    bool isDarknessBiome,
    bool headlampActive,
    float facingDir,
    bool timeStopActive,
    const std::vector<Vector2>& torchScreenPositions
) {
    EndScenePass();
    CompositeSceneAndLight();
    ApplyBloom();
    PresentFinalScene(
        ctx,
        playerScreenPos,
        isDarknessBiome,
        headlampActive,
        facingDir,
        timeStopActive,
        torchScreenPositions
    );
}
