#include "gfx/ParticleSystem.hpp"
#include "gfx/UiText.hpp"
#include "gfx/Palette.hpp"
#include "core/Config.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {
    inline float RandRange(float minV, float maxV) {
        float r = static_cast<float>(std::rand() % 10000) / 10000.0f;
        return minV + (maxV - minV) * r;
    }
}

ParticleSystem::ParticleSystem() {
    Clear();
}

void ParticleSystem::Clear() {
    for (auto& p : pool_) {
        p.active = false;
    }
    poolHead_ = 0;

    for (auto& t : texts_) {
        t.active = false;
    }
    textHead_ = 0;

    for (auto& l : decoyLogs_) {
        l.active = false;
    }
    logHead_ = 0;
}

void ParticleSystem::EmitParticle(const Particle& p) {
    pool_[poolHead_] = p;
    pool_[poolHead_].active = true;
    poolHead_ = (poolHead_ + 1) % POOL_SIZE;
}

void ParticleSystem::Update(float dt) {
    for (auto& p : pool_) {
        if (!p.active) continue;
        p.life -= dt;
        if (p.life <= 0.0f) {
            p.active = false;
            continue;
        }
        p.vel.y += Config::GRAVITY * p.gravityScale * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.rotation += p.rotSpeed * dt;
    }

    for (auto& ft : texts_) {
        if (!ft.active) continue;
        ft.life -= dt;
        if (ft.life <= 0.0f) {
            ft.active = false;
            continue;
        }
        ft.vel.y += 120.0f * dt;
        ft.pos.x += ft.vel.x * dt;
        ft.pos.y += ft.vel.y * dt;
    }

    for (auto& log : decoyLogs_) {
        if (!log.active) continue;
        log.life -= dt;
        if (log.life <= 0.0f) {
            log.active = false;
            continue;
        }
        log.vel.y += Config::GRAVITY * 0.75f * dt;
        log.pos.x += log.vel.x * dt;
        log.pos.y += log.vel.y * dt;
        log.rotation += 280.0f * dt;
    }
}

void ParticleSystem::Draw() const {
    const auto& pal = GetPalette();

    for (const auto& log : decoyLogs_) {
        if (!log.active) continue;
        float alpha = std::clamp(log.life / 0.4f, 0.0f, 1.0f);
        Rectangle rec = { log.pos.x, log.pos.y, 18.0f, 36.0f };
        DrawRectanglePro(rec, { 9.0f, 18.0f }, log.rotation, Fade(pal.wood[1], alpha));
        DrawRectanglePro({ log.pos.x, log.pos.y, 12.0f, 36.0f }, { 6.0f, 18.0f }, log.rotation, Fade(pal.wood[3], alpha));
    }

    for (const auto& p : pool_) {
        if (!p.active) continue;
        float ratio = std::clamp(p.life / std::max(0.001f, p.maxLife), 0.0f, 1.0f);
        Color c = p.fadeOut ? Fade(p.color, ratio) : p.color;

        if (p.isCircle) {
            DrawCircleV(p.pos, p.size * (p.fadeOut ? (0.4f + 0.6f * ratio) : 1.0f), c);
        } else {
            Rectangle rec = { p.pos.x, p.pos.y, p.size, p.size };
            DrawRectanglePro(rec, { p.size * 0.5f, p.size * 0.5f }, p.rotation, c);
        }
    }

    for (const auto& ft : texts_) {
        if (!ft.active) continue;
        float alpha = std::clamp(ft.life / std::max(0.001f, ft.maxLife), 0.0f, 1.0f);
        int fSize = ft.fontSize;
        if (ft.isCrit && ft.life > ft.maxLife * 0.7f) {
            fSize += 6;
        }
        int textW = Ui::Measure(ft.text, fSize);
        int drawX = static_cast<int>(ft.pos.x) - textW / 2;
        int drawY = static_cast<int>(ft.pos.y);

        Ui::Text(ft.text, drawX + 1, drawY + 1, fSize, Fade(pal.outline, alpha));
        Ui::Text(ft.text, drawX, drawY, fSize, Fade(ft.color, alpha));
    }
}

void ParticleSystem::SpawnBlood(Vector2 pos, int count) {
    const auto& pal = GetPalette();
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = pos;
        float angle = RandRange(-3.14159f, 0.0f);
        float speed = RandRange(80.0f, 340.0f);
        p.vel = { std::cos(angle) * speed, std::sin(angle) * speed - 60.0f };
        p.color = (i % 2 == 0) ? pal.bloodCloth[2] : pal.bloodCloth[1];
        p.size = RandRange(4.0f, 8.0f);
        p.maxLife = RandRange(0.28f, 0.58f);
        p.life = p.maxLife;
        p.gravityScale = 0.85f;
        p.rotSpeed = RandRange(-360.0f, 360.0f);
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnSparks(Vector2 pos, Color tint, int count) {
    const auto& pal = GetPalette();
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = pos;
        float angle = RandRange(0.0f, 6.28318f);
        float speed = RandRange(120.0f, 420.0f);
        p.vel = { std::cos(angle) * speed, std::sin(angle) * speed };
        p.color = (i % 3 == 0) ? pal.fireGold[4] : tint;
        p.size = RandRange(3.0f, 6.5f);
        p.maxLife = RandRange(0.18f, 0.36f);
        p.life = p.maxLife;
        p.gravityScale = 0.25f;
        p.rotSpeed = RandRange(-500.0f, 500.0f);
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnDust(Vector2 pos, int count) {
    const auto& pal = GetPalette();
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = { pos.x + RandRange(-10.0f, 10.0f), pos.y + RandRange(-4.0f, 2.0f) };
        p.vel = { RandRange(-75.0f, 75.0f), RandRange(-65.0f, -15.0f) };
        p.color = pal.bone[2];
        p.size = RandRange(5.0f, 10.0f);
        p.maxLife = RandRange(0.22f, 0.42f);
        p.life = p.maxLife;
        p.gravityScale = -0.08f;
        p.isCircle = true;
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnSmoke(Vector2 pos, Color tint, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = { pos.x + RandRange(-14.0f, 14.0f), pos.y + RandRange(-18.0f, 18.0f) };
        float angle = RandRange(0.0f, 6.28318f);
        float speed = RandRange(30.0f, 145.0f);
        p.vel = { std::cos(angle) * speed, std::sin(angle) * speed - 25.0f };
        p.color = tint;
        p.size = RandRange(9.0f, 18.0f);
        p.maxLife = RandRange(0.35f, 0.70f);
        p.life = p.maxLife;
        p.gravityScale = -0.12f;
        p.isCircle = true;
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnDebris(Vector2 pos, Color tint, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = { pos.x + RandRange(-12.0f, 12.0f), pos.y + RandRange(-12.0f, 12.0f) };
        p.vel = { RandRange(-220.0f, 220.0f), RandRange(-380.0f, -110.0f) };
        p.color = tint;
        p.size = RandRange(5.0f, 11.0f);
        p.maxLife = RandRange(0.45f, 0.85f);
        p.life = p.maxLife;
        p.gravityScale = 0.95f;
        p.rotSpeed = RandRange(-480.0f, 480.0f);
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnGasCloud(Vector2 pos) {
    const auto& pal = GetPalette();
    for (int i = 0; i < 14; ++i) {
        Particle p;
        p.pos = { pos.x + RandRange(-10.0f, 10.0f), pos.y + RandRange(-6.0f, 6.0f) };
        p.vel = { RandRange(-65.0f, 65.0f), RandRange(-50.0f, 15.0f) };
        p.color = (i % 2 == 0) ? pal.poisonSlime[2] : pal.poisonSlime[1];
        p.size = RandRange(8.0f, 16.0f);
        p.maxLife = RandRange(0.45f, 0.85f);
        p.life = p.maxLife;
        p.gravityScale = -0.10f;
        p.isCircle = true;
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnExplosion(Vector2 pos, float radius, Color tint) {
    const auto& pal = GetPalette();
    int count = std::clamp(static_cast<int>(radius * 0.35f), 20, 75);
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = pos;
        float angle = RandRange(0.0f, 6.28318f);
        float speed = RandRange(60.0f, radius * 2.2f);
        p.vel = { std::cos(angle) * speed, std::sin(angle) * speed };
        p.color = (i % 3 == 0) ? pal.fireGold[3] : tint;
        p.size = RandRange(6.0f, 14.0f);
        p.maxLife = RandRange(0.25f, 0.55f);
        p.life = p.maxLife;
        p.gravityScale = 0.0f;
        p.isCircle = (i % 2 == 0);
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnHealSparkles(Vector2 pos, Color tint, int count) {
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = { pos.x + RandRange(-16.0f, 16.0f), pos.y + RandRange(-20.0f, 20.0f) };
        p.vel = { RandRange(-30.0f, 30.0f), RandRange(-120.0f, -45.0f) };
        p.color = tint;
        p.size = RandRange(4.0f, 7.0f);
        p.maxLife = RandRange(0.35f, 0.65f);
        p.life = p.maxLife;
        p.gravityScale = -0.15f;
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnDecoyLog(Vector2 pos) {
    DecoyLog log;
    log.pos = pos;
    log.vel = { RandRange(-60.0f, 60.0f), -220.0f };
    log.rotation = RandRange(-25.0f, 25.0f);
    log.life = 1.20f;
    log.active = true;

    decoyLogs_[logHead_] = log;
    logHead_ = (logHead_ + 1) % LOG_POOL_SIZE;

    SpawnSmoke(pos, GetPalette().bone[2], 20);
}

void ParticleSystem::SpawnBones(Vector2 pos, int count) {
    const auto& pal = GetPalette();
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = pos;
        float angle = RandRange(-2.8f, -0.34f);
        float speed = RandRange(120.0f, 320.0f);
        p.vel = { std::cos(angle) * speed, std::sin(angle) * speed };
        p.color = (i % 2 == 0) ? pal.bone[4] : pal.bone[3];
        p.size = RandRange(4.0f, 7.5f);
        p.maxLife = RandRange(0.45f, 0.85f);
        p.life = p.maxLife;
        p.gravityScale = 1.1f;
        p.rotSpeed = RandRange(-550.0f, 550.0f);
        p.isCircle = false;
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnGoldSparkles(Vector2 pos, int count) {
    const auto& pal = GetPalette();
    for (int i = 0; i < count; ++i) {
        Particle p;
        p.pos = { pos.x + RandRange(-12.0f, 12.0f), pos.y + RandRange(-12.0f, 6.0f) };
        p.vel = { RandRange(-45.0f, 45.0f), RandRange(-110.0f, -30.0f) };
        p.color = (i % 2 == 0) ? pal.fireGold[4] : pal.fireGold[3];
        p.size = RandRange(3.0f, 6.0f);
        p.maxLife = RandRange(0.25f, 0.50f);
        p.life = p.maxLife;
        p.gravityScale = -0.05f;
        p.isCircle = (i % 2 == 0);
        EmitParticle(p);
    }
}

void ParticleSystem::SpawnFloatingText(
    Vector2 pos,
    const std::string& text,
    Color color,
    int fontSize,
    bool isCrit
) {
    FloatingText ft;
    ft.pos = { pos.x + RandRange(-10.0f, 10.0f), pos.y - 12.0f };
    ft.vel = { RandRange(-25.0f, 25.0f), isCrit ? -135.0f : -95.0f };
    ft.text = text;
    ft.color = color;
    ft.fontSize = fontSize;
    ft.maxLife = isCrit ? 1.05f : 0.80f;
    ft.life = ft.maxLife;
    ft.isCrit = isCrit;
    ft.active = true;

    texts_[textHead_] = ft;
    textHead_ = (textHead_ + 1) % TEXT_POOL_SIZE;
}
