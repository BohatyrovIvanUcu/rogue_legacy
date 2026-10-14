// === FILE: src/entities/Boss.cpp ===
#include "entities/Boss.hpp"
#include "entities/Player.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/UiText.hpp"
#include "gfx/Palette.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

SlimeUnit Boss::MakeSlimeUnit(int tier, int baseUnitHp, Vector2 center, Vector2 initialVel) {
    static uint32_t nextSlimeId = 50000;
    SlimeUnit u;
    u.id = nextSlimeId++;
    u.tier = std::clamp(tier, 1, 4);
    if (u.tier == 4)      u.scale = 2.40f;
    else if (u.tier == 3) u.scale = 1.70f;
    else if (u.tier == 2) u.scale = 1.20f;
    else                  u.scale = 0.75f;

    u.radius = 24.0f * u.scale;
    u.pos = center;
    u.vel = initialVel;
    u.maxHp = std::max(15, baseUnitHp * u.tier);
    u.hp = u.maxHp;
    u.jumpTimer = 0.6f + static_cast<float>(std::rand() % 80) / 100.0f;
    return u;
}

void Boss::Init(BossType bType, int bossLevel, Vector2 arenaCenter) {
    type = bType;
    level = std::max(1, bossLevel);
    phase = 1;
    active = true;
    stateTimer = 0.0f;
    subAttackTimer = 0.0f;
    currentPattern = 0;
    spiralAngle = 0.0f;
    isDashing = false;
    slimeUnits.clear();

    float lvlMult = 1.0f + (level - 1) * 0.14f;
    contactDamage = static_cast<int>(std::round(22.0f * lvlMult));

    switch (type) {
        case BossType::Khidr: {
            width = 112.0f;
            height = 112.0f;
            maxHp = static_cast<int>(std::round(420.0f * lvlMult));
            hp = maxHp;
            SetCenter({ Config::VIRTUAL_WIDTH * 0.5f, 300.0f });
            break;
        }
        case BossType::Alexander: {
            width = 96.0f;
            height = 96.0f;
            maxHp = static_cast<int>(std::round(540.0f * lvlMult));
            hp = maxHp;
            SetCenter(arenaCenter);
            break;
        }
        case BossType::PonceDeLeon: {
            width = 88.0f;
            height = 88.0f;
            maxHp = static_cast<int>(std::round(620.0f * lvlMult));
            hp = maxHp;
            SetCenter({ Config::VIRTUAL_WIDTH * 0.5f, 220.0f });
            dashCooldown = 5.0f;
            break;
        }
        case BossType::Herodotus: {
            width = 110.0f;
            height = 110.0f;
            int baseUnit = static_cast<int>(std::round(32.0f * lvlMult));
            // Сумарне HP усіх 15 потенційних поділів: 1*(4u) + 2*(3u) + 4*(2u) + 8*(1u) = 26 * baseUnit
            maxHp = 26 * baseUnit;
            hp = maxHp;
            slimeUnits.push_back(MakeSlimeUnit(4, baseUnit, arenaCenter, { 0.0f, -150.0f }));
            SetCenter(arenaCenter);
            break;
        }
        case BossType::JohannesTraitor: {
            width = 36.0f;
            height = 56.0f;
            maxHp = static_cast<int>(std::round(580.0f * lvlMult));
            hp = maxHp;
            SetCenter({ Config::VIRTUAL_WIDTH * 0.70f, 520.0f });
            facingDir = -1.0f;
            break;
        }
        default:
            break;
    }
}

std::string Boss::GetCurrentName() const {
    if (type == BossType::JohannesTraitor && phase == 2) {
        return "The Fountain";
    }
    return Registry::GetBossName(type);
}

std::string Boss::GetCurrentTitle() const {
    if (type == BossType::JohannesTraitor && phase == 2) {
        return "Colossus of Eternal Youth";
    }
    return Registry::GetBossTitle(type);
}

void Boss::Update(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    const Player& player,
    std::vector<Projectile>& outProjectiles,
    std::vector<Enemy>& outMinions,
    bool timeStopActive
) {
    if (!active) return;
    if (hurtFlashTimer > 0.0f) hurtFlashTimer -= dt;
    if (timeStopActive) return;

    animTimer += dt;

    switch (type) {
        case BossType::Khidr:
            UpdateKhidr(dt, player, outProjectiles);
            break;
        case BossType::Alexander:
            UpdateAlexander(dt, ctx, player, outProjectiles, outMinions);
            break;
        case BossType::PonceDeLeon:
            UpdatePonceDeLeon(dt, ctx, player, outProjectiles);
            break;
        case BossType::Herodotus:
            UpdateHerodotus(dt, tileMap, player, outProjectiles);
            break;
        case BossType::JohannesTraitor:
            UpdateJohannes(dt, ctx, tileMap, player, outProjectiles);
            break;
        default:
            break;
    }
}

void Boss::UpdateKhidr(float dt, const Player& player, std::vector<Projectile>& outProjectiles) {
    // Плавне зависання в центрі арени
    Vector2 home = { Config::VIRTUAL_WIDTH * 0.5f, 290.0f + std::sin(animTimer * 2.2f) * 16.0f };
    SetCenter(home);

    stateTimer += dt;
    subAttackTimer += dt;

    if (stateTimer >= 4.2f) {
        stateTimer = 0.0f;
        subAttackTimer = 0.0f;
        currentPattern = (currentPattern + 1) % 3;
    }

    Vector2 c = GetCenter();
    int projDmg = static_cast<int>(std::round(contactDamage * 0.85f));

    if (currentPattern == 0) {
        // (A) Подвійна спіраль із 36 куль (18 пар)
        if (subAttackTimer >= 0.16f) {
            subAttackTimer = 0.0f;
            spiralAngle += 0.36f;
            for (int arm = 0; arm < 2; ++arm) {
                float a = spiralAngle + arm * 3.14159265f;
                Vector2 v = { std::cos(a) * 255.0f, std::sin(a) * 255.0f };
                outProjectiles.push_back(Projectile::MakeEnemyEyeBolt(c, v, projDmg, RED));
            }
        }
    } else if (currentPattern == 1) {
        // (B) Концентричні кільця з 16 снарядів з безпечними проміжками
        if (subAttackTimer >= 0.95f) {
            subAttackTimer = 0.0f;
            int gapIndex = std::rand() % 16;
            for (int i = 0; i < 16; ++i) {
                if (i == gapIndex || i == (gapIndex + 1) % 16) continue;
                float a = i * (6.2831853f / 16.0f) + spiralAngle;
                Vector2 v = { std::cos(a) * 235.0f, std::sin(a) * 235.0f };
                outProjectiles.push_back(Projectile::MakeEnemyEyeBolt(c, v, projDmg, MAGENTA));
            }
            spiralAngle += 0.20f;
        }
    } else {
        // (C) Швидка прицільна черга з 6 великих сфер у поточну позицію гравця
        if (subAttackTimer >= 0.42f) {
            subAttackTimer = 0.0f;
            Vector2 pPos = player.GetCenter();
            Vector2 toP = { pPos.x - c.x, pPos.y - c.y };
            float d = std::max(1.0f, std::hypot(toP.x, toP.y));
            Projectile bolt = Projectile::MakeEnemyEyeBolt(
                c, { (toP.x / d) * 360.0f, (toP.y / d) * 360.0f }, projDmg, ORANGE
            );
            bolt.scale = 1.45f;
            outProjectiles.push_back(bolt);
        }
    }
}

void Boss::UpdateAlexander(
    float dt,
    GameContext& ctx,
    const Player& player,
    std::vector<Projectile>& outProjectiles,
    std::vector<Enemy>& outMinions
) {
    Vector2 pCenter = player.GetCenter();
    Vector2 myCenter = GetCenter();
    facingDir = (pCenter.x >= myCenter.x) ? 1.0f : -1.0f;

    stateTimer += dt;
    if (isDashing) {
        pos.x += dashVel.x * dt;
        pos.y += dashVel.y * dt;
        // Відбиття від меж арени
        if (pos.x < 96.0f || GetRight() > Config::VIRTUAL_WIDTH - 96.0f) dashVel.x = -dashVel.x;
        if (pos.y < 86.0f || GetBottom() > Config::VIRTUAL_HEIGHT - 120.0f) dashVel.y = -dashVel.y;
        if (stateTimer >= 1.15f) {
            isDashing = false;
            stateTimer = 0.0f;
        }
        return;
    }

    // Плавний політ за гравцем по синусоїді
    Vector2 toP = { pCenter.x - myCenter.x, (pCenter.y - 70.0f) - myCenter.y };
    float d = std::max(1.0f, std::hypot(toP.x, toP.y));
    vel.x = (toP.x / d) * 145.0f;
    vel.y = (toP.y / d) * 145.0f + std::sin(animTimer * 4.0f) * 65.0f;
    pos.x = std::clamp(pos.x + vel.x * dt, 96.0f, Config::VIRTUAL_WIDTH - 96.0f - width);
    pos.y = std::clamp(pos.y + vel.y * dt, 86.0f, Config::VIRTUAL_HEIGHT - 140.0f - height);

    if (stateTimer >= 3.2f) {
        stateTimer = 0.0f;
        currentPattern = (currentPattern + 1) % 3;

        if (currentPattern == 0) {
            // (A) Призов 2 черепів-міньйонів (Furies), якщо їх менше 4 в кімнаті
            if (outMinions.size() < 4) {
                for (int i = 0; i < 2; ++i) {
                    Enemy fury;
                    fury.Init(
                        EnemyType::FloatingEye, 2, level,
                        { myCenter.x + (i == 0 ? -65.0f : 65.0f), myCenter.y }
                    );
                    outMinions.push_back(fury);
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::BossRoar, 1.3f, 0.7f);
            }
        } else if (currentPattern == 1) {
            // (B) Таранний ривок через арену у гравця
            isDashing = true;
            Vector2 dir = { pCenter.x - myCenter.x, pCenter.y - myCenter.y };
            float len = std::max(1.0f, std::hypot(dir.x, dir.y));
            dashVel = { (dir.x / len) * 510.0f, (dir.y / len) * 510.0f };
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::BossRoar, 0.95f);
        } else {
            // (C) Віяловий залп фіолетового полум'я (7 куль)
            float baseAng = std::atan2(pCenter.y - myCenter.y, pCenter.x - myCenter.x);
            for (int i = -3; i <= 3; ++i) {
                float a = baseAng + i * 0.22f;
                outProjectiles.push_back(Projectile::MakeEnemyEyeBolt(
                    myCenter,
                    { std::cos(a) * 310.0f, std::sin(a) * 310.0f },
                    contactDamage, PURPLE
                ));
            }
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 0.7f);
        }
    }
}

void Boss::UpdatePonceDeLeon(
    float dt,
    GameContext& ctx,
    const Player& player,
    std::vector<Projectile>& outProjectiles
) {
    Vector2 pCenter = player.GetCenter();
    Vector2 myCenter = GetCenter();

    // Кожні 0.28s залишає позаду себе нерухому вогняну кулю на 4.5 секунди
    subAttackTimer += dt;
    if (subAttackTimer >= 0.28f) {
        subAttackTimer = 0.0f;
        outProjectiles.push_back(Projectile::MakeBossFlameTrail(myCenter, static_cast<int>(std::round(contactDamage * 0.8f)), 4.5f));
    }

    dashCooldown -= dt;
    if (dashCooldown <= 0.5f && dashCooldown > 0.0f) {
        // Зупиняється на 0.5s перед стрімким ривком
        vel = { 0.0f, 0.0f };
        Vector2 dir = { pCenter.x - myCenter.x, pCenter.y - myCenter.y };
        float len = std::max(1.0f, std::hypot(dir.x, dir.y));
        dashVel = { (dir.x / len) * 680.0f, (dir.y / len) * 680.0f };
        return;
    }

    if (dashCooldown <= 0.0f) {
        if (!isDashing) {
            isDashing = true;
            stateTimer = 0.55f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::Dash, 0.75f);
        }
        stateTimer -= dt;
        pos.x += dashVel.x * dt;
        pos.y += dashVel.y * dt;
        if (stateTimer <= 0.0f) {
            isDashing = false;
            dashCooldown = 5.0f;
        }
    } else {
        // Безперервний політ у напрямку гравця зі швидкістю 210 px/s
        Vector2 toP = { pCenter.x - myCenter.x, pCenter.y - myCenter.y };
        float d = std::max(1.0f, std::hypot(toP.x, toP.y));
        vel.x = (toP.x / d) * 210.0f;
        vel.y = (toP.y / d) * 210.0f;
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
    }

    pos.x = std::clamp(pos.x, 88.0f, Config::VIRTUAL_WIDTH - 88.0f - width);
    pos.y = std::clamp(pos.y, 64.0f, Config::VIRTUAL_HEIGHT - 88.0f - height);
}

void Boss::UpdateHerodotus(
    float dt,
    const ITileMap* tileMap,
    const Player& player,
    std::vector<Projectile>& outProjectiles
) {
    Vector2 pCenter = player.GetCenter();
    int totalRemainingHp = 0;

    for (auto& u : slimeUnits) {
        if (u.hurtFlashTimer > 0.0f) u.hurtFlashTimer -= dt;
        totalRemainingHp += std::max(0, u.hp);

        u.vel.y += Config::GRAVITY * 0.88f * dt;
        if (u.vel.y > Config::MAX_FALL_SPEED) u.vel.y = Config::MAX_FALL_SPEED;

        if (u.onGround) {
            u.vel.x *= std::pow(0.02f, dt);
            u.jumpTimer -= dt;
            if (u.jumpTimer <= 0.0f) {
                u.jumpTimer = 0.85f + (u.tier * 0.22f) + static_cast<float>(std::rand() % 40) / 100.0f;
                float dirX = (pCenter.x >= u.pos.x) ? 1.0f : -1.0f;
                u.vel.x = dirX * (180.0f + (5 - u.tier) * 45.0f);
                u.vel.y = -(540.0f + u.tier * 45.0f);
                u.onGround = false;
            }
        }

        // Рух по X та відбиття від стін
        u.pos.x += u.vel.x * dt;
        float minX = static_cast<float>(Config::ROOM_OFFSET_X + Config::TILE_SIZE) + u.radius;
        float maxX = static_cast<float>(Config::ROOM_OFFSET_X + (Config::ROOM_COLS - 1) * Config::TILE_SIZE) - u.radius;
        if (u.pos.x < minX) {
            u.pos.x = minX;
            u.vel.x = -u.vel.x;
        } else if (u.pos.x > maxX) {
            u.pos.x = maxX;
            u.vel.x = -u.vel.x;
        }

        // Рух по Y та приземлення з розбризкуванням кислоти
        bool wasOnGround = u.onGround;
        u.onGround = false;
        u.pos.y += u.vel.y * dt;

        float floorY = static_cast<float>(Config::ROOM_OFFSET_Y + (Config::ROOM_ROWS - 1) * Config::TILE_SIZE);
        if (tileMap && u.vel.y >= 0.0f) {
            int col = tileMap->WorldToCol(u.pos.x);
            int row = tileMap->WorldToRow(u.pos.y + u.radius);
            if (tileMap->IsSolidAt(col, row, false)) {
                floorY = std::min(floorY, tileMap->GetTileRect(col, row).y);
            }
        }

        if (u.pos.y + u.radius >= floorY) {
            u.pos.y = floorY - u.radius;
            if (!wasOnGround && u.vel.y > 240.0f) {
                // Бризки крапель кислоти при приземленні
                int drops = (u.tier >= 3) ? 4 : 2;
                for (int i = 0; i < drops; ++i) {
                    float vx = (i % 2 == 0 ? 1.0f : -1.0f) * (120.0f + (i / 2) * 95.0f);
                    float vy = -310.0f - (i % 2) * 70.0f;
                    outProjectiles.push_back(Projectile::MakeBossAcidDrop(
                        { u.pos.x, u.pos.y },
                        { vx, vy },
                        static_cast<int>(std::round(contactDamage * 0.75f))
                    ));
                }
            }
            u.vel.y = 0.0f;
            u.onGround = true;
        }
    }

    if (!slimeUnits.empty()) {
        SetCenter(slimeUnits.front().pos);
    }
    hp = totalRemainingHp;
    if (slimeUnits.empty()) {
        hp = 0;
        active = false;
    }
}

void Boss::UpdateJohannes(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    const Player& player,
    std::vector<Projectile>& outProjectiles
) {
    Vector2 pCenter = player.GetCenter();
    Vector2 myCenter = GetCenter();
    facingDir = (pCenter.x >= myCenter.x) ? 1.0f : -1.0f;
    stateTimer += dt;

    if (phase == 1) {
        // ФАЗА 1: Johannes the Traitor (агресивний темний лицар)
        float runSpeed = 240.0f;
        float distX = std::abs(pCenter.x - myCenter.x);
        if (distX > 65.0f) {
            vel.x = facingDir * runSpeed;
        } else {
            vel.x = -facingDir * 90.0f;
        }

        if (onGround && (distX < 130.0f || (std::rand() % 120 == 0))) {
            vel.y = -610.0f;
            onGround = false;
        }

        vel.y += Config::GRAVITY * dt;
        pos.x = std::clamp(pos.x + vel.x * dt, 96.0f, Config::VIRTUAL_WIDTH - 96.0f - width);
        pos.y += vel.y * dt;

        float floorY = static_cast<float>((Config::ROOM_ROWS - 1) * Config::TILE_SIZE);
        if (tileMap && vel.y >= 0.0f) {
            int c = tileMap->WorldToCol(myCenter.x);
            int r = tileMap->WorldToRow(GetBottom());
            if (tileMap->IsSolidAt(c, r, false) || tileMap->IsOneWayAt(c, r)) {
                floorY = tileMap->GetTileRect(c, r).y;
            }
        }
        if (GetBottom() >= floorY) {
            pos.y = floorY - height;
            vel.y = 0.0f;
            onGround = true;
        }

        if (stateTimer >= 1.35f) {
            stateTimer = 0.0f;
            currentPattern = (currentPattern + 1) % 3;
            int spellDmg = contactDamage;

            if (currentPattern == 0) {
                // Кидок 2 кинджалів та 1 сокири
                outProjectiles.push_back(Projectile::MakeEnemyArrow(myCenter, facingDir, spellDmg));
                outProjectiles.push_back(Projectile::MakeEnemyBone(myCenter, { facingDir * 310.0f, -580.0f }, spellDmg));
            } else if (currentPattern == 1) {
                // Вогняні стовпи з-під землі
                for (int i = -1; i <= 1; ++i) {
                    outProjectiles.push_back(Projectile::MakeEnemyFirePillar(
                        { pCenter.x + i * 64.0f, static_cast<float>((Config::ROOM_ROWS - 1) * Config::TILE_SIZE) },
                        spellDmg
                    ));
                }
            } else {
                // Мечі з небес
                for (int i = -2; i <= 2; ++i) {
                    outProjectiles.push_back(Projectile::MakeBossSwordWave(
                        { pCenter.x + i * 85.0f, 60.0f },
                        { 0.0f, 520.0f },
                        spellDmg
                    ));
                }
            }
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 0.9f);
        }
    } else {
        // ФАЗА 2: The Fountain (Проклятий Колос у мантії)
        Vector2 targetPos = {
            Config::VIRTUAL_WIDTH * 0.5f + std::sin(animTimer * 1.6f) * 320.0f,
            240.0f + std::cos(animTimer * 2.4f) * 55.0f
        };
        SetCenter(targetPos);

        if (stateTimer >= 1.75f) {
            stateTimer = 0.0f;
            currentPattern = (currentPattern + 1) % 2;

            if (currentPattern == 0) {
                // Хвиля мечів з небес по всій ширині кімнати з безпечними проміжками
                int safeSlot = 2 + (std::rand() % 7);
                for (int i = 1; i <= 10; ++i) {
                    if (i == safeSlot || i == safeSlot + 1) continue;
                    float x = 110.0f + i * 102.0f;
                    outProjectiles.push_back(Projectile::MakeBossSwordWave({ x, 55.0f }, { 0.0f, 470.0f }, contactDamage));
                }
            } else {
                // Горизонтальні ударні хвилі мечів по землі в обидва боки + вогняне кільце
                Vector2 c = GetCenter();
                for (int i = 0; i < 12; ++i) {
                    float a = i * (6.2831853f / 12.0f);
                    outProjectiles.push_back(Projectile::MakeEnemyEyeBolt(
                        c, { std::cos(a) * 290.0f, std::sin(a) * 290.0f }, contactDamage, GOLD
                    ));
                }
                float floorY = static_cast<float>((Config::ROOM_ROWS - 1) * Config::TILE_SIZE - 36.0f);
                outProjectiles.push_back(Projectile::MakeBossSwordWave({ c.x, floorY }, { -460.0f, 0.0f }, contactDamage));
                outProjectiles.push_back(Projectile::MakeBossSwordWave({ c.x, floorY }, {  460.0f, 0.0f }, contactDamage));
            }
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::BossRoar, 0.85f);
        }
    }
}

bool Boss::CheckAndApplyPlayerAttack(
    const Rectangle& attackBox,
    int damage,
    bool isCrit,
    uint32_t swingOrProjId,
    std::vector<uint32_t>& hitRegistry,
    GameContext& ctx,
    std::vector<Projectile>& /*outProjectiles*/,
    Vector2& outHitCenter
) {
    if (!active || hp <= 0) return false;

    // Спеціальна логіка поділу для Боса 4 (Herodotus)
    if (type == BossType::Herodotus) {
        bool anyHit = false;
        std::vector<SlimeUnit> newlySplit;

        for (auto it = slimeUnits.begin(); it != slimeUnits.end(); ) {
            Rectangle uBox = {
                it->pos.x - it->radius,
                it->pos.y - it->radius,
                it->radius * 2.0f,
                it->radius * 2.0f
            };

            bool alreadyHit = (std::find(hitRegistry.begin(), hitRegistry.end(), it->id) != hitRegistry.end());
            if (!alreadyHit && CheckCollisionRecs(attackBox, uBox)) {
                hitRegistry.push_back(it->id);
                anyHit = true;
                outHitCenter = it->pos;
                it->hp -= std::max(1, damage);
                it->hurtFlashTimer = 0.14f;

                if (ctx.particles) {
                    ctx.particles->SpawnSparks(it->pos, LIME, 10);
                    ctx.particles->SpawnFloatingText(
                        { it->pos.x, it->pos.y - it->radius },
                        std::to_string(damage) + (isCrit ? "!" : ""),
                        isCrit ? GOLD : WHITE,
                        isCrit ? 24 : 20,
                        isCrit
                    );
                }

                if (it->hp <= 0) {
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitEnemy, 0.85f);
                    if (ctx.particles) ctx.particles->SpawnExplosion(it->pos, it->radius, LIME);

                    if (it->tier > 1) {
                        int childTier = it->tier - 1;
                        int baseUnitHp = std::max(15, maxHp / 26);
                        newlySplit.push_back(MakeSlimeUnit(childTier, baseUnitHp, { it->pos.x - 18.0f, it->pos.y }, { -190.0f, -380.0f }));
                        newlySplit.push_back(MakeSlimeUnit(childTier, baseUnitHp, { it->pos.x + 18.0f, it->pos.y }, {  190.0f, -380.0f }));
                    }
                    it = slimeUnits.erase(it);
                    continue;
                } else {
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitEnemy, 1.05f);
                }
            }
            ++it;
        }

        for (const auto& child : newlySplit) {
            slimeUnits.push_back(child);
        }

        int sumHp = 0;
        for (const auto& u : slimeUnits) sumHp += std::max(0, u.hp);
        hp = sumHp;
        if (slimeUnits.empty()) {
            hp = 0;
            active = false;
        }
        return anyHit;
    }

    // Стандартні боси (Khidr, Alexander, PonceDeLeon, Johannes)
    bool alreadyHit = (std::find(hitRegistry.begin(), hitRegistry.end(), id) != hitRegistry.end());
    if (alreadyHit || !CheckCollisionRecs(attackBox, GetBounds())) {
        return false;
    }

    hitRegistry.push_back(id);
    outHitCenter = GetCenter();
    hp -= std::max(1, damage);
    hurtFlashTimer = 0.14f;

    if (ctx.particles) {
        ctx.particles->SpawnBlood(outHitCenter, 12);
        ctx.particles->SpawnFloatingText(
            { outHitCenter.x, pos.y - 12.0f },
            std::to_string(damage) + (isCrit ? "!" : ""),
            isCrit ? GOLD : WHITE,
            isCrit ? 26 : 21,
            isCrit
        );
    }

    if (hp <= 0) {
        if (type == BossType::JohannesTraitor && phase == 1) {
            // Перехід Johannes у Фазу 2 (The Fountain — Проклятий Колос)
            phase = 2;
            width = 108.0f;
            height = 148.0f;
            maxHp = static_cast<int>(std::round(maxHp * 1.35f));
            hp = maxHp;
            stateTimer = 0.0f;
            SetCenter({ Config::VIRTUAL_WIDTH * 0.5f, 260.0f });
            if (ctx.audio)     ctx.audio->PlaySfx(SfxId::BossRoar, 0.72f);
            if (ctx.particles) ctx.particles->SpawnExplosion(GetCenter(), 180.0f, PURPLE);
        } else {
            hp = 0;
            active = false;
            if (ctx.audio)     ctx.audio->PlaySfx(SfxId::BossRoar, 0.65f);
            if (ctx.particles) ctx.particles->SpawnExplosion(GetCenter(), 200.0f, GOLD);
        }
    } else {
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitEnemy, 0.95f);
    }

    (void)swingOrProjId;
    return true;
}

bool Boss::CheckPlayerContact(const Rectangle& playerBounds, Vector2& outContactCenter) const {
    if (!active || hp <= 0) return false;

    if (type == BossType::Herodotus) {
        for (const auto& u : slimeUnits) {
            Rectangle uBox = {
                u.pos.x - u.radius * 0.85f,
                u.pos.y - u.radius * 0.85f,
                u.radius * 1.70f,
                u.radius * 1.70f
            };
            if (CheckCollisionRecs(playerBounds, uBox)) {
                outContactCenter = u.pos;
                return true;
            }
        }
        return false;
    }

    if (CheckCollisionRecs(playerBounds, GetBounds())) {
        outContactCenter = GetCenter();
        return true;
    }
    return false;
}

void Boss::Draw() const {
    if (!active) return;

    const auto& pal = GetPalette();
    Vector2 c = GetCenter();
    bool flash = (hurtFlashTimer > 0.0f);
    bool isEnraged = (hp <= maxHp / 2);

    switch (type) {
        case BossType::Khidr: {
            // Гігантське демонічне око Khidr з щупальцями (Масштаб x2.5)
            // Ореол боса: у 2-й фазі змінюється на лютий фіолетово-магічний
            float haloR = isEnraged ? (105.0f + std::sin(animTimer * 10.0f) * 8.0f) : 85.0f;
            Color haloCol = isEnraged ? pal.moonMagic[3] : pal.bloodCloth[2];
            DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y), haloR, Fade(haloCol, isEnraged ? 0.45f : 0.25f), Fade(pal.outline, 0.0f));

            // 1. Звивисті щупальця з шипами
            for (int i = 0; i < 10; ++i) {
                float a = i * (6.2831853f / 10.0f) + animTimer * 1.5f;
                float wave = std::sin(animTimer * 5.0f + i) * 12.0f;
                Vector2 mid = { c.x + std::cos(a) * 55.0f + std::sin(a) * wave, c.y + std::sin(a) * 55.0f - std::cos(a) * wave };
                Vector2 tip = { c.x + std::cos(a) * 88.0f, c.y + std::sin(a) * 88.0f };
                Color tentCol = flash ? pal.metal[4] : ((i % 2 == 0) ? pal.bloodCloth[2] : pal.bloodCloth[1]);
                DrawLineEx(c, mid, 9.0f, tentCol);
                DrawLineEx(mid, tip, 5.0f, tentCol);
                Color tipCol = isEnraged ? pal.fireGold[3] : (flash ? pal.danger[2] : pal.bloodCloth[3]);
                DrawCircleV(tip, isEnraged ? 5.5f : 4.0f, tipCol); // отруйний шип
            }

            // 2. Очне яблуко з кровоносними судинами
            Color eyeSclera = flash ? pal.danger[2] : pal.bone[4];
            DrawCircleV(c, 58.0f, eyeSclera);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), 58.0f, pal.bloodCloth[1]);
            for (int v = 0; v < 8; ++v) {
                float va = v * 0.785f + 0.2f;
                Vector2 vOut = { c.x + std::cos(va) * 56.0f, c.y + std::sin(va) * 56.0f };
                Vector2 vIn  = { c.x + std::cos(va) * 32.0f, c.y + std::sin(va) * 32.0f };
                DrawLineEx(vOut, vIn, 2.0f, Fade(pal.bloodCloth[2], 0.65f));
            }

            // 3. Багатошарова кривава райдужка
            float irisR = 30.0f + std::sin(animTimer * 4.0f) * 2.0f;
            DrawCircleV(c, irisR, pal.bloodCloth[3]);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y), irisR, pal.bloodCloth[1]);
            DrawCircleV(c, irisR * 0.65f, pal.fireGold[2]);

            // 4. Демонічна щілинна зіниця
            DrawEllipse(static_cast<int>(c.x), static_cast<int>(c.y), 8.0f, irisR * 0.75f, pal.outline);
            // Блики світла
            DrawCircleV({ c.x - 10.0f, c.y - 12.0f }, 7.0f, pal.metal[4]);
            DrawCircleV({ c.x - 4.0f, c.y - 6.0f }, 3.5f, pal.metal[4]);
            break;
        }

        case BossType::Alexander: {
            // Гігантський рогатий череп Alexander (Масштаб x2.2)
            Color alexHalo = isEnraged ? pal.bloodCloth[3] : pal.moonMagic[1];
            float alexHaloR = isEnraged ? (92.0f + std::sin(animTimer * 12.0f) * 6.0f) : 75.0f;
            DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y), alexHaloR, Fade(alexHalo, isEnraged ? 0.55f : 0.45f), Fade(pal.outline, 0.0f));

            // 1. Величні закручені роги
            for (int side = -1; side <= 1; side += 2) {
                float s = static_cast<float>(side);
                Vector2 hBase = { c.x + s * 28.0f, c.y - 24.0f };
                Vector2 hMid  = { c.x + s * 72.0f, c.y - 65.0f };
                Vector2 hTip  = { c.x + s * 95.0f, c.y - 35.0f };
                DrawLineEx(hBase, hMid, 14.0f, pal.metal[2]);
                DrawLineEx(hMid, hTip, 8.0f, pal.metal[2]);
                // Золоті кільця на рогах
                DrawCircleV(hMid, 7.5f, pal.fireGold[3]);
                DrawCircleV(hTip, 4.0f, pal.fireGold[3]);
            }

            // 2. Масивна кістка черепа
            Color skullCol = flash ? pal.danger[2] : pal.bone[4];
            Color boneShade = ColorTint(skullCol, pal.bone[2]);
            DrawCircleV({ c.x, c.y - 8.0f }, 46.0f, skullCol);
            DrawCircleLines(static_cast<int>(c.x), static_cast<int>(c.y - 8.0f), 46.0f, boneShade);

            // Тріщини на черепі
            DrawLineEx({ c.x - 12.0f, c.y - 42.0f }, { c.x - 4.0f, c.y - 28.0f }, 2.0f, Fade(pal.outline, 0.65f));
            DrawLineEx({ c.x - 4.0f, c.y - 28.0f }, { c.x + 8.0f, c.y - 20.0f }, 1.5f, Fade(pal.outline, 0.65f));

            // Верхня щелепа
            DrawRectangleRounded({ c.x - 26.0f, c.y + 14.0f, 52.0f, 26.0f }, 0.25f, 4, skullCol);
            DrawRectangleRoundedLines({ c.x - 26.0f, c.y + 14.0f, 52.0f, 26.0f }, 0.25f, 4, 2.0f, boneShade);
            // Зуби
            for (int t = -20; t <= 20; t += 10) {
                DrawRectangle(static_cast<int>(c.x + t - 2), static_cast<int>(c.y + 24.0f), 5, 12, pal.bone[4]);
                DrawRectangleLines(static_cast<int>(c.x + t - 2), static_cast<int>(c.y + 24.0f), 5, 12, pal.outline);
            }

            // Носова западина
            DrawTriangle({ c.x, c.y + 6.0f }, { c.x - 4.5f, c.y + 16.0f }, { c.x + 4.5f, c.y + 16.0f }, pal.outline);

            // 3. Величезні чорні очниці з магічним фіолетовим вогнем
            for (int side = -1; side <= 1; side += 2) {
                float s = static_cast<float>(side);
                Vector2 eyePos = { c.x + s * 17.0f, c.y - 6.0f };
                DrawCircleV(eyePos, 13.0f, pal.outline);
                // Духовне полум'я в очах
                float fSize = 6.0f + std::sin(animTimer * 12.0f + s) * 2.0f;
                DrawCircleV(eyePos, fSize, pal.moonMagic[3]);
                DrawCircleV(eyePos, fSize * 0.5f, pal.metal[4]);
            }
            break;
        }

        case BossType::PonceDeLeon: {
            // Вогняний титан Ponce De Leon
            float pulse = 1.0f + 0.12f * std::sin(animTimer * 16.0f);

            // 1. Зовнішня сонячна корона з променями
            for (int f = 0; f < 12; ++f) {
                float fa = f * (6.2831853f / 12.0f) + animTimer * 2.2f;
                float fLen = 65.0f + std::sin(animTimer * 20.0f + f) * 16.0f;
                Vector2 fTip = { c.x + std::cos(fa) * fLen, c.y + std::sin(fa) * fLen };
                DrawLineEx(c, fTip, 7.0f, Fade(pal.danger[2], 0.55f));
                DrawCircleV(fTip, 4.0f, pal.fireGold[2]);
            }

            // 2. Шари палаючої плазми
            DrawCircleV(c, 56.0f * pulse, Fade(pal.danger[2], 0.45f));
            DrawCircleV(c, 44.0f * pulse, flash ? pal.metal[4] : pal.fireGold[2]);
            DrawCircleV(c, 30.0f * pulse, pal.fireGold[3]);
            DrawCircleV(c, 16.0f * pulse, pal.fireGold[4]);

            // 3. Зловісне кам'яно-лавове обличчя демона
            DrawCircleV({ c.x - 14.0f, c.y - 6.0f }, 7.5f, pal.bloodCloth[1]);
            DrawCircleV({ c.x + 14.0f, c.y - 6.0f }, 7.5f, pal.bloodCloth[1]);
            DrawCircleV({ c.x - 14.0f, c.y - 6.0f }, 3.5f, pal.metal[4]); // вогняні зіниці
            DrawCircleV({ c.x + 14.0f, c.y - 6.0f }, 3.5f, pal.metal[4]);
            // Палаюча паща
            DrawTriangle({ c.x - 12.0f, c.y + 14.0f }, { c.x + 12.0f, c.y + 14.0f }, { c.x, c.y + 24.0f }, pal.bloodCloth[1]);
            break;
        }

        case BossType::Herodotus: {
            // Велетенський давній слайм Herodotus
            for (const auto& u : slimeUnits) {
                Color sBase = (u.hurtFlashTimer > 0.0f) ? pal.metal[4] : pal.poisonSlime[3];
                Color sRim  = pal.poisonSlime[1];
                float squash = u.onGround ? 1.15f : 0.90f;
                float rx = u.radius * squash;
                float ry = u.radius / squash;

                // Зовнішня прозора желатинова оболонка
                DrawEllipse(static_cast<int>(u.pos.x), static_cast<int>(u.pos.y), rx, ry, sBase);
                DrawEllipseLines(static_cast<int>(u.pos.x), static_cast<int>(u.pos.y), rx, ry, sRim);

                // Внутрішнє ядро слайма
                DrawCircleV({ u.pos.x, u.pos.y }, u.radius * 0.38f, pal.poisonSlime[2]);
                DrawCircleV({ u.pos.x - rx * 0.3f, u.pos.y - ry * 0.3f }, u.radius * 0.22f, Fade(pal.metal[4], 0.45f)); // блік

                // Виразні очі
                float eyeOffX = u.radius * 0.28f;
                float eyeOffY = u.radius * 0.16f;
                float eyeR    = std::max(4.0f, 4.5f * u.scale);
                // Білки очей
                DrawCircleV({ u.pos.x - eyeOffX, u.pos.y - eyeOffY }, eyeR, pal.bone[4]);
                DrawCircleV({ u.pos.x + eyeOffX, u.pos.y - eyeOffY }, eyeR, pal.bone[4]);
                // Зіниці
                DrawCircleV({ u.pos.x - eyeOffX + 1.0f, u.pos.y - eyeOffY }, eyeR * 0.55f, pal.outline);
                DrawCircleV({ u.pos.x + eyeOffX + 1.0f, u.pos.y - eyeOffY }, eyeR * 0.55f, pal.outline);
                DrawCircleV({ u.pos.x - eyeOffX + 0.5f, u.pos.y - eyeOffY - 1.0f }, eyeR * 0.25f, pal.metal[4]);
                DrawCircleV({ u.pos.x + eyeOffX + 0.5f, u.pos.y - eyeOffY - 1.0f }, eyeR * 0.25f, pal.metal[4]);
            }
            break;
        }

        case BossType::JohannesTraitor: {
            if (phase == 1) {
                // Фаза 1: Темний лицар Johannes (The Traitor)
                Color armor     = flash ? pal.danger[2] : pal.metal[1];
                Color goldTrim  = pal.fireGold[3];

                // Розвіваючий темний плащ
                float cWave = std::sin(animTimer * 10.0f) * 6.0f;
                DrawTriangle({ c.x, pos.y + 14.0f }, { c.x - facingDir * 32.0f, GetBottom() + cWave }, { c.x - facingDir * 12.0f, GetBottom() }, pal.bloodCloth[1]);

                // Важкі латні ноги
                DrawRectangleRec({ pos.x + 8.0f, GetBottom() - 18.0f, 10.0f, 18.0f }, armor);
                DrawRectangleRec({ GetRight() - 18.0f, GetBottom() - 18.0f, 10.0f, 18.0f }, armor);

                // Чорна кіраса з золотим кантом
                Rectangle chestRec = { pos.x + 6.0f, pos.y + 18.0f, width - 12.0f, height - 34.0f };
                DrawRectangleRounded(chestRec, 0.25f, 4, armor);
                DrawRectangleRoundedLines(chestRec, 0.25f, 4, 2.5f, goldTrim);

                // Наплічники зі шпичаками
                DrawCircle(static_cast<int>(c.x - facingDir * 12.0f), static_cast<int>(pos.y + 20.0f), 8.0f, armor);
                DrawCircle(static_cast<int>(c.x + facingDir * 12.0f), static_cast<int>(pos.y + 20.0f), 8.0f, armor);
                DrawCircleLines(static_cast<int>(c.x - facingDir * 12.0f), static_cast<int>(pos.y + 20.0f), 8.0f, goldTrim);
                DrawCircleLines(static_cast<int>(c.x + facingDir * 12.0f), static_cast<int>(pos.y + 20.0f), 8.0f, goldTrim);

                // Рогатий шолом темного лицаря
                Vector2 hC = { c.x, pos.y + 12.0f };
                DrawCircleV(hC, 12.5f, armor);
                DrawCircleLines(static_cast<int>(hC.x), static_cast<int>(hC.y), 12.5f, goldTrim);
                // Роги на шоломі
                DrawTriangle({ hC.x - 8.0f, hC.y - 4.0f }, { hC.x - 18.0f, hC.y - 18.0f }, { hC.x - 4.0f, hC.y - 10.0f }, goldTrim);
                DrawTriangle({ hC.x + 8.0f, hC.y - 4.0f }, { hC.x + 4.0f, hC.y - 10.0f }, { hC.x + 18.0f, hC.y - 18.0f }, goldTrim);
                // Очна щілина з кривавим вогнем
                DrawRectangle(static_cast<int>(hC.x + facingDir * 3.0f - 5.0f), static_cast<int>(hC.y - 2.0f), 10, 4, pal.outline);
                DrawCircleV({ hC.x + facingDir * 4.5f, hC.y }, 2.5f, pal.bloodCloth[3]);
                DrawCircleV({ hC.x + facingDir * 4.5f, hC.y }, 1.2f, pal.metal[4]);

                // Подвійні рунічні клинки (Blood + Moon)
                Vector2 b1Hilt = { c.x + facingDir * 8.0f, c.y - 2.0f };
                Vector2 b1Tip  = { c.x + facingDir * 44.0f, c.y - 18.0f };
                DrawLineEx(b1Hilt, b1Tip, 5.5f, pal.bloodCloth[3]);
                DrawLineEx(b1Hilt, b1Tip, 2.5f, pal.metal[4]);

                Vector2 b2Hilt = { c.x - facingDir * 8.0f, c.y + 8.0f };
                Vector2 b2Tip  = { c.x - facingDir * 38.0f, c.y + 22.0f };
                DrawLineEx(b2Hilt, b2Tip, 5.5f, pal.moonMagic[3]);
                DrawLineEx(b2Hilt, b2Tip, 2.5f, pal.metal[4]);
            } else {
                // Фаза 2: The Fountain — Проклятий Колос
                DrawCircleGradient(static_cast<int>(c.x), static_cast<int>(c.y), 92.0f, Fade(pal.fireGold[3], 0.35f), Fade(pal.outline, 0.0f));
                Color robe = flash ? pal.danger[2] : pal.dungeon.stoneTile[2];

                DrawTriangle(
                    { GetRight() + 22.0f, GetBottom() },
                    { c.x, pos.y },
                    { pos.x - 22.0f, GetBottom() },
                    robe
                );
                DrawRectangleRoundedLines(GetBounds(), 0.3f, 4, 3.5f, pal.fireGold[3]);

                // Золота корона та очі божества
                Vector2 headC = { c.x, pos.y + 26.0f };
                DrawCircleV(headC, 24.0f, pal.fireGold[3]);
                DrawCircleV(headC, 20.0f, pal.dungeon.stoneBg[1]);
                // Сяючі небесні очі
                DrawCircleV({ headC.x - 8.0f, headC.y - 2.0f }, 5.5f, pal.moonMagic[3]);
                DrawCircleV({ headC.x + 8.0f, headC.y - 2.0f }, 5.5f, pal.moonMagic[3]);
                DrawCircleV({ headC.x - 8.0f, headC.y - 2.0f }, 2.5f, pal.metal[4]);
                DrawCircleV({ headC.x + 8.0f, headC.y - 2.0f }, 2.5f, pal.metal[4]);
            }
            break;
        }

        default:
            break;
    }
}

void Boss::DrawHudHealthBar() const {
    if (!active || hp <= 0) return;

    int barW = 680;
    int barH = 22;
    int barX = (Config::VIRTUAL_WIDTH - barW) / 2;
    int barY = Config::VIRTUAL_HEIGHT - 54;

    std::string header = GetCurrentName() + "  -  " + GetCurrentTitle();
    int tw = Ui::Measure(header, 18);
    const auto& pal = GetPalette();
    Ui::Text(header, (Config::VIRTUAL_WIDTH - tw) / 2 + 1, barY - 24 + 1, 18, pal.outline);
    Ui::Text(header, (Config::VIRTUAL_WIDTH - tw) / 2, barY - 24, 18, pal.fireGold[3]);

    DrawRectangle(barX - 4, barY - 4, barW + 8, barH + 8, pal.outline);
    DrawRectangleLinesEx({ static_cast<float>(barX - 4), static_cast<float>(barY - 4), static_cast<float>(barW + 8), static_cast<float>(barH + 8) }, 2.5f, pal.fireGold[3]);

    float ratio = std::clamp(static_cast<float>(hp) / static_cast<float>(std::max(1, maxHp)), 0.0f, 1.0f);
    Color fillCol = (type == BossType::JohannesTraitor && phase == 2) ? pal.moonMagic[1] : pal.bloodCloth[3];
    DrawRectangle(barX, barY, static_cast<int>(barW * ratio), barH, fillCol);
    DrawRectangle(barX, barY, static_cast<int>(barW * ratio), barH / 3, Fade(pal.bloodCloth[4], 0.35f));

    std::string hpText = std::to_string(hp) + " / " + std::to_string(maxHp);
    int htw = Ui::Measure(hpText, 14);
    Ui::Text(hpText, (Config::VIRTUAL_WIDTH - htw) / 2, barY + 3, 14, RAYWHITE);
}