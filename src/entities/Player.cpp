// === FILE: src/entities/Player.cpp ===
#include "entities/Player.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

void Player::InitFromContext(GameContext& ctx, Vector2 spawnCenter) {
    stats = ctx.ComputePlayerStats();

    // Обмежуємо фізичний колізійний AABB, щоб гіганти (Gigantism) вільно проходили крізь прорізи в 1 тайл (48px)
    width  = std::min(34.0f, Config::PLAYER_BASE_WIDTH * stats.scaleX);
    height = std::min(58.0f, Config::PLAYER_BASE_HEIGHT * stats.scaleY);
    SetCenter(spawnCenter);
    vel = { 0.0f, 0.0f };

    maxHp = stats.maxHp;
    hp    = stats.maxHp;
    mp    = stats.maxMp;
    active = true;
    onGround = false;
    facingDir = 1.0f;

    coyoteTimer = 0.0f;
    jumpBufferTimer = 0.0f;
    airJumpsLeft = stats.extraAirJumps;
    airDashesLeft = 1 + stats.maxAirDashes;
    isDashing = false;
    dashTimer = 0.0f;
    dashCooldownTimer = 0.0f;
    dropThroughPlatformTimer = 0.0f;

    isAttacking = false;
    isDownStriking = false;
    attackTimer = 0.0f;
    attackCooldownTimer = 0.0f;
    currentSwingHitIds.clear();

    activeSpell = ctx.activeHeir.spell;
    archmageSpells[0] = ctx.activeHeir.spell;
    archmageSpells[1] = static_cast<SpellType>((static_cast<int>(ctx.activeHeir.spell) + 2) % static_cast<int>(SpellType::COUNT));
    archmageSpells[2] = static_cast<SpellType>((static_cast<int>(ctx.activeHeir.spell) + 5) % static_cast<int>(SpellType::COUNT));
    archmageSpellIndex = 0;

    spellCooldownTimer = 0.0f;
    talentCooldownTimer = 0.0f;
    timeStopTimer = 0.0f;

    isBlocking = false;
    isMistForm = false;
    mistManaAccum = 0.0f;
    headlampOn = (ctx.activeHeir.classType == ClassType::Miner_Base ||
                  ctx.activeHeir.classType == ClassType::Spelunker_Upgraded);

    pendingShout_ = ShoutPulseEvent{};
    shoutVisualTimer = 0.0f;

    iframeTimer = 0.0f;
    hitstopTimer = 0.0f;
    hurtFlashTimer = 0.0f;
    ibsTimer = 0.0f;
    animTime = 0.0f;

    ResetRoomTrialFlags();
}

void Player::RecalculateStats(GameContext& ctx, bool preserveRatios) {
    float hpRatio = (maxHp > 0) ? (static_cast<float>(hp) / static_cast<float>(maxHp)) : 1.0f;
    float mpRatio = (stats.maxMp > 0) ? (static_cast<float>(mp) / static_cast<float>(stats.maxMp)) : 1.0f;
    Vector2 oldBottomCenter = { pos.x + width * 0.5f, pos.y + height };

    stats = ctx.ComputePlayerStats();
    width  = std::min(34.0f, Config::PLAYER_BASE_WIDTH * stats.scaleX);
    height = std::min(58.0f, Config::PLAYER_BASE_HEIGHT * stats.scaleY);

    pos.x = oldBottomCenter.x - width * 0.5f;
    pos.y = oldBottomCenter.y - height;

    maxHp = stats.maxHp;
    if (preserveRatios) {
        hp = std::clamp(static_cast<int>(std::round(maxHp * hpRatio)), 1, maxHp);
        mp = std::clamp(static_cast<int>(std::round(stats.maxMp * mpRatio)), 0, stats.maxMp);
    } else {
        hp = maxHp;
        mp = stats.maxMp;
    }
}

void Player::ResetRoomTrialFlags() {
    jumpedInCurrentRoom = false;
    damagedInCurrentRoom = false;
}

void Player::Update(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    std::vector<Projectile>& projectiles
) {
    if (!active) return;

    float animSpeedMult = ctx.CurrentHeirHasTrait(TraitType::ADHD) ? 1.35f : 1.0f;
    animTime += dt * animSpeedMult;

    if (iframeTimer > 0.0f)         iframeTimer -= dt;
    if (hurtFlashTimer > 0.0f)      hurtFlashTimer -= dt;
    if (landSquashTimer > 0.0f)     landSquashTimer -= dt;
    if (jumpStretchTimer > 0.0f)    jumpStretchTimer -= dt;
    if (dashCooldownTimer > 0.0f)   dashCooldownTimer -= dt;
    if (attackCooldownTimer > 0.0f) attackCooldownTimer -= dt;
    if (spellCooldownTimer > 0.0f)  spellCooldownTimer -= dt;
    if (talentCooldownTimer > 0.0f) talentCooldownTimer -= dt;
    if (timeStopTimer > 0.0f)       timeStopTimer -= dt;
    if (shoutVisualTimer > 0.0f)    shoutVisualTimer -= dt;
    if (dropThroughPlatformTimer > 0.0f) dropThroughPlatformTimer -= dt;

    if (hitstopTimer > 0.0f) {
        hitstopTimer -= dt;
        return;
    }

    // Автоматична регенерація мани для класу Dragon (35 MP/сек)
    if (ctx.activeHeir.classType == ClassType::Dragon_Special && mp < stats.maxMp) {
        manaRegenAccum += 35.0f * dt;
        if (manaRegenAccum >= 1.0f) {
            int gain = static_cast<int>(manaRegenAccum);
            manaRegenAccum -= static_cast<float>(gain);
            mp = std::min(stats.maxMp, mp + gain);
        }
    }

    // Підтримка Mist Form (Knave / Assassin: витрачає 14 MP/сек)
    if (isMistForm) {
        mistManaAccum += 14.0f * dt;
        if (mistManaAccum >= 1.0f) {
            int drain = static_cast<int>(mistManaAccum);
            mistManaAccum -= static_cast<float>(drain);
            mp -= drain;
            if (mp <= 0) {
                mp = 0;
                isMistForm = false;
            }
        }
        if (ctx.particles && (std::rand() % 4 == 0)) {
            ctx.particles->SpawnSmoke(GetCenter(), Color{ 70, 50, 100, 180 }, 1);
        }
    }

    // Риса IBS: кожні 4 секунди спавнить газову хмарку зі звуком
    if (ctx.CurrentHeirHasTrait(TraitType::IBS)) {
        ibsTimer += dt;
        if (ibsTimer >= 4.0f) {
            ibsTimer = 0.0f;
            if (ctx.particles) ctx.particles->SpawnGasCloud({ GetCenter().x, GetBottom() - 6.0f });
            if (ctx.audio)     ctx.audio->PlayIbsFart();
        }
    }

    HandleCombatAndAbilities(dt, ctx, tileMap, projectiles);
    HandleMovementAndPhysics(dt, ctx, tileMap, projectiles);

    // Перевірка контакту з шипами на тайловій карті
    if (tileMap && !isMistForm && iframeTimer <= 0.0f) {
        bool hasPad = ctx.CurrentHeirHasTrait(TraitType::PAD);
        Rectangle myBounds = GetBounds();
        int cMin = tileMap->WorldToCol(myBounds.x + 4.0f);
        int cMax = tileMap->WorldToCol(myBounds.x + myBounds.width - 4.0f);
        int rMin = tileMap->WorldToRow(myBounds.y + 4.0f);
        int rMax = tileMap->WorldToRow(myBounds.y + myBounds.height - 2.0f);

        bool touchedSpike = false;
        Vector2 spikePos = GetCenter();
        for (int r = rMin; r <= rMax && !touchedSpike; ++r) {
            for (int c = cMin; c <= cMax && !touchedSpike; ++c) {
                if (tileMap->IsSpikeHazardAt(c, r, hasPad)) {
                    Rectangle sBox = tileMap->GetSpikeHitbox(c, r);
                    if (CheckCollisionRecs(myBounds, sBox)) {
                        touchedSpike = true;
                        spikePos = { sBox.x + sBox.width * 0.5f, sBox.y + sBox.height * 0.5f };
                    }
                }
            }
        }

        if (touchedSpike) {
            int dummyRetal = 0;
            int spikeDmg = std::max(12, static_cast<int>(std::round(maxHp * 0.16f)));
            TakeDamage(spikeDmg, spikePos, ctx, "Castle Spikes", dummyRetal, &projectiles);
        }
    }
}

void Player::HandleMovementAndPhysics(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    std::vector<Projectile>& /*projectiles*/
) {
    bool isDragon = (ctx.activeHeir.classType == ClassType::Dragon_Special);
    bool isDwarf  = IsDwarf() || isMistForm;

    // Зчитування горизонтального та вертикального вводу
    float moveX = 0.0f;
    if (ctx.input.IsActionDown(InputAction::MoveLeft))  moveX -= 1.0f;
    if (ctx.input.IsActionDown(InputAction::MoveRight)) moveX += 1.0f;

    float moveY = 0.0f;
    if (ctx.input.IsActionDown(InputAction::MoveUp))    moveY -= 1.0f;
    if (ctx.input.IsActionDown(InputAction::MoveDown))  moveY += 1.0f;

    if (moveX != 0.0f && !isDashing) {
        facingDir = (moveX > 0.0f) ? 1.0f : -1.0f;
    }

    // 1. ВІЛЬНИЙ ПОЛІТ У 8 НАПРЯМКАХ ДЛЯ КЛАСУ DRAGON
    if (isDragon) {
        if (isDashing) {
            dashTimer -= dt;
            vel.x = dashDirX * Config::DASH_SPEED;
            if (dashTimer <= 0.0f) isDashing = false;
        } else {
            float len = std::hypot(moveX, moveY);
            if (len > 0.001f) {
                vel.x = (moveX / len) * stats.moveSpeed;
                vel.y = (moveY / len) * stats.moveSpeed;
            } else {
                vel.x *= std::pow(0.001f, dt);
                vel.y *= std::pow(0.001f, dt);
            }
        }

        float prevBottom = GetBottom();
        pos.x += vel.x * dt;
        ResolveTileCollisionsX(tileMap, isDwarf);
        pos.y += vel.y * dt;
        ResolveTileCollisionsY(tileMap, isDwarf, prevBottom);
        return;
    }

    // 2. РИВОК (DASH)
    bool dashPressed = ctx.input.IsActionPressed(InputAction::Dash);
    if (dashPressed && !isDashing && dashCooldownTimer <= 0.0f && airDashesLeft > 0) {
        isDashing = true;
        dashTimer = Config::DASH_DURATION;
        dashCooldownTimer = Config::DASH_COOLDOWN;
        dashDirX = (moveX != 0.0f) ? ((moveX > 0.0f) ? 1.0f : -1.0f) : facingDir;
        facingDir = dashDirX;
        if (!onGround) {
            airDashesLeft--;
        }
        vel.y = 0.0f;
        if (ctx.audio)     ctx.audio->PlaySfx(SfxId::Dash);
        if (ctx.particles) ctx.particles->SpawnDust({ GetCenter().x, GetBottom() }, 8);
    }

    if (isDashing) {
        dashTimer -= dt;
        vel.x = dashDirX * Config::DASH_SPEED;
        vel.y = 0.0f;
        if (ctx.particles && (std::rand() % 2 == 0)) {
            ctx.particles->SpawnDust(GetCenter(), 1);
        }
        if (dashTimer <= 0.0f) {
            isDashing = false;
        }
    } else {
        float speedFactor = isBlocking ? 0.55f : 1.0f;
        vel.x = moveX * stats.moveSpeed * speedFactor;
    }

    // 3. COYOTE TIME & JUMP BUFFER
    if (onGround) {
        coyoteTimer = Config::COYOTE_TIME_MAX;
        airJumpsLeft = stats.extraAirJumps;
        airDashesLeft = 1 + stats.maxAirDashes;
    } else {
        coyoteTimer = std::max(0.0f, coyoteTimer - dt);
    }

    bool jumpPressed  = ctx.input.IsActionPressed(InputAction::Jump);
    bool jumpReleased = ctx.input.IsActionReleased(InputAction::Jump);
    bool jumpHeld     = ctx.input.IsActionDown(InputAction::Jump);
    bool downHeld     = ctx.input.IsActionDown(InputAction::MoveDown);

    // Стрибок вниз крізь напівпрозору платформу '=' (S + Space або Down + Space)
    if (onGround && downHeld && jumpPressed) {
        dropThroughPlatformTimer = 0.30f;
        onGround = false;
        coyoteTimer = 0.0f;
        pos.y += 6.0f;
    } else if (jumpPressed) {
        jumpBufferTimer = Config::JUMP_BUFFER_MAX;
    } else {
        jumpBufferTimer = std::max(0.0f, jumpBufferTimer - dt);
    }

    // Виконання наземного стрибка (з урахуванням Coyote Time) або повітряного стрибка (Vault Rune)
    if (jumpBufferTimer > 0.0f) {
        if (coyoteTimer > 0.0f) {
            vel.y = Config::PLAYER_JUMP_VELOCITY;
            onGround = false;
            coyoteTimer = 0.0f;
            jumpBufferTimer = 0.0f;
            jumpedInCurrentRoom = true;
            jumpStretchTimer = 0.08f; // Phase 7: jump stretch 1.10 on Y
            if (ctx.audio)     ctx.audio->PlaySfx(SfxId::Jump);
            if (ctx.particles) ctx.particles->SpawnDust({ GetCenter().x, GetBottom() }, 6);
            if (ctx.CurrentHeirHasTrait(TraitType::IBS)) {
                if (ctx.particles) ctx.particles->SpawnGasCloud({ GetCenter().x, GetBottom() });
                if (ctx.audio)     ctx.audio->PlayIbsFart();
            }
        } else if (airJumpsLeft > 0) {
            vel.y = Config::PLAYER_JUMP_VELOCITY * 0.94f;
            airJumpsLeft--;
            jumpBufferTimer = 0.0f;
            jumpedInCurrentRoom = true;
            jumpStretchTimer = 0.08f; // Phase 7: jump stretch 1.10 on Y
            if (ctx.audio)     ctx.audio->PlaySfx(SfxId::Jump, 1.15f);
            if (ctx.particles) ctx.particles->SpawnDust({ GetCenter().x, GetBottom() }, 8);
            if (ctx.CurrentHeirHasTrait(TraitType::IBS)) {
                if (ctx.particles) ctx.particles->SpawnGasCloud({ GetCenter().x, GetBottom() });
                if (ctx.audio)     ctx.audio->PlayIbsFart();
            }
        }
    }

    // Variable Jump Height (обрізання швидкості стрибка при ранньому відпусканні клавіші)
    if (jumpReleased && vel.y < 0.0f) {
        vel.y *= Config::PLAYER_JUMP_CUT_MULTIPLIER;
    }

    // 4. ГРАВІТАЦІЯ ТА ПЛАНЕРУВАННЯ GRACE RUNE
    if (!isDashing) {
        vel.y += Config::GRAVITY * dt;
        float maxFall = (stats.graceRuneCount > 0 && jumpHeld && vel.y > 0.0f)
                        ? Config::GRACE_RUNE_MAX_FALL_SPEED
                        : Config::MAX_FALL_SPEED;
        if (vel.y > maxFall) {
            vel.y = maxFall;
        }
    }

    // 5. РУХ ТА РОЗВ'ЯЗАННЯ КОЛІЗІЙ З ТАЙЛАМИ
    float prevBottom = GetBottom();
    pos.x += vel.x * dt;
    ResolveTileCollisionsX(tileMap, isDwarf);

    onGround = false;
    pos.y += vel.y * dt;
    ResolveTileCollisionsY(tileMap, isDwarf, prevBottom);

    if (onGround) {
        isDownStriking = false;
        if (!wasOnGround) {
            landSquashTimer = 0.08f; // Phase 7: landing squash 0.85 Y / 1.12 X
            if (ctx.particles) ctx.particles->SpawnDust({ GetCenter().x, GetBottom() }, 6);
        }
        if (std::abs(vel.x) > 20.0f) {
            footstepDustTimer += dt;
            if (footstepDustTimer >= 0.18f) {
                footstepDustTimer = 0.0f;
                if (ctx.particles) ctx.particles->SpawnDust({ GetCenter().x - facingDir * 8.0f, GetBottom() }, 2);
            }
        }
    }
    wasOnGround = onGround;
}

void Player::HandleCombatAndAbilities(
    float dt,
    GameContext& ctx,
    const ITileMap* tileMap,
    std::vector<Projectile>& projectiles
) {
    ClassType cls = ctx.activeHeir.classType;
    bool downHeld = ctx.input.IsActionDown(InputAction::MoveDown);

    // 1. ОНОВЛЕННЯ ТАЙМЕРА АТАКИ МЕЧЕМ
    if (isAttacking) {
        attackTimer -= dt;
        if (attackTimer <= 0.0f) {
            isAttacking = false;
            isDownStriking = false;
            currentSwingHitIds.clear();
        }
    }

    // 2. АТАКА МЕЧЕМ АБО POGO DOWN-STRIKE
    bool attackPressed = ctx.input.IsActionPressed(InputAction::Attack);
    if (attackPressed && attackCooldownTimer <= 0.0f) {
        isMistForm = false;

        if (cls == ClassType::Dragon_Special) {
            // Клас Dragon стріляє вогняними кулями замість удару мечем (12 MP)
            if (mp >= 12) {
                mp -= 12;
                attackCooldownTimer = 0.26f;
                Vector2 aimDir = { facingDir, 0.0f };
                if (ctx.input.IsActionDown(InputAction::MoveUp))   aimDir.y = -0.6f;
                if (ctx.input.IsActionDown(InputAction::MoveDown)) aimDir.y =  0.6f;
                int fbDmg = std::max(8, static_cast<int>(std::round(stats.str * 0.65f + stats.intelligence * 0.85f)));
                projectiles.push_back(Projectile::MakeDragonFireball(GetCenter(), aimDir, fbDmg));
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 0.9f);
            }
        } else {
            isAttacking = true;
            isDownStriking = (!onGround && downHeld);
            attackTimer = Config::SWORD_ATTACK_DURATION;
            attackCooldownTimer = Config::SWORD_COOLDOWN;
            currentSwingHitIds.clear();
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SwordSlash);

            // Перевірка Pogo від шипів на тайловій карті
            if (isDownStriking && tileMap) {
                Rectangle swordBox = GetSwordHitbox();
                int cMin = tileMap->WorldToCol(swordBox.x);
                int cMax = tileMap->WorldToCol(swordBox.x + swordBox.width);
                int rMin = tileMap->WorldToRow(swordBox.y);
                int rMax = tileMap->WorldToRow(swordBox.y + swordBox.height);
                bool pogoHitSpike = false;
                for (int r = rMin; r <= rMax && !pogoHitSpike; ++r) {
                    for (int c = cMin; c <= cMax && !pogoHitSpike; ++c) {
                        char tc = tileMap->GetTileAt(c, r);
                        if (tc == '^' || tc == '~') {
                            Rectangle sBox = tileMap->GetSpikeHitbox(c, r);
                            if (CheckCollisionRecs(swordBox, sBox)) {
                                pogoHitSpike = true;
                                TriggerPogoBounce(ctx);
                                if (ctx.particles) {
                                    ctx.particles->SpawnSparks({ swordBox.x + swordBox.width * 0.5f, sBox.y }, GOLD, 10);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Якщо гравець натиснув вниз під час вже активного повітряного удару — переводимо в Pogo
    if (isAttacking && !onGround && downHeld) {
        isDownStriking = true;
    }

    // 3. КАСТ ЗАКЛЯТТЯ
    bool spellPressed = ctx.input.IsActionPressed(InputAction::CastSpell);
    if (spellPressed && spellCooldownTimer <= 0.0f) {
        isMistForm = false;
        SpellType spellToCast = activeSpell;
        if (ctx.CurrentHeirHasTrait(TraitType::Savant)) {
            spellToCast = static_cast<SpellType>(std::rand() % static_cast<int>(SpellType::COUNT));
        }

        const SpellDef& sDef = Registry::GetSpell(spellToCast);
        if (mp >= sDef.manaCost) {
            mp -= sDef.manaCost;
            spellCooldownTimer = sDef.cooldown;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast);

            if (spellToCast == SpellType::TimeStop) {
                timeStopTimer = 3.5f;
                if (ctx.particles) ctx.particles->SpawnExplosion(GetCenter(), 140.0f, SKYBLUE);
            } else {
                int spellDmg = std::max(5, static_cast<int>(std::round(stats.intelligence * sDef.intMultiplier)));
                Projectile::SpawnPlayerSpell(projectiles, spellToCast, GetCenter(), facingDir, spellDmg, 1.0f, 1.0f);
            }
        }
    }

    // 4. КЛАСОВИЙ ТАЛАНТ
    bool talentHeld    = ctx.input.IsActionDown(InputAction::ClassTalent);
    bool talentPressed = ctx.input.IsActionPressed(InputAction::ClassTalent);

    // Щит Лицаря / Паладина (утримання Q)
    if (cls == ClassType::Knights_Base || cls == ClassType::Paladin_Upgraded) {
        isBlocking = (talentHeld && mp >= 15 && !isAttacking);
    } else {
        isBlocking = false;
    }

    if (talentPressed && talentCooldownTimer <= 0.0f) {
        switch (cls) {
            case ClassType::Barbarian_Base:
            case ClassType::BarbarianKing_Upgraded: {
                // Barbarian Shout (25 MP): радіальна ударна хвиля на весь екран
                if (mp >= 25) {
                    mp -= 25;
                    talentCooldownTimer = 1.2f;
                    shoutVisualTimer = 0.45f;
                    pendingShout_.triggered = true;
                    pendingShout_.origin = GetCenter();
                    pendingShout_.radius = 1400.0f;
                    pendingShout_.knockbackSpeed = 450.0f;
                    pendingShout_.stunDuration = 1.5f;
                    pendingShout_.damage = (cls == ClassType::BarbarianKing_Upgraded)
                                           ? static_cast<int>(std::round(stats.str * 0.85f))
                                           : 5;
                    if (ctx.audio)     ctx.audio->PlaySfx(SfxId::BossRoar, 1.25f);
                    if (ctx.particles) ctx.particles->SpawnExplosion(GetCenter(), 220.0f, WHITE);
                }
                break;
            }

            case ClassType::Mage_Base: {
                // Arcane Focus (відновлює 8 MP на кулдауні)
                talentCooldownTimer = 3.5f;
                RestoreMp(8, ctx);
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 1.3f);
                break;
            }

            case ClassType::Archmage_Upgraded: {
                // Spell Cycle (0 MP): перемикає 3 закляття по колу
                archmageSpellIndex = (archmageSpellIndex + 1) % 3;
                activeSpell = archmageSpells[static_cast<size_t>(archmageSpellIndex)];
                talentCooldownTimer = 0.15f;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.85f);
                if (ctx.particles) {
                    ctx.particles->SpawnFloatingText(
                        { GetCenter().x, pos.y - 10.0f },
                        Registry::GetSpell(activeSpell).name,
                        SKYBLUE, 18, false
                    );
                }
                break;
            }

            case ClassType::Knave_Base:
            case ClassType::Assassin_Upgraded: {
                // Mist Form (14 MP/сек перемикач)
                if (isMistForm) {
                    isMistForm = false;
                } else if (mp >= 5) {
                    isMistForm = true;
                    if (ctx.audio)     ctx.audio->PlaySfx(SfxId::Dash, 0.75f);
                    if (ctx.particles) ctx.particles->SpawnSmoke(GetCenter(), PURPLE, 18);
                }
                talentCooldownTimer = 0.25f;
                break;
            }

            case ClassType::Shinobi_Base:
            case ClassType::Hokage_Upgraded: {
                // Replacement Jutsu (10 MP): миттєвий телепорт на 190px крізь стіни/шипи/ворогів зі спавном колоди
                if (mp >= 10) {
                    mp -= 10;
                    talentCooldownTimer = 0.45f;
                    Vector2 oldCenter = GetCenter();
                    if (ctx.particles) ctx.particles->SpawnDecoyLog(oldCenter);
                    if (ctx.audio)     ctx.audio->PlaySfx(SfxId::Dash, 1.35f);

                    float targetX = oldCenter.x + facingDir * 190.0f;
                    float minX = static_cast<float>(Config::ROOM_OFFSET_X + Config::TILE_SIZE) + width * 0.5f;
                    float maxX = static_cast<float>(Config::ROOM_OFFSET_X + (Config::ROOM_COLS - 1) * Config::TILE_SIZE) - width * 0.5f;
                    targetX = std::clamp(targetX, minX, maxX);

                    // Якщо точка приземлення всередині суцільної внутрішньої стіни — шукаємо найближчу вільну точку
                    if (tileMap) {
                        int destRow = tileMap->WorldToRow(oldCenter.y);
                        int destCol = tileMap->WorldToCol(targetX);
                        if (tileMap->IsSolidAt(destCol, destRow, IsDwarf())) {
                            for (int step = 1; step <= 4; ++step) {
                                float tryForward = targetX + facingDir * (step * 24.0f);
                                if (tryForward >= minX && tryForward <= maxX &&
                                    !tileMap->IsSolidAt(tileMap->WorldToCol(tryForward), destRow, IsDwarf())) {
                                    targetX = tryForward;
                                    break;
                                }
                                float tryBack = targetX - facingDir * (step * 24.0f);
                                if (tryBack >= minX && tryBack <= maxX &&
                                    !tileMap->IsSolidAt(tileMap->WorldToCol(tryBack), destRow, IsDwarf())) {
                                    targetX = tryBack;
                                    break;
                                }
                            }
                        }
                    }

                    SetCenter({ targetX, oldCenter.y });
                    if (ctx.particles) ctx.particles->SpawnSmoke(GetCenter(), WHITE, 18);
                }
                break;
            }

            case ClassType::Miner_Base:
            case ClassType::Spelunker_Upgraded: {
                // Headlamp (0 MP перемикач)
                headlampOn = !headlampOn;
                talentCooldownTimer = 0.20f;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitShield, 1.5f, 0.5f);
                break;
            }

            case ClassType::Lich_Base:
            case ClassType::LichKing_Upgraded: {
                // HP Conversion (0 MP): жертвує 30% поточного MaxHP, щоб миттєво відновити 100% MP
                int sacrifice = std::max(1, static_cast<int>(std::round(maxHp * 0.30f)));
                if (hp > sacrifice && mp < stats.maxMp) {
                    hp -= sacrifice;
                    mp = stats.maxMp;
                    talentCooldownTimer = 0.80f;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 0.75f);
                    if (ctx.particles) {
                        ctx.particles->SpawnBlood(GetCenter(), 12);
                        ctx.particles->SpawnHealSparkles(GetCenter(), SKYBLUE, 16);
                        ctx.particles->SpawnFloatingText(GetCenter(), "MAX MP!", SKYBLUE, 22, true);
                    }
                }
                break;
            }

            case ClassType::Spellthief_Base:
            case ClassType::Spellsword_Upgraded: {
                // Empowered Spell: витрачає 2x MP, щоб випустити гігантське закляття з 2.5x шкодою
                SpellType sp = activeSpell;
                if (ctx.CurrentHeirHasTrait(TraitType::Savant)) {
                    sp = static_cast<SpellType>(std::rand() % static_cast<int>(SpellType::COUNT));
                }
                const SpellDef& sDef = Registry::GetSpell(sp);
                int empCost = std::max(20, sDef.manaCost * 2);
                if (mp >= empCost) {
                    mp -= empCost;
                    talentCooldownTimer = 0.75f;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 0.72f);
                    if (sp == SpellType::TimeStop) {
                        timeStopTimer = 5.5f;
                    } else {
                        int baseDmg = std::max(8, static_cast<int>(std::round(stats.intelligence * sDef.intMultiplier)));
                        Projectile::SpawnPlayerSpell(projectiles, sp, GetCenter(), facingDir, baseDmg, 2.0f, 2.5f);
                    }
                }
                break;
            }

            case ClassType::Dragon_Special: {
                if (mp >= 15) {
                    mp -= 15;
                    talentCooldownTimer = 0.65f;
                    isDashing = true;
                    dashTimer = 0.18f;
                    dashDirX = facingDir;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::Dash, 0.85f);
                }
                break;
            }

            case ClassType::Traitor_Special: {
                // Fountain Surge: спавн 4 мечів з небес
                if (mp >= 20) {
                    mp -= 20;
                    talentCooldownTimer = 0.90f;
                    int bladeDmg = static_cast<int>(std::round((stats.str + stats.intelligence) * 0.95f));
                    for (int i = -2; i <= 2; ++i) {
                        if (i == 0) continue;
                        float bx = GetCenter().x + i * 95.0f;
                        projectiles.push_back(Projectile::MakeAncestorBlade(bx, bladeDmg));
                    }
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 1.1f);
                }
                break;
            }

            default:
                break;
        }
    }
}

void Player::ResolveTileCollisionsX(const ITileMap* tileMap, bool isDwarf) {
    if (!tileMap) return;

    Rectangle b = GetBounds();
    int rMin = tileMap->WorldToRow(b.y + 3.0f);
    int rMax = tileMap->WorldToRow(b.y + b.height - 3.0f);

    if (vel.x > 0.0f) {
        int cRight = tileMap->WorldToCol(b.x + b.width);
        for (int r = rMin; r <= rMax; ++r) {
            if (tileMap->IsSolidAt(cRight, r, isDwarf)) {
                Rectangle tRec = tileMap->GetTileRect(cRight, r);
                pos.x = tRec.x - width - 0.01f;
                vel.x = 0.0f;
                break;
            }
        }
    } else if (vel.x < 0.0f) {
        int cLeft = tileMap->WorldToCol(b.x);
        for (int r = rMin; r <= rMax; ++r) {
            if (tileMap->IsSolidAt(cLeft, r, isDwarf)) {
                Rectangle tRec = tileMap->GetTileRect(cLeft, r);
                pos.x = tRec.x + tRec.width + 0.01f;
                vel.x = 0.0f;
                break;
            }
        }
    }
}

void Player::ResolveTileCollisionsY(const ITileMap* tileMap, bool isDwarf, float prevBottom) {
    if (!tileMap) return;

    Rectangle b = GetBounds();
    int cMin = tileMap->WorldToCol(b.x + 3.0f);
    int cMax = tileMap->WorldToCol(b.x + b.width - 3.0f);

    if (vel.y >= 0.0f) {
        int rBottom = tileMap->WorldToRow(b.y + b.height);
        for (int c = cMin; c <= cMax; ++c) {
            if (tileMap->IsSolidAt(c, rBottom, isDwarf)) {
                Rectangle tRec = tileMap->GetTileRect(c, rBottom);
                pos.y = tRec.y - height;
                vel.y = 0.0f;
                onGround = true;
                break;
            }
            if (dropThroughPlatformTimer <= 0.0f && tileMap->IsOneWayAt(c, rBottom)) {
                Rectangle tRec = tileMap->GetTileRect(c, rBottom);
                if (prevBottom <= tRec.y + 10.0f) {
                    pos.y = tRec.y - height;
                    vel.y = 0.0f;
                    onGround = true;
                    break;
                }
            }
        }
    } else if (vel.y < 0.0f) {
        int rTop = tileMap->WorldToRow(b.y);
        for (int c = cMin; c <= cMax; ++c) {
            if (tileMap->IsSolidAt(c, rTop, isDwarf)) {
                Rectangle tRec = tileMap->GetTileRect(c, rTop);
                pos.y = tRec.y + tRec.height + 0.01f;
                vel.y = 0.0f;
                break;
            }
        }
    }
}

Rectangle Player::GetSwordHitbox() const {
    Vector2 c = GetCenter();
    float sx = stats.scaleX;
    float sy = stats.scaleY;

    if (isDownStriking) {
        float w = 38.0f * sx;
        float h = 48.0f * sy;
        return Rectangle{ c.x - w * 0.5f, GetBottom() - 6.0f, w, h };
    }

    float reachW = 58.0f * sx;
    float reachH = 50.0f * sy;
    if (facingDir >= 0.0f) {
        return Rectangle{ c.x + 2.0f, c.y - reachH * 0.55f, reachW, reachH };
    } else {
        return Rectangle{ c.x - 2.0f - reachW, c.y - reachH * 0.55f, reachW, reachH };
    }
}

int Player::RollMeleeDamage(bool isDownStrike, bool& outIsCrit) const {
    float r = static_cast<float>(std::rand() % 10000) / 10000.0f;
    outIsCrit = (r < stats.critChance);

    float dmg = static_cast<float>(stats.str);
    if (isDownStrike) {
        dmg *= (0.90f * stats.downStrikeDmgMult);
    }
    if (outIsCrit) {
        dmg *= stats.critDmg;
    }
    return std::max(1, static_cast<int>(std::round(dmg)));
}

void Player::TriggerPogoBounce(GameContext& ctx) {
    vel.y = Config::PLAYER_POGO_BOUNCE_VELOCITY;
    onGround = false;
    hitstopTimer = Config::HITSTOP_DURATION;
    if (ctx.audio) ctx.audio->PlaySfx(SfxId::PogoBounce);
}

void Player::OnMeleeDamageDealt(int damageDealt, GameContext& ctx) {
    hitstopTimer = Config::HITSTOP_DURATION;

    // Пасивка Spellthief / Spellsword: удар мечем відновлює MP у розмірі 30% / 35% від завданої шкоди
    if (ctx.activeHeir.classType == ClassType::Spellthief_Base ||
        ctx.activeHeir.classType == ClassType::Spellsword_Upgraded) {
        float ratio = (ctx.activeHeir.classType == ClassType::Spellsword_Upgraded) ? 0.35f : 0.30f;
        int manaGain = std::max(1, static_cast<int>(std::round(damageDealt * ratio)));
        RestoreMp(manaGain, ctx);
    }
}

void Player::OnEnemyKilled(GameContext& ctx, EnemyType enemyType) {
    ctx.runKills++;
    size_t tidx = static_cast<size_t>(enemyType);
    if (tidx < ctx.runSlainByType.size()) {
        ctx.runSlainByType[tidx]++;
    }

    // Vampire Rune (+4 HP за кожну руну)
    if (stats.vampireHealPerKill > 0) {
        HealHp(stats.vampireHealPerKill, ctx);
    }

    // Siphon Rune (+4 MP за кожну руну) та пасивка Mage / Archmage (+6 MP)
    int manaGain = stats.siphonManaPerKill;
    if (ctx.activeHeir.classType == ClassType::Mage_Base ||
        ctx.activeHeir.classType == ClassType::Archmage_Upgraded) {
        manaGain += 6;
    }
    if (manaGain > 0) {
        RestoreMp(manaGain, ctx);
    }

    // Пасивка Lich / Lich King "Soul Feast": +5 (+8) до MaxHP до ліміту 2.5x нормального HP
    if (ctx.activeHeir.classType == ClassType::Lich_Base ||
        ctx.activeHeir.classType == ClassType::LichKing_Upgraded) {
        int normalMaxHp = maxHp - ctx.lichBonusMaxHp;
        int capBonus = static_cast<int>(std::round(normalMaxHp * 1.50f));
        if (ctx.lichBonusMaxHp < capBonus) {
            int addHp = (ctx.activeHeir.classType == ClassType::LichKing_Upgraded) ? 8 : 5;
            ctx.lichBonusMaxHp = std::min(capBonus, ctx.lichBonusMaxHp + addHp);
            RecalculateStats(ctx, false);
            hp = std::min(maxHp, hp + addHp);
            if (ctx.particles) {
                ctx.particles->SpawnFloatingText(GetCenter(), "+" + std::to_string(addHp) + " MAX HP", LIME, 18, false);
            }
        }
    }
}

bool Player::TakeDamage(
    int rawDamage,
    Vector2 sourceCenter,
    GameContext& ctx,
    const std::string& attackerName,
    int& outRetaliationDamage,
    std::vector<Projectile>* outProjectiles
) {
    outRetaliationDamage = 0;
    if (!active || iframeTimer > 0.0f || isMistForm) {
        return false;
    }

    // Перевірка фронтального блоку щитом Лицаря / Паладина (Guardian Shield)
    float dirToSource = sourceCenter.x - GetCenter().x;
    bool sourceInFront = (dirToSource * facingDir >= -8.0f);
    if (isBlocking && sourceInFront && mp >= 15) {
        mp -= 15;
        iframeTimer = 0.28f;
        if (ctx.audio)     ctx.audio->PlaySfx(SfxId::HitShield);
        if (ctx.particles) ctx.particles->SpawnSparks(GetCenter(), SKYBLUE, 14);
        ctx.AddTrauma(0.18f);

        if (ctx.activeHeir.classType == ClassType::Paladin_Upgraded && outProjectiles) {
            int lightningDmg = std::max(10, static_cast<int>(std::round(stats.intelligence * 1.5f)));
            outProjectiles->push_back(Projectile::MakePaladinLightning(sourceCenter, lightningDmg));
        }
        return false;
    }

    // Формула зменшення шкоди від Armor
    float mitigation = 100.0f / (100.0f + static_cast<float>(stats.armor));
    int finalDmg = std::max(1, static_cast<int>(std::round(rawDamage * mitigation)));

    hp = std::max(0, hp - finalDmg);
    damagedInCurrentRoom = true;
    iframeTimer = stats.iframeDuration;
    hurtFlashTimer = 0.08f; // Phase 7: 80 ms pure white hit flash

    // Retaliation Rune: повертає 60% шкоди за кожну руну
    if (stats.retaliationFactor > 0.0f) {
        outRetaliationDamage = static_cast<int>(std::round(finalDmg * stats.retaliationFactor));
    }

    // Knockback з урахуванням Ectomorph (2.0x) та Endomorph (0.0x)
    float kbDir = (GetCenter().x >= sourceCenter.x) ? 1.0f : -1.0f;
    vel.x = kbDir * 280.0f * stats.knockbackTakenMult;
    vel.y = -260.0f * stats.knockbackTakenMult;

    if (ctx.particles) {
        ctx.particles->SpawnBlood(GetCenter(), 14);
        if (!ctx.CurrentHeirHasTrait(TraitType::CIP)) {
            ctx.particles->SpawnFloatingText(GetCenter(), "-" + std::to_string(finalDmg), RED, 22, false);
        }
    }

    if (hp <= 0) {
        ctx.lastKillerName = attackerName;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::PlayerDeath);
        ctx.AddTrauma(0.80f); // Screen shake trauma: 0.80 on player death
    } else {
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::PlayerHurt);
        ctx.AddTrauma(0.45f); // Screen shake trauma on hurt
    }

    return true;
}

void Player::HealHp(int amount, GameContext& ctx) {
    if (amount <= 0 || hp <= 0) return;
    int oldHp = hp;
    hp = std::min(maxHp, hp + amount);
    int healed = hp - oldHp;
    if (healed > 0 && ctx.particles) {
        ctx.particles->SpawnHealSparkles(GetCenter(), LIME, 10);
        if (!ctx.CurrentHeirHasTrait(TraitType::CIP)) {
            ctx.particles->SpawnFloatingText(GetCenter(), "+" + std::to_string(healed) + " HP", LIME, 20, false);
        }
    }
}

void Player::RestoreMp(int amount, GameContext& ctx) {
    if (amount <= 0) return;
    int oldMp = mp;
    mp = std::min(stats.maxMp, mp + amount);
    int gained = mp - oldMp;
    if (gained > 0 && ctx.particles) {
        ctx.particles->SpawnHealSparkles(GetCenter(), SKYBLUE, 8);
        ctx.particles->SpawnFloatingText(GetCenter(), "+" + std::to_string(gained) + " MP", SKYBLUE, 18, false);
    }
}

bool Player::ConsumeShoutPulse(ShoutPulseEvent& outEvent) {
    if (!pendingShout_.triggered) return false;
    outEvent = pendingShout_;
    pendingShout_.triggered = false;
    return true;
}

void Player::Draw(const GameContext& ctx) const {
    if (!active || !ctx.renderer) return;

    // Візуальна хвиля крику Варвара (Barbarian Shout)
    if (shoutVisualTimer > 0.0f) {
        float progress = 1.0f - (shoutVisualTimer / 0.45f);
        float r = progress * 640.0f;
        DrawRing(GetCenter(), r, r + 14.0f, 0.0f, 360.0f, 36, Fade(WHITE, 1.0f - progress));
    }

    HeroRenderParams rp{};
    rp.centerPos = GetCenter();
    if (onGround && std::abs(vel.x) < 10.0f && !isAttacking && !isDashing) {
        // Hero idle breathing bobbing ±1px (Phase 7)
        rp.centerPos.y += std::sin(animTime * 3.5f) * 1.0f;
    }
    rp.velocity = vel;

    float sx = stats.scaleX;
    float sy = stats.scaleY;
    if (landSquashTimer > 0.0f) {
        float t = landSquashTimer / 0.08f;
        sy *= (1.0f - 0.15f * t); // landing scaleY 0.85
        sx *= (1.0f + 0.12f * t); // landing scaleX 1.12
    } else if (jumpStretchTimer > 0.0f) {
        float t = jumpStretchTimer / 0.08f;
        sy *= (1.0f + 0.10f * t); // jump stretch 1.10 on Y
        sx *= (1.0f - 0.08f * t);
    }
    rp.scaleX = sx;
    rp.scaleY = sy;
    rp.height = height;
    rp.facingDir = facingDir;
    rp.classType = ctx.activeHeir.classType;
    rp.swordTier = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Sword)]);
    rp.helmTier  = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Helm)]);
    rp.chestTier = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Chest)]);
    rp.limbsTier = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Limbs)]);
    rp.capeTier  = static_cast<EquipTier>(ctx.equippedTier[static_cast<size_t>(EquipSlot::Cape)]);
    rp.isAttacking = isAttacking;
    rp.isDownStriking = isDownStriking;
    rp.attackProgress = isAttacking ? (1.0f - attackTimer / Config::SWORD_ATTACK_DURATION) : 0.0f;
    rp.isBlocking = isBlocking;
    rp.isMistForm = isMistForm;
    rp.headlampOn = headlampOn;
    rp.isInvulnerable = (iframeTimer > 0.0f);
    rp.isHitFlash = (hurtFlashTimer > 0.0f);
    rp.animTime = animTime;

    rp.isFemale = ctx.activeHeir.isFemale;
    for (auto t : ctx.activeHeir.traits) {
        if (t == TraitType::Ectomorph) rp.isEctomorph = true;
        if (t == TraitType::Endomorph) rp.isEndomorph = true;
    }

    ctx.renderer->DrawHero(rp);
}