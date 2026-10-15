// === FILE: src/entities/Projectile.cpp ===
#include "entities/Projectile.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/Palette.hpp"
#include <cmath>
#include <algorithm>

Projectile::Projectile() {
    static uint32_t nextProjId = 1;
    id = nextProjId++;
}

Rectangle Projectile::GetHitbox() const {
    if (kind == ProjectileKind::EnemyFirePillar) {
        float w = 28.0f * scale;
        float h = 88.0f * scale;
        return Rectangle{ pos.x - w * 0.5f, pos.y - h, w, h };
    }
    if (kind == ProjectileKind::BossSwordWave) {
        float w = 24.0f * scale;
        float h = 68.0f * scale;
        return Rectangle{ pos.x - w * 0.5f, pos.y - h * 0.5f, w, h };
    }
    float r = radius * scale;
    return Rectangle{ pos.x - r, pos.y - r, r * 2.0f, r * 2.0f };
}

bool Projectile::HasAlreadyHit(uint32_t entityId) const {
    return std::find(hitIds.begin(), hitIds.end(), entityId) != hitIds.end();
}

void Projectile::RegisterHit(uint32_t entityId) {
    if (!HasAlreadyHit(entityId)) {
        hitIds.push_back(entityId);
    }
}

void Projectile::Update(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    Vector2 playerCenter,
    bool timeStopActive
) {
    if (!active) return;

    // Під час дії TimeStop ворожі снаряди повністю заморожені
    if (!fromPlayer && timeStopActive) {
        return;
    }

    life -= dt;
    if (life <= 0.0f) {
        active = false;
        return;
    }

    // Очищення списку влучань для орбітальних сфер кожні 0.35с, щоб вони могли бити повторно
    if (kind == ProjectileKind::PlayerFlameOrb) {
        rehitResetTimer += dt;
        if (rehitResetTimer >= 0.35f) {
            rehitResetTimer = 0.0f;
            hitIds.clear();
        }
    }

    rotation += rotSpeed * dt;

    switch (kind) {
        case ProjectileKind::PlayerAxe:
        case ProjectileKind::EnemyBone:
        case ProjectileKind::BossAcidDrop: {
            vel.y += Config::GRAVITY * 0.78f * dt;
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            break;
        }

        case ProjectileKind::PlayerChakram: {
            // Бумеранг: летить вперед, гальмує і повертається до поточної позиції гравця
            float elapsed = maxLife - life;
            if (!returningToPlayer && elapsed >= 0.42f) {
                returningToPlayer = true;
                hitIds.clear(); // Може вдарити ворогів вдруге на зворотному шляху
            }
            if (returningToPlayer) {
                Vector2 toPlayer = { playerCenter.x - pos.x, playerCenter.y - pos.y };
                float dist = std::hypot(toPlayer.x, toPlayer.y);
                if (dist < 26.0f && elapsed > 0.55f) {
                    active = false;
                    return;
                }
                if (dist > 0.001f) {
                    Vector2 dir = { toPlayer.x / dist, toPlayer.y / dist };
                    vel.x += dir.x * 2400.0f * dt;
                    vel.y += dir.y * 2400.0f * dt;
                    float spd = std::hypot(vel.x, vel.y);
                    if (spd > 680.0f) {
                        vel.x = (vel.x / spd) * 680.0f;
                        vel.y = (vel.y / spd) * 680.0f;
                    }
                }
            }
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            break;
        }

        case ProjectileKind::PlayerFlameOrb: {
            // Обертається по орбіті навколо героя
            orbitAngle += 5.4f * dt;
            float r = orbitRadius * scale;
            pos.x = playerCenter.x + std::cos(orbitAngle) * r;
            pos.y = playerCenter.y + std::sin(orbitAngle) * r;
            if (ctx.particles && (std::rand() % 3 == 0)) {
                ctx.particles->SpawnSparks(pos, GetPalette().fireGold[3], 1);
            }
            break;
        }

        case ProjectileKind::PlayerConflux: {
            // Рикошетить від твердих тайлів кімнати
            float nextX = pos.x + vel.x * dt;
            float nextY = pos.y + vel.y * dt;
            if (tileMap) {
                int colX = tileMap->WorldToCol(nextX + (vel.x > 0.0f ? radius : -radius));
                int rowX = tileMap->WorldToRow(pos.y);
                if (tileMap->IsSolidAt(colX, rowX, false)) {
                    vel.x = -vel.x;
                    nextX = pos.x + vel.x * dt;
                    if (--bouncesLeft < 0) active = false;
                    if (ctx.particles) ctx.particles->SpawnSparks(pos, GetPalette().moonMagic[2], 4);
                }

                int colY = tileMap->WorldToCol(pos.x);
                int rowY = tileMap->WorldToRow(nextY + (vel.y > 0.0f ? radius : -radius));
                if (tileMap->IsSolidAt(colY, rowY, false)) {
                    vel.y = -vel.y;
                    nextY = pos.y + vel.y * dt;
                    if (--bouncesLeft < 0) active = false;
                    if (ctx.particles) ctx.particles->SpawnSparks(pos, GetPalette().moonMagic[2], 4);
                }
            }
            pos.x = nextX;
            pos.y = nextY;
            break;
        }

        case ProjectileKind::PlayerCrow: {
            // Самонавідний ворон
            if (hasValidTargetHint) {
                Vector2 toTarget = { targetHint.x - pos.x, targetHint.y - pos.y };
                float d = std::hypot(toTarget.x, toTarget.y);
                if (d > 1.0f) {
                    Vector2 desired = { (toTarget.x / d) * 520.0f, (toTarget.y / d) * 520.0f };
                    vel.x += (desired.x - vel.x) * 8.5f * dt;
                    vel.y += (desired.y - vel.y) * 8.5f * dt;
                }
            }
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            rotation = std::atan2(vel.y, vel.x) * (180.0f / 3.14159265f);
            break;
        }

        case ProjectileKind::EnemyFirePillar:
        case ProjectileKind::BossFlameTrail:
        case ProjectileKind::PlayerPaladinLightning: {
            // Стаціонарні або вертикально пульсуючі ефекти
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            break;
        }

        default: {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            break;
        }
    }

    // Перевірка зіткнення зі стінами для звичайних непроникних снарядів
    if (!ignoreWalls && tileMap && kind != ProjectileKind::PlayerConflux) {
        int c = tileMap->WorldToCol(pos.x);
        int r = tileMap->WorldToRow(pos.y);
        if (tileMap->IsSolidAt(c, r, false)) {
            if (ctx.particles) {
                ctx.particles->SpawnSparks(pos, primaryColor, 6);
            }
            active = false;
            return;
        }
    }

    // Вихід за межі кімнати (з урахуванням розмірів багатоклітинних кімнат 1x1, 2x1, 1x2, 2x2)
    float maxBoundW = tileMap ? static_cast<float>(tileMap->GetPixelWidth()) : static_cast<float>(Config::VIRTUAL_WIDTH);
    float maxBoundH = tileMap ? static_cast<float>(tileMap->GetPixelHeight()) : static_cast<float>(Config::VIRTUAL_HEIGHT);
    if (pos.x < -120.0f || pos.x > maxBoundW + 120.0f ||
        pos.y < -180.0f || pos.y > maxBoundH + 120.0f) {
        active = false;
    }
}

void Projectile::Draw() const {
    if (!active) return;
    const auto& pal = GetPalette();

    float s = scale;
    switch (kind) {
        case ProjectileKind::PlayerDagger: {
            float dir = (vel.x >= 0.0f) ? 1.0f : -1.0f;
            DrawRectanglePro(
                { pos.x, pos.y, 28.0f * s, 8.0f * s },
                { 14.0f * s, 4.0f * s },
                0.0f,
                pal.outline
            );
            DrawRectanglePro(
                { pos.x, pos.y, 26.0f * s, 6.0f * s },
                { 13.0f * s, 3.0f * s },
                0.0f,
                pal.metal[3]
            );
            DrawTriangle(
                { pos.x + dir * 13.0f * s, pos.y - 4.0f * s },
                { pos.x + dir * 13.0f * s, pos.y + 4.0f * s },
                { pos.x + dir * 21.0f * s, pos.y },
                pal.metal[4]
            );
            DrawRectanglePro(
                { pos.x - dir * 10.0f * s, pos.y, 4.0f * s, 12.0f * s },
                { 2.0f * s, 6.0f * s },
                0.0f,
                pal.fireGold[3]
            );
            break;
        }

        case ProjectileKind::PlayerAxe: {
            DrawRectanglePro(
                { pos.x, pos.y, 8.0f * s, 36.0f * s },
                { 4.0f * s, 18.0f * s },
                rotation,
                pal.outline
            );
            DrawRectanglePro(
                { pos.x, pos.y, 6.0f * s, 34.0f * s },
                { 3.0f * s, 17.0f * s },
                rotation,
                pal.wood[2]
            );
            DrawCircleSector(pos, 20.0f * s, rotation - 48.0f, rotation + 48.0f, 10, pal.outline);
            DrawCircleSector(pos, 18.0f * s, rotation - 45.0f, rotation + 45.0f, 10, pal.metal[3]);
            DrawCircleSector(pos, 20.0f * s, rotation + 132.0f, rotation + 228.0f, 10, pal.outline);
            DrawCircleSector(pos, 18.0f * s, rotation + 135.0f, rotation + 225.0f, 10, pal.metal[3]);
            break;
        }

        case ProjectileKind::PlayerChakram: {
            DrawRing(pos, 8.0f * s, 17.0f * s, 0.0f, 360.0f, 18, pal.outline);
            DrawRing(pos, 9.0f * s, 16.0f * s, 0.0f, 360.0f, 18, pal.moonMagic[2]);
            DrawRing(pos, 12.0f * s, 17.5f * s, rotation, rotation + 90.0f, 8, pal.moonMagic[4]);
            DrawRing(pos, 12.0f * s, 17.5f * s, rotation + 180.0f, rotation + 270.0f, 8, pal.moonMagic[4]);
            break;
        }

        case ProjectileKind::PlayerScythe: {
            DrawRectanglePro(
                { pos.x, pos.y, 7.0f * s, 40.0f * s },
                { 3.5f * s, 20.0f * s },
                rotation,
                pal.outline
            );
            DrawRectanglePro(
                { pos.x, pos.y, 5.0f * s, 38.0f * s },
                { 2.5f * s, 19.0f * s },
                rotation,
                pal.bloodCloth[1]
            );
            DrawCircleSector(pos, 24.0f * s, rotation, rotation + 115.0f, 12, pal.outline);
            DrawCircleSector(pos, 22.0f * s, rotation, rotation + 115.0f, 12, pal.metal[4]);
            break;
        }

        case ProjectileKind::PlayerFlameOrb:
        case ProjectileKind::PlayerDragonFireball:
        case ProjectileKind::BossFlameTrail: {
            float pulse = 1.0f + 0.15f * std::sin(life * 18.0f);
            float r = radius * s * pulse;
            DrawCircleV(pos, r * 1.35f, Fade(pal.fireGold[0], 0.40f));
            DrawCircleV(pos, r, pal.fireGold[2]);
            DrawCircleV(pos, r * 0.55f, pal.fireGold[4]);
            break;
        }

        case ProjectileKind::PlayerConflux: {
            float r = radius * s;
            DrawCircleV(pos, r * 1.4f, pal.outline);
            DrawCircleV(pos, r * 1.35f, Fade(pal.moonMagic[1], 0.35f));
            DrawCircleV(pos, r, pal.moonMagic[2]);
            DrawCircleV(pos, r * 0.5f, pal.moonMagic[4]);
            break;
        }

        case ProjectileKind::PlayerCrow: {
            float flap = std::sin(life * 26.0f) * 8.0f * s;
            DrawCircleV(pos, 9.0f * s, pal.outline);
            DrawCircleV(pos, 8.0f * s, pal.stoneBg[1]);
            DrawTriangle(
                pos,
                { pos.x - 10.0f * s, pos.y - 12.0f * s + flap },
                { pos.x - 4.0f * s,  pos.y + 4.0f * s },
                pal.bloodCloth[0]
            );
            DrawCircleV({ pos.x + 4.0f * s, pos.y - 2.0f * s }, 2.5f * s, pal.bloodCloth[4]);
            break;
        }

        case ProjectileKind::PlayerAncestorBlade:
        case ProjectileKind::BossSwordWave: {
            float w = 16.0f * s;
            float h = 58.0f * s;
            DrawRectanglePro({ pos.x, pos.y, w + 4.0f * s, h + 4.0f * s }, { (w + 4.0f * s) * 0.5f, (h + 4.0f * s) * 0.5f }, 0.0f, pal.outline);
            DrawRectanglePro({ pos.x, pos.y, w, h }, { w * 0.5f, h * 0.5f }, 0.0f, primaryColor);
            DrawRectanglePro({ pos.x, pos.y, w * 0.45f, h * 0.9f }, { w * 0.225f, h * 0.45f }, 0.0f, pal.moonMagic[4]);
            break;
        }

        case ProjectileKind::PlayerPaladinLightning: {
            for (int i = -1; i <= 1; ++i) {
                float offX = i * 6.0f;
                DrawLineEx({ pos.x + offX, 0.0f }, { pos.x - offX, pos.y + 20.0f }, 4.0f, pal.moonMagic[2]);
            }
            DrawCircleV(pos, 26.0f, Fade(pal.moonMagic[4], 0.7f));
            break;
        }

        case ProjectileKind::EnemyBone: {
            DrawRectanglePro(
                { pos.x, pos.y, 26.0f * s, 8.0f * s },
                { 13.0f * s, 4.0f * s },
                rotation,
                pal.outline
            );
            DrawRectanglePro(
                { pos.x, pos.y, 24.0f * s, 6.0f * s },
                { 12.0f * s, 3.0f * s },
                rotation,
                pal.bone[3]
            );
            float rad = rotation * (3.14159265f / 180.0f);
            Vector2 endA = { pos.x + std::cos(rad) * 11.0f * s, pos.y + std::sin(rad) * 11.0f * s };
            Vector2 endB = { pos.x - std::cos(rad) * 11.0f * s, pos.y - std::sin(rad) * 11.0f * s };
            DrawCircleV(endA, 5.5f * s, pal.outline);
            DrawCircleV(endB, 5.5f * s, pal.outline);
            DrawCircleV(endA, 4.5f * s, pal.bone[4]);
            DrawCircleV(endB, 4.5f * s, pal.bone[4]);
            break;
        }

        case ProjectileKind::EnemyEyeBolt: {
            float r = radius * s;
            DrawCircleV(pos, r * 1.35f, pal.outline);
            DrawCircleV(pos, r * 1.3f, Fade(primaryColor, 0.35f));
            DrawCircleV(pos, r, primaryColor);
            DrawCircleV(pos, r * 0.48f, pal.danger[4]);
            break;
        }

        case ProjectileKind::EnemyFirePillar: {
            float ratio = std::clamp(life / std::max(0.01f, maxLife), 0.0f, 1.0f);
            float baseW = 34.0f * s * (0.6f + 0.4f * std::sin(ratio * 3.14159f));
            float flameH = 105.0f * s * (0.55f + 0.45f * std::sin(ratio * 3.14159f));

            // 1. Теплове сяйво навколо вогняного стовпа (Heat haze glow)
            DrawCircleGradient(
                static_cast<int>(pos.x),
                static_cast<int>(pos.y - flameH * 0.45f),
                baseW * 1.5f,
                Fade(pal.fireGold[1], 0.30f * ratio),
                BLANK
            );

            // 2. Киплячий розплавлений басейн на підлозі (Boiling magma pool)
            DrawEllipse(static_cast<int>(pos.x), static_cast<int>(pos.y), baseW * 0.95f, 7.0f * s, pal.fireGold[0]);
            DrawEllipse(static_cast<int>(pos.x), static_cast<int>(pos.y), baseW * 0.65f, 4.5f * s, pal.fireGold[2]);
            DrawEllipse(static_cast<int>(pos.x), static_cast<int>(pos.y), baseW * 0.35f, 2.5f * s, pal.fireGold[4]);

            // 3. Багатошарові палаючі язики полум'я, що звиваються
            float wave1 = std::sin(life * 24.0f) * 7.0f * s;
            float wave2 = std::cos(life * 28.0f) * 6.0f * s;
            float wave3 = std::sin(life * 32.0f + 1.2f) * 5.0f * s;

            // Зовнішній шар
            DrawTriangle(
                { pos.x - baseW * 0.5f, pos.y },
                { pos.x - baseW * 0.15f + wave1, pos.y - flameH * 0.88f },
                { pos.x + 2.0f, pos.y },
                pal.fireGold[1]
            );
            DrawTriangle(
                { pos.x - 2.0f, pos.y },
                { pos.x + baseW * 0.15f + wave2, pos.y - flameH * 0.85f },
                { pos.x + baseW * 0.5f, pos.y },
                pal.fireGold[1]
            );
            DrawTriangle(
                { pos.x - baseW * 0.38f, pos.y },
                { pos.x + wave3, pos.y - flameH },
                { pos.x + baseW * 0.38f, pos.y },
                pal.danger[2]
            );

            // Середній шар
            DrawTriangle(
                { pos.x - baseW * 0.32f, pos.y },
                { pos.x + wave1 * 0.6f, pos.y - flameH * 0.80f },
                { pos.x + baseW * 0.32f, pos.y },
                pal.fireGold[2]
            );
            DrawTriangle(
                { pos.x - baseW * 0.22f, pos.y },
                { pos.x + wave2 * 0.5f, pos.y - flameH * 0.68f },
                { pos.x + baseW * 0.22f, pos.y },
                pal.fireGold[3]
            );

            // Внутрішнє ядро
            DrawTriangle(
                { pos.x - baseW * 0.14f, pos.y },
                { pos.x + wave3 * 0.3f, pos.y - flameH * 0.48f },
                { pos.x + baseW * 0.14f, pos.y },
                pal.fireGold[4]
            );
            DrawTriangle(
                { pos.x - baseW * 0.08f, pos.y },
                { pos.x, pos.y - flameH * 0.30f },
                { pos.x + baseW * 0.08f, pos.y },
                pal.fireGold[4]
            );

            // 4. Ембери
            for (int k = 0; k < 6; ++k) {
                float emberY = pos.y - std::fmod(life * (130.0f + k * 20.0f) + k * 18.0f, flameH + 28.0f);
                float emberX = pos.x + std::sin(life * 14.0f + k * 1.5f) * (baseW * 0.55f);
                float emberR = (1.5f + (k % 2) * 1.0f) * s;
                Color emberCol = (k % 2 == 0) ? pal.fireGold[4] : pal.fireGold[3];
                DrawCircle(static_cast<int>(emberX), static_cast<int>(emberY), emberR, emberCol);
                DrawCircle(static_cast<int>(emberX), static_cast<int>(emberY), emberR * 0.5f, pal.fireGold[4]);
            }
            break;
        }

        case ProjectileKind::EnemyArrow: {
            float dir = (vel.x >= 0.0f) ? 1.0f : -1.0f;
            DrawLineEx({ pos.x - dir * 16.0f, pos.y }, { pos.x + dir * 16.0f, pos.y }, 6.0f, pal.outline);
            DrawLineEx({ pos.x - dir * 15.0f, pos.y }, { pos.x + dir * 15.0f, pos.y }, 4.0f, pal.wood[2]);
            DrawCircleV({ pos.x + dir * 14.0f, pos.y }, 6.0f, pal.outline);
            DrawCircleV({ pos.x + dir * 14.0f, pos.y }, 4.5f, pal.danger[2]);
            break;
        }

        case ProjectileKind::BossAcidDrop: {
            DrawCircleV(pos, radius * s + 1.5f, pal.outline);
            DrawCircleV(pos, radius * s, pal.poisonSlime[3]);
            DrawCircleV({ pos.x - 2.0f, pos.y - 2.0f }, radius * s * 0.45f, pal.poisonSlime[4]);
            break;
        }
    }
}

void Projectile::SpawnPlayerSpell(
    std::vector<Projectile>& outList,
    SpellType spell,
    Vector2 origin,
    float facingDir,
    int baseDamage,
    float empowerScale,
    float empowerDmgMult
) {
    float dir = (facingDir >= 0.0f) ? 1.0f : -1.0f;
    int finalDmg = std::max(1, static_cast<int>(std::round(baseDamage * empowerDmgMult)));

    switch (spell) {
        case SpellType::Dagger: {
            Projectile p;
            p.kind = ProjectileKind::PlayerDagger;
            p.fromPlayer = true;
            p.pos = origin;
            p.vel = { dir * 740.0f, 0.0f };
            p.radius = 10.0f;
            p.scale = empowerScale;
            p.damage = finalDmg;
            p.life = 1.4f;
            p.maxLife = 1.4f;
            p.piercing = (empowerScale > 1.2f);
            outList.push_back(p);
            break;
        }

        case SpellType::Axe: {
            Projectile p;
            p.kind = ProjectileKind::PlayerAxe;
            p.fromPlayer = true;
            p.pos = origin;
            p.vel = { dir * 290.0f, -660.0f };
            p.radius = 18.0f;
            p.scale = empowerScale;
            p.damage = finalDmg;
            p.life = 2.2f;
            p.maxLife = 2.2f;
            p.rotSpeed = dir * 640.0f;
            p.piercing = true;
            p.ignoreWalls = true;
            outList.push_back(p);
            break;
        }

        case SpellType::Chakram: {
            Projectile p;
            p.kind = ProjectileKind::PlayerChakram;
            p.fromPlayer = true;
            p.pos = origin;
            p.vel = { dir * 560.0f, 0.0f };
            p.radius = 16.0f;
            p.scale = empowerScale;
            p.damage = finalDmg;
            p.life = 2.6f;
            p.maxLife = 2.6f;
            p.rotSpeed = 780.0f;
            p.piercing = true;
            p.ignoreWalls = true;
            outList.push_back(p);
            break;
        }

        case SpellType::Scythe: {
            Projectile p;
            p.kind = ProjectileKind::PlayerScythe;
            p.fromPlayer = true;
            p.pos = origin;
            p.vel = { dir * 460.0f, -460.0f };
            p.radius = 20.0f;
            p.scale = empowerScale;
            p.damage = finalDmg;
            p.life = 1.8f;
            p.maxLife = 1.8f;
            p.rotSpeed = dir * 720.0f;
            p.piercing = true;
            p.ignoreWalls = true;
            outList.push_back(p);
            break;
        }

        case SpellType::FlameBarrier: {
            int orbCount = (empowerScale > 1.2f) ? 4 : 2;
            for (int i = 0; i < orbCount; ++i) {
                Projectile p;
                p.kind = ProjectileKind::PlayerFlameOrb;
                p.fromPlayer = true;
                p.pos = origin;
                p.radius = 14.0f;
                p.scale = empowerScale;
                p.damage = finalDmg;
                p.life = 4.5f;
                p.maxLife = 4.5f;
                p.orbitAngle = (6.283185f / static_cast<float>(orbCount)) * static_cast<float>(i);
                p.orbitRadius = 68.0f;
                p.piercing = true;
                p.ignoreWalls = true;
                outList.push_back(p);
            }
            break;
        }

        case SpellType::Conflux: {
            const std::array<Vector2, 4> dirs = {{
                {  380.0f, -380.0f },
                { -380.0f, -380.0f },
                {  380.0f,  280.0f },
                { -380.0f,  280.0f }
            }};
            for (const Vector2& v : dirs) {
                Projectile p;
                p.kind = ProjectileKind::PlayerConflux;
                p.fromPlayer = true;
                p.pos = origin;
                p.vel = v;
                p.radius = 11.0f;
                p.scale = empowerScale;
                p.damage = finalDmg;
                p.life = 3.2f;
                p.maxLife = 3.2f;
                p.bouncesLeft = 5;
                outList.push_back(p);
            }
            break;
        }

        case SpellType::CrowStorm: {
            int crowCount = (empowerScale > 1.2f) ? 6 : 4;
            for (int i = 0; i < crowCount; ++i) {
                float angle = (6.283185f / static_cast<float>(crowCount)) * static_cast<float>(i);
                Projectile p;
                p.kind = ProjectileKind::PlayerCrow;
                p.fromPlayer = true;
                p.pos = origin;
                p.vel = { std::cos(angle) * 260.0f, std::sin(angle) * 260.0f - 80.0f };
                p.radius = 13.0f;
                p.scale = empowerScale;
                p.damage = finalDmg;
                p.life = 2.4f;
                p.maxLife = 2.4f;
                p.ignoreWalls = true;
                outList.push_back(p);
            }
            break;
        }

        case SpellType::TimeStop:
        default:
            break;
    }
}

Projectile Projectile::MakeDragonFireball(Vector2 origin, Vector2 dir, int damage) {
    Projectile p;
    p.kind = ProjectileKind::PlayerDragonFireball;
    p.fromPlayer = true;
    p.pos = origin;
    float len = std::hypot(dir.x, dir.y);
    if (len < 0.001f) dir = { 1.0f, 0.0f };
    else dir = { dir.x / len, dir.y / len };
    p.vel = { dir.x * 580.0f, dir.y * 580.0f };
    p.radius = 13.0f;
    p.damage = damage;
    p.life = 1.6f;
    p.maxLife = 1.6f;
    return p;
}

Projectile Projectile::MakeAncestorBlade(float xPos, int damage) {
    Projectile p;
    p.kind = ProjectileKind::PlayerAncestorBlade;
    p.fromPlayer = true;
    p.pos = { xPos, 40.0f };
    p.vel = { 0.0f, 680.0f };
    p.radius = 16.0f;
    p.damage = damage;
    p.life = 1.4f;
    p.maxLife = 1.4f;
    p.piercing = true;
    p.ignoreWalls = true;
    p.primaryColor = GetPalette().moonMagic[2];
    return p;
}

Projectile Projectile::MakePaladinLightning(Vector2 targetPos, int damage) {
    Projectile p;
    p.kind = ProjectileKind::PlayerPaladinLightning;
    p.fromPlayer = true;
    p.pos = targetPos;
    p.vel = { 0.0f, 0.0f };
    p.radius = 34.0f;
    p.damage = damage;
    p.life = 0.22f;
    p.maxLife = 0.22f;
    p.piercing = true;
    p.ignoreWalls = true;
    return p;
}

Projectile Projectile::MakeEnemyBone(Vector2 origin, Vector2 velocity, int damage) {
    Projectile p;
    p.kind = ProjectileKind::EnemyBone;
    p.fromPlayer = false;
    p.pos = origin;
    p.vel = velocity;
    p.radius = 10.0f;
    p.damage = damage;
    p.life = 2.6f;
    p.maxLife = 2.6f;
    p.rotSpeed = 520.0f;
    p.ignoreWalls = false;
    p.destroyableBySword = true;
    return p;
}

Projectile Projectile::MakeEnemyEyeBolt(Vector2 origin, Vector2 velocity, int damage, Color tint) {
    Projectile p;
    p.kind = ProjectileKind::EnemyEyeBolt;
    p.fromPlayer = false;
    p.pos = origin;
    p.vel = velocity;
    p.radius = 10.0f;
    p.damage = damage;
    p.life = 3.8f;
    p.maxLife = 3.8f;
    p.ignoreWalls = true;
    p.primaryColor = tint;
    p.destroyableBySword = false;
    return p;
}

Projectile Projectile::MakeEnemyFirePillar(Vector2 floorPos, int damage) {
    Projectile p;
    p.kind = ProjectileKind::EnemyFirePillar;
    p.fromPlayer = false;
    p.pos = floorPos;
    p.vel = { 0.0f, 0.0f };
    p.radius = 16.0f;
    p.damage = damage;
    p.life = 0.85f;
    p.maxLife = 0.85f;
    p.piercing = true;
    p.ignoreWalls = true;
    return p;
}

Projectile Projectile::MakeEnemyArrow(Vector2 origin, float dirX, int damage) {
    Projectile p;
    p.kind = ProjectileKind::EnemyArrow;
    p.fromPlayer = false;
    p.pos = origin;
    p.vel = { dirX * 420.0f, 0.0f };
    p.radius = 8.0f;
    p.damage = damage;
    p.life = 3.0f;
    p.maxLife = 3.0f;
    p.ignoreWalls = false;
    p.destroyableBySword = true;
    return p;
}

Projectile Projectile::MakeBossFlameTrail(Vector2 pos, int damage, float duration) {
    Projectile p;
    p.kind = ProjectileKind::BossFlameTrail;
    p.fromPlayer = false;
    p.pos = pos;
    p.vel = { 0.0f, 0.0f };
    p.radius = 15.0f;
    p.damage = damage;
    p.life = duration;
    p.maxLife = duration;
    p.piercing = true;
    p.ignoreWalls = true;
    return p;
}

Projectile Projectile::MakeBossAcidDrop(Vector2 origin, Vector2 velocity, int damage) {
    Projectile p;
    p.kind = ProjectileKind::BossAcidDrop;
    p.fromPlayer = false;
    p.pos = origin;
    p.vel = velocity;
    p.radius = 9.0f;
    p.damage = damage;
    p.life = 2.2f;
    p.maxLife = 2.2f;
    p.ignoreWalls = false;
    return p;
}

Projectile Projectile::MakeBossSwordWave(Vector2 origin, Vector2 velocity, int damage) {
    Projectile p;
    p.kind = ProjectileKind::BossSwordWave;
    p.fromPlayer = false;
    p.pos = origin;
    p.vel = velocity;
    p.radius = 18.0f;
    p.damage = damage;
    p.life = 3.0f;
    p.maxLife = 3.0f;
    p.piercing = true;
    p.ignoreWalls = true;
    p.primaryColor = GetPalette().danger[2];
    return p;
}