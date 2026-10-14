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

} // anonymous namespace

ShaderManager::~ShaderManager() {
    Shutdown();
}

void ShaderManager::Init() {
    if (initialized_) return;

    pingTarget_ = LoadRenderTexture(Config::RENDER_WIDTH, Config::RENDER_HEIGHT);
    pongTarget_ = LoadRenderTexture(Config::RENDER_WIDTH, Config::RENDER_HEIGHT);

    shColorblind_ = LoadShaderFromMemory(nullptr, FS_COLORBLIND);
    shNostalgic_  = LoadShaderFromMemory(nullptr, FS_NOSTALGIC);
    shMyopia_     = LoadShaderFromMemory(nullptr, FS_MYOPIA);
    shHyperopia_  = LoadShaderFromMemory(nullptr, FS_HYPEROPIA);
    shDarkness_   = LoadShaderFromMemory(nullptr, FS_DARKNESS);

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
}

void ShaderManager::Shutdown() {
    if (!initialized_) return;

    UnloadRenderTexture(pingTarget_);
    UnloadRenderTexture(pongTarget_);

    UnloadShader(shColorblind_);
    UnloadShader(shNostalgic_);
    UnloadShader(shMyopia_);
    UnloadShader(shHyperopia_);
    UnloadShader(shDarkness_);

    initialized_ = false;
}

void ShaderManager::BeginSceneCapture() {
    BeginTextureMode(pingTarget_);
    ClearBackground(BLACK);
    Camera2D cam{};
    cam.zoom = Config::SCALE_FACTOR;
    BeginMode2D(cam);
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
    EndMode2D();
    EndTextureMode();

    float res[2] = {
        static_cast<float>(Config::RENDER_WIDTH),
        static_cast<float>(Config::RENDER_HEIGHT)
    };
    float pPos[2] = {
        playerScreenPos.x * Config::SCALE_FACTOR,
        playerScreenPos.y * Config::SCALE_FACTOR
    };

    RenderTexture2D* src = &pingTarget_;
    RenderTexture2D* dst = &pongTarget_;

    auto ApplyPass = [&](Shader& shader) {
        BeginTextureMode(*dst);
        ClearBackground(BLANK);
        BeginShaderMode(shader);
        Rectangle srcRec = {
            0.0f, 0.0f,
            static_cast<float>(src->texture.width),
            -static_cast<float>(src->texture.height)
        };
        DrawTextureRec(src->texture, srcRec, { 0.0f, 0.0f }, WHITE);
        EndShaderMode();
        EndTextureMode();
        std::swap(src, dst);
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

    if (ctx.CurrentHeirHasTrait(TraitType::Colorblind)) {
        ApplyPass(shColorblind_);
    } else if (ctx.CurrentHeirHasTrait(TraitType::Nostalgic)) {
        float timeVal = static_cast<float>(GetTime());
        SetShaderValue(shNostalgic_, locNostalgicTime_, &timeVal, SHADER_UNIFORM_FLOAT);
        ApplyPass(shNostalgic_);
    }

    bool hasVertigo = ctx.CurrentHeirHasTrait(TraitType::Vertigo);
    Rectangle finalSrcRec = {
        0.0f, 0.0f,
        static_cast<float>(src->texture.width),
        hasVertigo ? static_cast<float>(src->texture.height)
                   : -static_cast<float>(src->texture.height)
    };
    // EndTextureMode у raylib завжди повертає на екран, тому знову вмикаємо полотно,
    // щоб фінальний кадр і HUD після нього малювались у полотно 1920x1080.
    float outW = static_cast<float>(GetScreenWidth());
    float outH = static_cast<float>(GetScreenHeight());
    if (presentTarget_) {
        BeginTextureMode(*presentTarget_);
        outW = static_cast<float>(Config::RENDER_WIDTH);
        outH = static_cast<float>(Config::RENDER_HEIGHT);
    }
    Rectangle finalDstRec = { 0.0f, 0.0f, outW, outH };
    DrawTexturePro(src->texture, finalSrcRec, finalDstRec, { 0.0f, 0.0f }, 0.0f, WHITE);
}
