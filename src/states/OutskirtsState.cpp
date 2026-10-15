// === FILE: src/states/OutskirtsState.cpp ===
#include "states/OutskirtsState.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/UiText.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <string>

namespace {
    constexpr float BLACKSMITH_X  = 310.0f;
    constexpr float ENCHANTRESS_X = 590.0f;
    constexpr float ARCHITECT_X   = 860.0f;
    constexpr float CHARON_GATE_X = 1110.0f;
    constexpr float GROUND_Y      = 580.0f;
}

void OutskirtsState::OnEnter(GameContext& /*ctx*/) {
    heroX_ = 150.0f;
    heroY_ = GROUND_Y;
    heroVelX_ = 0.0f;
    heroVelY_ = 0.0f;
    onGround_ = true;
    facingDir_ = 1.0f;
    animTimer_ = 0.0f;
    attackTimer_ = 0.0f;
    activeMenu_ = ModalMenu::None;
    menuSlotTab_ = 0;
    menuItemRow_ = 0;
    charonSequenceActive_ = false;
    charonTimer_ = 0.0f;
    gatePortcullisY_ = 0.0f;
    flyingCoins_.clear();
}

void OutskirtsState::OnExit(GameContext& ctx) {
    SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
}

void OutskirtsState::Update(float dt, GameContext& ctx) {
    animTimer_ += dt;
    if (ctx.particles) {
        ctx.particles->Update(dt);
    }

    // 1. Анімація сплати мита Харону та входу у Замок
    if (charonSequenceActive_) {
        charonTimer_ += dt;
        gatePortcullisY_ = std::min(165.0f, charonTimer_ * 150.0f);

        Vector2 charonHand = { CHARON_GATE_X - 45.0f, GROUND_Y - 46.0f };
        for (auto& fc : flyingCoins_) {
            if (!fc.active) continue;
            Vector2 toHand = { charonHand.x - fc.pos.x, charonHand.y - fc.pos.y };
            float d = std::hypot(toHand.x, toHand.y);
            if (d < 18.0f) {
                fc.active = false;
                if (ctx.audio)     ctx.audio->PlaySfx(SfxId::CoinPickup, 1.1f, 0.5f);
                if (ctx.particles) ctx.particles->SpawnSparks(charonHand, GOLD, 4);
            } else {
                fc.vel.x += (toHand.x / d) * 1800.0f * dt;
                fc.vel.y += (toHand.y / d) * 1800.0f * dt;
                fc.pos.x += fc.vel.x * dt;
                fc.pos.y += fc.vel.y * dt;
            }
        }

        if (charonTimer_ >= 1.35f) {
            ctx.RequestStateChange(GameStateId::CastlePlay);
        }
        return;
    }

    // 2. Якщо відкрито модальне меню Коваля / Чарівниці / Архітектора
    if (settingsMenu_.IsOpen()) {
        settingsMenu_.Update(dt, ctx);
        return;
    }

    if (activeMenu_ != ModalMenu::None) {
        UpdateModalMenu(ctx);
        return;
    }

    // Пауза / Налаштування по Escape
    if (IsKeyPressed(KEY_ESCAPE)) {
        settingsMenu_.Open(true);
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.9f, 0.6f);
        return;
    }

    // Повернення у Родовий Маєток (M або вихід вліво)
    if (ctx.input.IsActionPressed(InputAction::OpenMap) || heroX_ < 60.0f) {
        ctx.RequestStateChange(GameStateId::Manor);
        return;
    }

    // Спавн фонових частинок диму з кузні та іскор від чарівниці
    if (ctx.particles) {
        if (ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0 && (std::rand() % 16 == 0)) {
            ctx.particles->SpawnSmoke({ BLACKSMITH_X + 48.0f, GROUND_Y - 175.0f }, Color{ 70, 75, 90, 110 }, 1);
        }
        if (ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0 && (std::rand() % 14 == 0)) {
            ctx.particles->SpawnHealSparkles({ ENCHANTRESS_X + (-16.0f + static_cast<float>(std::rand() % 32)), GROUND_Y - 32.0f }, MAGENTA, 1);
        }
    }

    // 3. Пересування героя по бруківці
    float moveDir = 0.0f;
    if (ctx.input.IsActionDown(InputAction::MoveLeft))  moveDir -= 1.0f;
    if (ctx.input.IsActionDown(InputAction::MoveRight)) moveDir += 1.0f;

    heroVelX_ = moveDir * 310.0f;
    if (moveDir != 0.0f) {
        facingDir_ = (moveDir > 0.0f) ? 1.0f : -1.0f;
    }
    heroX_ = std::clamp(heroX_ + heroVelX_ * dt, 50.0f, CHARON_GATE_X + 10.0f);

    // 4. Стрибок та вертикальна фізика
    bool nearBlacksmith  = (std::abs(heroX_ - BLACKSMITH_X) < 75.0f && ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0);
    bool nearEnchantress = (std::abs(heroX_ - ENCHANTRESS_X) < 75.0f && ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0);
    bool nearArchitect   = (std::abs(heroX_ - ARCHITECT_X) < 75.0f && ctx.GetManorLevel(ManorNodeId::Unlock_Architect) > 0);
    bool nearCharonGate  = (std::abs(heroX_ - CHARON_GATE_X) < 85.0f);
    bool nearInteractive = (nearBlacksmith || nearEnchantress || nearArchitect || nearCharonGate);

    bool jumpPressed = ctx.input.IsActionPressed(InputAction::Jump);
    if (!jumpPressed && ctx.input.IsActionPressed(InputAction::MoveUp) && !nearInteractive) {
        jumpPressed = true;
    }

    if (jumpPressed && onGround_) {
        heroVelY_ = Config::PLAYER_JUMP_VELOCITY;
        onGround_ = false;
        if (ctx.audio)     ctx.audio->PlaySfx(SfxId::Jump);
        if (ctx.particles) ctx.particles->SpawnDust({ heroX_, GROUND_Y }, 6);
    }

    // Зрізання стрибка при передчасному відпусканні клавіші
    if (!onGround_ && heroVelY_ < 0.0f) {
        if (ctx.input.IsActionReleased(InputAction::Jump)) {
            heroVelY_ *= Config::PLAYER_JUMP_CUT_MULTIPLIER;
        }
    }

    // Гравітація
    if (!onGround_) {
        heroVelY_ = std::min(heroVelY_ + Config::GRAVITY * dt, Config::MAX_FALL_SPEED);
        heroY_ += heroVelY_ * dt;
        if (heroY_ >= GROUND_Y) {
            heroY_ = GROUND_Y;
            heroVelY_ = 0.0f;
            onGround_ = true;
            if (ctx.particles) ctx.particles->SpawnDust({ heroX_, GROUND_Y }, 5);
            if (ctx.audio && attackTimer_ > 0.0f && (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))) {
                ctx.audio->PlaySfx(SfxId::HitShield);
            }
        }
    }

    // 5. Атака мечем (J / Z / ЛКМ)
    if (attackTimer_ > 0.0f) {
        attackTimer_ -= dt;
        if (attackTimer_ <= 0.0f) {
            attackTimer_ = 0.0f;
        }
    }

    bool attackPressed = ctx.input.IsActionPressed(InputAction::Attack);
    if (attackPressed && attackTimer_ <= 0.0f) {
        attackTimer_ = Config::SWORD_ATTACK_DURATION;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::SwordSlash);
        if (ctx.particles) {
            Vector2 slashPos = { heroX_ + facingDir_ * 32.0f, heroY_ - 26.0f };
            ctx.particles->SpawnSparks(slashPos, Color{ 220, 235, 255, 255 }, 6);
        }
    }

    // 6. Взаємодія з NPC
    bool interact = ctx.input.IsActionPressed(InputAction::Interact) ||
                    ctx.input.IsActionPressed(InputAction::MoveUp) ||
                    IsKeyPressed(KEY_ENTER);
    if (interact) {
        if (nearBlacksmith) {
            activeMenu_ = ModalMenu::Blacksmith;
            menuSlotTab_ = 0;
            menuItemRow_ = 0;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild, 1.1f);
            return;
        }
        if (nearEnchantress) {
            activeMenu_ = ModalMenu::Enchantress;
            menuSlotTab_ = 0;
            menuItemRow_ = 0;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast, 1.1f);
            return;
        }
        if (nearArchitect) {
            activeMenu_ = ModalMenu::Architect;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitShield, 1.0f);
            return;
        }
    }

    // 7. Вхід у ворота Замку з Хароном
    if (heroX_ >= CHARON_GATE_X - 24.0f || (interact && nearCharonGate)) {
        if (!charonSequenceActive_) {
            activeMenu_ = ModalMenu::CharonToll;
            heroVelX_ = 0.0f;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::BossRoar, 0.7f, 0.6f);
            return;
        }
    }
}

void OutskirtsState::UpdateModalMenu(GameContext& ctx) {
    if (activeMenu_ == ModalMenu::CharonToll) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
            activeMenu_ = ModalMenu::None;
            heroX_ = std::min(heroX_, CHARON_GATE_X - 70.0f);
            return;
        }

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_E)) {
            activeMenu_ = ModalMenu::None;
            charonSequenceActive_ = true;
            charonTimer_ = 0.0f;
            heroY_ = GROUND_Y;
            heroVelY_ = 0.0f;
            onGround_ = true;
            attackTimer_ = 0.0f;

            CalculatedStats st = ctx.ComputePlayerStats();
            int keptGold = static_cast<int>(std::round(ctx.gold * st.hagglingKeptRatio));
            ctx.charonTollPaid = std::max(0, ctx.gold - keptGold);
            ctx.gold = keptGold;

            int coinVisualCount = std::clamp(ctx.charonTollPaid / 15 + 5, 6, 24);
            for (int i = 0; i < coinVisualCount; ++i) {
                TollCoinVisual tc;
                tc.pos = { heroX_, GROUND_Y - 35.0f };
                tc.vel = {
                    -140.0f + static_cast<float>(std::rand() % 220),
                    -260.0f - static_cast<float>(std::rand() % 180)
                };
                flyingCoins_.push_back(tc);
            }

            if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 0.8f);
            if (ctx.particles && ctx.charonTollPaid > 0) {
                ctx.particles->SpawnFloatingText(
                    { CHARON_GATE_X - 45.0f, GROUND_Y - 95.0f },
                    "CHARON TOLL: -" + std::to_string(ctx.charonTollPaid) + "G",
                    GOLD, 20, true
                );
            }
            return;
        }
        return;
    }

    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_Q)) {
        activeMenu_ = ModalMenu::None;
        SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
        return;
    }

    if (activeMenu_ == ModalMenu::Blacksmith) {
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  menuSlotTab_ = (menuSlotTab_ + 4) % 5;
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) menuSlotTab_ = (menuSlotTab_ + 1) % 5;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    menuItemRow_ = (menuItemRow_ + 4) % 5;
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  menuItemRow_ = (menuItemRow_ + 1) % 5;

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            size_t s = static_cast<size_t>(menuSlotTab_);
            size_t t = static_cast<size_t>(menuItemRow_);
            const EquipmentDef& eq = Registry::GetEquipment(static_cast<EquipSlot>(s), static_cast<EquipTier>(t));

            bool unlocked = ctx.unlockedBlueprints[s][t];
            bool owned    = ctx.ownedEquipment[s][t];

            if (!unlocked) {
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                return;
            }

            if (!owned) {
                if (ctx.gold >= eq.cost) {
                    ctx.gold -= eq.cost;
                    ctx.ownedEquipment[s][t] = true;
                    owned = true;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild);
                } else {
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                    return;
                }
            }

            if (owned) {
                // Якщо вже одягнено цей тир — знімаємо його
                if (ctx.equippedTier[s] == static_cast<int>(t)) {
                    ctx.equippedTier[s] = -1;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.8f);
                } else {
                    // Перевіряємо ліміт ваги спорядження перед одяганням
                    int oldTier = ctx.equippedTier[s];
                    ctx.equippedTier[s] = static_cast<int>(t);
                    if (ctx.GetCurrentEquipWeight() <= ctx.GetMaxEquipWeight()) {
                        if (ctx.audio) ctx.audio->PlaySfx(SfxId::HitShield, 1.2f);
                    } else {
                        // Перевищено MaxWeight — відкочуємо
                        ctx.equippedTier[s] = oldTier;
                        if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                    }
                }
            }
        }
    } else if (activeMenu_ == ModalMenu::Enchantress) {
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  menuSlotTab_ = (menuSlotTab_ + 4) % 5;
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) menuSlotTab_ = (menuSlotTab_ + 1) % 5;
        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    menuItemRow_ = (menuItemRow_ + 7) % 8;
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  menuItemRow_ = (menuItemRow_ + 1) % 8;

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            size_t s = static_cast<size_t>(menuSlotTab_);
            size_t r = static_cast<size_t>(menuItemRow_);
            RuneType rType = static_cast<RuneType>(r);
            const RuneDef& rDef = Registry::GetRune(rType);

            bool unlocked = ctx.unlockedRunes[s][r];
            bool owned    = ctx.ownedRunes[s][r];

            if (!unlocked) {
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                return;
            }
            if (!owned) {
                if (ctx.gold >= rDef.cost) {
                    ctx.gold -= rDef.cost;
                    ctx.ownedRunes[s][r] = true;
                    owned = true;
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::SpellCast);
                } else {
                    if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
                    return;
                }
            }
            if (owned) {
                if (ctx.equippedRunes[s] == rType) {
                    ctx.equippedRunes[s] = RuneType::None;
                } else {
                    ctx.equippedRunes[s] = rType;
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.15f);
            }
        }
    } else if (activeMenu_ == ModalMenu::Architect) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            if (ctx.hasSavedCastle) {
                ctx.isCastleLocked = !ctx.isCastleLocked;
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild);
            } else {
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail);
            }
        }
    }
}

void OutskirtsState::Draw(GameContext& ctx) {
    Camera2D worldCam{};
    worldCam.zoom = Config::SCALE_FACTOR;
    BeginMode2D(worldCam);

    // 1. Процедурний нічний готичний пейзаж (Бейканий бекдроп, зорі, місяць, хмари, туман, ліхтарі, трава)
    if (ctx.renderer) {
        ctx.renderer->DrawOutskirtsLandscape(ctx, animTimer_);
    } else {
        ClearBackground(BLACK);
    }

    // Вказівник повернення до Маєтку ліворуч (дерев'яний дороговказ)
    int signX = 28;
    int signY = static_cast<int>(GROUND_Y - 110.0f);
    DrawRectangle(signX + 44, signY, 8, static_cast<int>(GROUND_Y - signY), Color{ 58, 40, 28, 255 });
    DrawRectangleRounded({ static_cast<float>(signX), static_cast<float>(signY), 108.0f, 32.0f }, 0.25f, 4, Color{ 36, 26, 22, 240 });
    DrawRectangleRoundedLines({ static_cast<float>(signX), static_cast<float>(signY), 108.0f, 32.0f }, 0.25f, 4, 1.5f, GOLD);

    // 2. Кузня Коваля
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0 && ctx.renderer) {
        bool nearBs = (std::abs(heroX_ - BLACKSMITH_X) < 75.0f);
        ctx.renderer->DrawBlacksmithForge({ BLACKSMITH_X, GROUND_Y }, animTimer_, nearBs);
    }

    // 3. Намет Чарівниці (Enchantress Pavilion)
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0 && ctx.renderer) {
        bool nearEnch = (std::abs(heroX_ - ENCHANTRESS_X) < 75.0f);
        ctx.renderer->DrawEnchantressTent({ ENCHANTRESS_X, GROUND_Y }, animTimer_, nearEnch);
    }

    // 4. Стовп та альтанка Архітектора (Architect Drafting Gazebo)
    if (ctx.GetManorLevel(ManorNodeId::Unlock_Architect) > 0 && ctx.renderer) {
        bool nearArch = (std::abs(heroX_ - ARCHITECT_X) < 75.0f);
        ctx.renderer->DrawArchitectGazebo({ ARCHITECT_X, GROUND_Y }, animTimer_, ctx.isCastleLocked, nearArch);
    }

    // 5. Готична брама Замку та Харон
    int gx = static_cast<int>(CHARON_GATE_X);
    int gy = static_cast<int>(GROUND_Y);
    bool nearCharon = (std::abs(heroX_ - CHARON_GATE_X) < 85.0f);

    if (ctx.renderer) {
        ctx.renderer->DrawCastleGate({ CHARON_GATE_X, GROUND_Y }, gatePortcullisY_, animTimer_, nearCharon);
    }

    // Постать похмурого перевізника Харона перед входом
    int cx = gx - 45;
    int cy = gy;
    if (ctx.renderer) {
        ctx.renderer->DrawDetailedCharon({ static_cast<float>(cx), static_cast<float>(cy) }, animTimer_, nearCharon);
    }

    DrawRectangleRounded({ static_cast<float>(cx - 40), static_cast<float>(cy - 116), 80.0f, 22.0f }, 0.25f, 4, Color{ 20, 16, 28, 230 });
    DrawRectangleRoundedLines({ static_cast<float>(cx - 40), static_cast<float>(cy - 116), 80.0f, 22.0f }, 0.25f, 4, 1.5f, GOLD);

    if (nearCharon && activeMenu_ == ModalMenu::None && !charonSequenceActive_) {
        DrawRectangleRounded({ static_cast<float>(gx - 14), static_cast<float>(gy - 200), 168.0f, 26.0f }, 0.25f, 4, Fade(BLACK, 0.85f));
        DrawRectangleRoundedLines({ static_cast<float>(gx - 14), static_cast<float>(gy - 200), 168.0f, 26.0f }, 0.25f, 4, 1.5f, GOLD);
    }

    // 11. Рендер лицаря (зі стрибком, падінням та ударом мечем)
    if (ctx.renderer) {
        HeroRenderParams hp{};
        CalculatedStats st = ctx.ComputePlayerStats();
        hp.centerPos = { heroX_, heroY_ - 27.0f * st.scaleY };
        hp.velocity = { heroVelX_, heroVelY_ };
        hp.scaleX = st.scaleX;
        hp.scaleY = st.scaleY;
        hp.facingDir = facingDir_;
        hp.classType = ctx.activeHeir.classType;
        hp.swordTier = static_cast<EquipTier>(ctx.equippedTier[0]);
        hp.helmTier  = static_cast<EquipTier>(ctx.equippedTier[1]);
        hp.chestTier = static_cast<EquipTier>(ctx.equippedTier[2]);
        hp.limbsTier = static_cast<EquipTier>(ctx.equippedTier[3]);
        hp.capeTier  = static_cast<EquipTier>(ctx.equippedTier[4]);
        hp.isAttacking = (attackTimer_ > 0.0f);
        hp.isDownStriking = (!onGround_ && (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) && attackTimer_ > 0.0f);
        hp.attackProgress = (Config::SWORD_ATTACK_DURATION > 0.0f) ? (1.0f - (attackTimer_ / Config::SWORD_ATTACK_DURATION)) : 1.0f;
        hp.headlampOn = true;
        hp.animTime = animTimer_;
        ctx.renderer->DrawHero(hp);
    }

    // Монети, що летять до Харона
    for (const auto& fc : flyingCoins_) {
        if (fc.active) {
            DrawCircleV(fc.pos, 6.0f, GOLD);
            DrawCircleLines(static_cast<int>(fc.pos.x), static_cast<int>(fc.pos.y), 6.0f, ORANGE);
        }
    }

    if (ctx.particles) {
        ctx.particles->Draw();
    }
    EndMode2D();

    // 12. Текст та підписи у світі через GetWorldToScreen2D (повна роздільність 1080p, без пікселізації)
    Vector2 manorPos = GetWorldToScreen2D({ static_cast<float>(signX + 10), static_cast<float>(signY + 7) }, worldCam);
    Ui::Text("<-- [M] MANOR", static_cast<int>(manorPos.x), static_cast<int>(manorPos.y), 20, GOLD);

    if (ctx.GetManorLevel(ManorNodeId::Unlock_Blacksmith) > 0) {
        int bx = static_cast<int>(BLACKSMITH_X - 85.0f);
        int by = static_cast<int>(GROUND_Y - 150.0f);
        Vector2 bsPos = GetWorldToScreen2D({ static_cast<float>(bx + 36), static_cast<float>(by - 38) }, worldCam);
        Ui::Text("BLACKSMITH", static_cast<int>(bsPos.x), static_cast<int>(bsPos.y), 20, GOLD);
        if (std::abs(heroX_ - BLACKSMITH_X) < 75.0f) {
            Vector2 bsPrompt = GetWorldToScreen2D({ static_cast<float>(bx + 18), static_cast<float>(by - 68) }, worldCam);
            Ui::Text("[W / E] FORGE GEAR", static_cast<int>(bsPrompt.x), static_cast<int>(bsPrompt.y), 18, LIME);
        }
    }

    if (ctx.GetManorLevel(ManorNodeId::Unlock_Enchantress) > 0) {
        int ex = static_cast<int>(ENCHANTRESS_X);
        int ey = static_cast<int>(GROUND_Y);
        Vector2 enchPos = GetWorldToScreen2D({ static_cast<float>(ex - 52), static_cast<float>(ey - 184) }, worldCam);
        Ui::Text("ENCHANTRESS", static_cast<int>(enchPos.x), static_cast<int>(enchPos.y), 20, MAGENTA);
        if (std::abs(heroX_ - ENCHANTRESS_X) < 75.0f) {
            Vector2 enchPrompt = GetWorldToScreen2D({ static_cast<float>(ex - 67), static_cast<float>(ey - 214) }, worldCam);
            Ui::Text("[W / E] EQUIP RUNES", static_cast<int>(enchPrompt.x), static_cast<int>(enchPrompt.y), 18, LIME);
        }
    }

    Vector2 charonPos = GetWorldToScreen2D({ static_cast<float>(cx - 28), static_cast<float>(cy - 112) }, worldCam);
    Ui::Text("CHARON", static_cast<int>(charonPos.x), static_cast<int>(charonPos.y), 19, GOLD);
    if (nearCharon && activeMenu_ == ModalMenu::None && !charonSequenceActive_) {
        Vector2 charonPrompt = GetWorldToScreen2D({ static_cast<float>(gx - 2), static_cast<float>(gy - 195) }, worldCam);
        Ui::Text("[E / ENTER] PAY TOLL", static_cast<int>(charonPrompt.x), static_cast<int>(charonPrompt.y), 18, GOLD);
    }

    // 13. HUD та модальні вікна
    Camera2D hudCam{};
    hudCam.zoom = Config::SCALE_FACTOR;
    BeginMode2D(hudCam);

    // Нижня панель підказок керування
    int hintBarY = static_cast<int>(GROUND_Y + 98.0f);
    DrawRectangleRounded({ 160.0f, static_cast<float>(hintBarY), static_cast<float>(Config::VIRTUAL_WIDTH - 320), 32.0f }, 0.35f, 6, Color{ 14, 16, 26, 215 });
    DrawRectangleLinesEx({ 160.0f, static_cast<float>(hintBarY), static_cast<float>(Config::VIRTUAL_WIDTH - 320), 32.0f }, 1.5f, Color{ 60, 65, 85, 255 });
    Ui::Text("[A / D] MOVE   *   [SPACE] JUMP   *   [J / LMB] ATTACK   *   [W / E] INTERACT   *   [M] MANOR",
             205, hintBarY + 8, 14, Color{ 220, 228, 245, 255 });

    // Верхня інфо-панель
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, 56, Fade(BLACK, 0.72f));
    Ui::Text("HEIR: " + ctx.activeHeir.name + " (" + Registry::GetClass(ctx.activeHeir.classType).name + ")", 24, 18, 18, GOLD);
    Ui::Text("GOLD: " + std::to_string(ctx.gold) + " G", 540, 18, 18, YELLOW);
    CalculatedStats curSt = ctx.ComputePlayerStats();
    Ui::Text("WEIGHT: " + std::to_string(curSt.currentWeight) + " / " + std::to_string(curSt.maxWeight), 780, 18, 18, SKYBLUE);

    // 14. Модальні вікна NPC
    if (activeMenu_ == ModalMenu::Blacksmith) {
        DrawBlacksmithModal(ctx);
    } else if (activeMenu_ == ModalMenu::Enchantress) {
        DrawEnchantressModal(ctx);
    } else if (activeMenu_ == ModalMenu::Architect) {
        DrawArchitectModal(ctx);
    } else if (activeMenu_ == ModalMenu::CharonToll) {
        DrawCharonModal(ctx);
    }

    if (settingsMenu_.IsOpen()) {
        settingsMenu_.Draw(ctx);
    }

    EndMode2D();
}

void OutskirtsState::DrawBlacksmithModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 940;
    int mh = 550;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    Rectangle modalRec = { static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) };
    DrawRectangleRounded(modalRec, 0.04f, 4, Color{ 22, 24, 38, 252 });
    DrawRectangleRoundedLines(modalRec, 0.04f, 4, 3.5f, GOLD);

    // Золоті кутові ковані акценти
    DrawLineEx({ modalRec.x, modalRec.y + 24.0f }, { modalRec.x, modalRec.y }, 4.0f, GOLD);
    DrawLineEx({ modalRec.x, modalRec.y }, { modalRec.x + 24.0f, modalRec.y }, 4.0f, GOLD);
    DrawLineEx({ modalRec.x + modalRec.width - 24.0f, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y }, 4.0f, GOLD);
    DrawLineEx({ modalRec.x + modalRec.width, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y + 24.0f }, 4.0f, GOLD);

    Ui::Text("ROYAL BLACKSMITH FORGE", mx + 32, my + 18, 22, GOLD);
    Ui::Text("[A/D] Category   [W/S] Tier   [SPACE/ENTER] Buy / Equip   [ESC] Close", mx + 380, my + 22, 14, Color{ 220, 225, 235, 255 });

    // 5 вкладок категорій (Sword, Helm, Chest, Limbs, Cape) з іконками
    for (int s = 0; s < 5; ++s) {
        int tx = mx + 28 + s * 178;
        int ty = my + 64;
        bool activeTab = (s == menuSlotTab_);
        Rectangle tabRec = { static_cast<float>(tx), static_cast<float>(ty), 166.0f, 38.0f };
        DrawRectangleRounded(tabRec, 0.25f, 4, activeTab ? Color{ 82, 64, 26, 255 } : Color{ 34, 36, 52, 255 });
        DrawRectangleRoundedLines(tabRec, 0.25f, 4, activeTab ? 2.0f : 1.0f, activeTab ? GOLD : GRAY);

        if (ctx.renderer) {
            ctx.renderer->DrawEquipSlotIcon({ static_cast<float>(tx + 24), static_cast<float>(ty + 19) }, static_cast<EquipSlot>(s), activeTab);
        }
        std::string sName = Registry::GetEquipSlotName(static_cast<EquipSlot>(s));
        Ui::Text(sName.c_str(), tx + 46, ty + 10, 15, activeTab ? GOLD : RAYWHITE);
    }

    // Список 5 тирів обраного слота
    size_t slotIdx = static_cast<size_t>(menuSlotTab_);
    for (int t = 0; t < 5; ++t) {
        size_t tierIdx = static_cast<size_t>(t);
        const EquipmentDef& eq = Registry::GetEquipment(static_cast<EquipSlot>(slotIdx), static_cast<EquipTier>(tierIdx));
        bool unlocked = ctx.unlockedBlueprints[slotIdx][tierIdx];
        bool owned    = ctx.ownedEquipment[slotIdx][tierIdx];
        bool equipped = (ctx.equippedTier[slotIdx] == t);
        bool sel      = (t == menuItemRow_);

        int ry = my + 120 + t * 62;
        Rectangle rowRec = { static_cast<float>(mx + 28), static_cast<float>(ry), 475.0f, 54.0f };
        DrawRectangleRounded(rowRec, 0.25f, 4, sel ? Color{ 52, 56, 88, 255 } : Color{ 30, 32, 48, 255 });
        DrawRectangleRoundedLines(rowRec, 0.25f, 4, sel ? 2.0f : 1.0f, sel ? GOLD : Color{ 70, 75, 95, 255 });

        if (ctx.renderer) {
            ctx.renderer->DrawEquipTierBadge({ static_cast<float>(mx + 56), static_cast<float>(ry + 27) }, static_cast<EquipTier>(t));
        }

        Color nameCol = unlocked ? RAYWHITE : DARKGRAY;
        if (equipped) nameCol = LIME;
        Ui::Text(eq.name.c_str(), mx + 82, ry + 8, 17, nameCol);

        std::string statusStr;
        Color statusCol = GRAY;
        if (!unlocked) {
            statusStr = "LOCKED (Find Blueprint in Castle)";
            statusCol = RED;
        } else if (equipped) {
            statusStr = "EQUIPPED  [Weight: " + std::to_string(eq.weight) + "]";
            statusCol = LIME;
        } else if (owned) {
            statusStr = "OWNED - [SPACE] Equip  [Weight: " + std::to_string(eq.weight) + "]";
            statusCol = SKYBLUE;
        } else {
            statusStr = "FORGE: " + std::to_string(eq.cost) + " G  [Weight: " + std::to_string(eq.weight) + "]";
            statusCol = (ctx.gold >= eq.cost) ? YELLOW : RED;
        }
        Ui::Text(statusStr.c_str(), mx + 82, ry + 30, 13, statusCol);
    }

    // Права панель порівняння статів (Зелений '+' / Червоний '-') та шкала Ваги
    int px = mx + 524;
    int py = my + 120;
    Rectangle previewRec = { static_cast<float>(px), static_cast<float>(py), 388.0f, 304.0f };
    DrawRectangleRounded(previewRec, 0.08f, 4, Color{ 16, 18, 28, 255 });
    DrawRectangleRoundedLines(previewRec, 0.08f, 4, 2.0f, GOLD);

    const EquipmentDef& candEq = Registry::GetEquipment(static_cast<EquipSlot>(slotIdx), static_cast<EquipTier>(menuItemRow_));
    EquipmentDef curEq{};
    curEq.weight = 0;
    if (ctx.equippedTier[slotIdx] >= 0) {
        curEq = Registry::GetEquipment(static_cast<EquipSlot>(slotIdx), static_cast<EquipTier>(ctx.equippedTier[slotIdx]));
    }

    Ui::Text(candEq.name.c_str(), px + 20, py + 14, 20, GOLD);
    Ui::TextWrapped(candEq.description.c_str(), px + 20, py + 40, 348, 13, Color{ 210, 215, 225, 255 });

    auto DrawDiffLine = [&](const char* label, int curV, int newV, int yPos) {
        int diff = newV - curV;
        Ui::Text(label, px + 20, yPos, 16, RAYWHITE);
        std::string valStr = std::to_string(newV) + "  (" + (diff >= 0 ? "+" : "") + std::to_string(diff) + ")";
        Color c = (diff > 0) ? LIME : ((diff < 0) ? RED : Color{ 175, 180, 195, 255 });
        Ui::Text(valStr.c_str(), px + 195, yPos, 16, c);
    };

    DrawDiffLine("HP Bonus:",    curEq.hpBonus,    candEq.hpBonus,    py + 78);
    DrawDiffLine("MP Bonus:",    curEq.mpBonus,    candEq.mpBonus,    py + 106);
    DrawDiffLine("STR Bonus:",   curEq.strBonus,   candEq.strBonus,   py + 134);
    DrawDiffLine("INT Bonus:",   curEq.intBonus,   candEq.intBonus,   py + 162);
    DrawDiffLine("Armor Bonus:", curEq.armorBonus, candEq.armorBonus, py + 190);
    DrawDiffLine("Item Weight:", curEq.weight,     candEq.weight,     py + 218);

    // Шкала CurrentWeight / MaxWeight
    int curTotalWt = ctx.GetCurrentEquipWeight();
    int maxWt      = ctx.GetMaxEquipWeight();
    int projectedWt = curTotalWt - curEq.weight + candEq.weight;

    int barY = my + 448;
    Ui::Text(("EQUIP WEIGHT CAPACITY: " + std::to_string(curTotalWt) + " / " + std::to_string(maxWt) +
             "  (Projected: " + std::to_string(projectedWt) + ")").c_str(), mx + 32, barY, 15, (projectedWt <= maxWt) ? SKYBLUE : RED);

    Rectangle barRec = { static_cast<float>(mx + 28), static_cast<float>(barY + 26), static_cast<float>(mw - 56), 24.0f };
    DrawRectangleRounded(barRec, 0.35f, 4, BLACK);
    float wtRatio = std::clamp(static_cast<float>(projectedWt) / static_cast<float>(std::max(1, maxWt)), 0.0f, 1.0f);
    if (wtRatio > 0.01f) {
        Rectangle fillRec = { barRec.x, barRec.y, barRec.width * wtRatio, barRec.height };
        DrawRectangleRounded(fillRec, 0.35f, 4, (projectedWt <= maxWt) ? SKYBLUE : RED);
    }
    DrawRectangleRoundedLines(barRec, 0.35f, 4, 1.5f, GOLD);
}

void OutskirtsState::DrawEnchantressModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 940;
    int mh = 570;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    Rectangle modalRec = { static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) };
    DrawRectangleRounded(modalRec, 0.04f, 4, Color{ 26, 18, 38, 252 });
    DrawRectangleRoundedLines(modalRec, 0.04f, 4, 3.5f, MAGENTA);

    // Аметистові магічні кути
    DrawLineEx({ modalRec.x, modalRec.y + 24.0f }, { modalRec.x, modalRec.y }, 4.0f, MAGENTA);
    DrawLineEx({ modalRec.x, modalRec.y }, { modalRec.x + 24.0f, modalRec.y }, 4.0f, MAGENTA);
    DrawLineEx({ modalRec.x + modalRec.width - 24.0f, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y }, 4.0f, MAGENTA);
    DrawLineEx({ modalRec.x + modalRec.width, modalRec.y }, { modalRec.x + modalRec.width, modalRec.y + 24.0f }, 4.0f, MAGENTA);

    Ui::Text("ENCHANTRESS RUNE SANCTUM", mx + 32, my + 18, 22, MAGENTA);
    Ui::Text("Identical Runes in different slots STACK their effects!", mx + 430, my + 22, 14, GOLD);

    // Вкладки слотів з іконками
    for (int s = 0; s < 5; ++s) {
        int tx = mx + 28 + s * 178;
        int ty = my + 60;
        bool activeTab = (s == menuSlotTab_);
        Rectangle tabRec = { static_cast<float>(tx), static_cast<float>(ty), 166.0f, 44.0f };
        DrawRectangleRounded(tabRec, 0.25f, 4, activeTab ? Color{ 92, 34, 115, 255 } : Color{ 36, 26, 52, 255 });
        DrawRectangleRoundedLines(tabRec, 0.25f, 4, activeTab ? 2.0f : 1.0f, activeTab ? MAGENTA : GRAY);

        if (ctx.renderer) {
            ctx.renderer->DrawEquipSlotIcon({ static_cast<float>(tx + 24), static_cast<float>(ty + 22) }, static_cast<EquipSlot>(s), activeTab);
        }
        std::string sName = Registry::GetEquipSlotName(static_cast<EquipSlot>(s));
        Ui::Text(sName.c_str(), tx + 46, ty + 6, 14, activeTab ? GOLD : RAYWHITE);

        RuneType eqR = ctx.equippedRunes[static_cast<size_t>(s)];
        std::string rSub = (eqR == RuneType::None) ? "[Empty]" : Registry::GetRune(eqR).name;
        Ui::Text(rSub.c_str(), tx + 46, ty + 24, 12, (eqR == RuneType::None) ? Color{ 170, 175, 190, 255 } : SKYBLUE);
    }

    // Список 8 рун з індивідуальними procedural іконками
    size_t slotIdx = static_cast<size_t>(menuSlotTab_);
    for (int r = 0; r < 8; ++r) {
        size_t rIdx = static_cast<size_t>(r);
        RuneType rType = static_cast<RuneType>(r);
        const RuneDef& rDef = Registry::GetRune(rType);

        bool unlocked = ctx.unlockedRunes[slotIdx][rIdx];
        bool owned    = ctx.ownedRunes[slotIdx][rIdx];
        bool equipped = (ctx.equippedRunes[slotIdx] == rType);
        bool sel      = (r == menuItemRow_);

        int ry = my + 118 + r * 53;
        Rectangle rowRec = { static_cast<float>(mx + 28), static_cast<float>(ry), static_cast<float>(mw - 56), 47.0f };
        DrawRectangleRounded(rowRec, 0.25f, 4, sel ? Color{ 64, 40, 92, 255 } : Color{ 32, 22, 48, 255 });
        DrawRectangleRoundedLines(rowRec, 0.25f, 4, sel ? 2.0f : 1.0f, sel ? GOLD : Color{ 75, 55, 95, 255 });

        // Унікальна рунна скрижаль ліворуч
        if (ctx.renderer) {
            ctx.renderer->DrawRuneIcon({ static_cast<float>(mx + 62), static_cast<float>(ry + 23) }, rType, unlocked, animTimer_);
        }

        Ui::Text(rDef.name.c_str(), mx + 96, ry + 6, 17, unlocked ? (equipped ? LIME : GOLD) : DARKGRAY);
        Ui::Text(rDef.description.c_str(), mx + 96, ry + 26, 13, unlocked ? Color{ 215, 220, 230, 255 } : DARKGRAY);

        // Статус-бейдж праворуч
        std::string rightStatus;
        Color rightCol = GRAY;
        if (!unlocked) {
            rightStatus = "LOCKED (Fairy Chest)";
            rightCol = RED;
        } else if (equipped) {
            rightStatus = "EQUIPPED";
            rightCol = LIME;
        } else if (owned) {
            rightStatus = "OWNED - [SPACE] Equip";
            rightCol = SKYBLUE;
        } else {
            rightStatus = "ENCHANT: " + std::to_string(rDef.cost) + " G";
            rightCol = (ctx.gold >= rDef.cost) ? YELLOW : RED;
        }

        int rw = Ui::Measure(rightStatus.c_str(), 14);
        Rectangle pillRec = { static_cast<float>(mx + mw - rw - 58), static_cast<float>(ry + 10), static_cast<float>(rw + 18), 26.0f };
        DrawRectangleRounded(pillRec, 0.4f, 4, Color{ 20, 16, 30, 240 });
        DrawRectangleRoundedLines(pillRec, 0.4f, 4, 1.2f, rightCol);
        Ui::Text(rightStatus.c_str(), static_cast<int>(pillRec.x + 9.0f), ry + 16, 14, rightCol);
    }
}

void OutskirtsState::DrawArchitectModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 640;
    int mh = 320;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    DrawRectangle(mx, my, mw, mh, Color{ 20, 28, 42, 250 });
    DrawRectangleLinesEx({ static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) }, 3.5f, SKYBLUE);

    bool hasDyslexia = ctx.CurrentHeirHasTrait(TraitType::Dyslexia);
    auto Fmt = [hasDyslexia](const std::string& s) -> std::string {
        return hasDyslexia ? Ui::ScrambleDyslexia(s) : s;
    };

    Ui::Text(Fmt("THE ROYAL ARCHITECT").c_str(), mx + 28, my + 22, 24, SKYBLUE);
    Ui::Text(Fmt("Lock the exact 9x9 Castle Layout from your previous run").c_str(), mx + 28, my + 68, 16, RAYWHITE);
    Ui::Text(Fmt("so you can return straight to unopened Fairy Chests and Bosses.").c_str(), mx + 28, my + 92, 16, RAYWHITE);
    Ui::Text(Fmt("Penalty: All Gold collected while locked is reduced to 70%.").c_str(), mx + 28, my + 125, 16, ORANGE);

    std::string stateTxt = std::string("CURRENT STATUS: ") + (ctx.isCastleLocked ? "CASTLE LOCKED (70% GOLD)" : "FRESH CASTLE (100% GOLD)");
    Ui::Text(Fmt(stateTxt), mx + 28, my + 180, 20, ctx.isCastleLocked ? GOLD : LIME);

    Ui::Text(Fmt("Press [SPACE / ENTER] to Toggle Lock   |   Press [ESC] to Close"), mx + 28, my + 252, 16, Color{ 220, 225, 235, 255 });
}

void OutskirtsState::DrawCharonModal(const GameContext& ctx) const {
    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.78f));

    int mw = 680;
    int mh = 420;
    int mx = (Config::VIRTUAL_WIDTH - mw) / 2;
    int my = (Config::VIRTUAL_HEIGHT - mh) / 2;

    Rectangle modalRec = { static_cast<float>(mx), static_cast<float>(my), static_cast<float>(mw), static_cast<float>(mh) };
    DrawRectangleRounded(modalRec, 0.04f, 4, Color{ 18, 16, 26, 252 });
    DrawRectangleRoundedLines(modalRec, 0.04f, 4, 2.5f, GOLD);

    // Header bar
    DrawRectangle(mx + 2, my + 2, mw - 4, 38, Color{ 36, 26, 48, 255 });
    DrawLine(mx, my + 40, mx + mw, my + 40, GOLD);
    Ui::Text("CHARON'S TOLL - FERRYMAN OF CASTLE HAMSON", mx + 24, my + 11, 17, GOLD);

    // Left side: Charon figure showcase
    if (ctx.renderer) {
        ctx.renderer->DrawDetailedCharon({ static_cast<float>(mx + 115), static_cast<float>(my + 280) }, animTimer_, true);
    }

    // Right side: Dialogue & Gold ledger
    int rx = mx + 230;
    int rw = mw - 260;

    bool hasDyslexia = ctx.CurrentHeirHasTrait(TraitType::Dyslexia);
    auto Fmt = [hasDyslexia](const std::string& s) -> std::string {
        return hasDyslexia ? Ui::ScrambleDyslexia(s) : s;
    };

    Ui::Text(Fmt("\"All who seek entry into Castle Hamson").c_str(), rx, my + 56, 13, Color{ 235, 220, 180, 255 });
    Ui::Text(Fmt(" must pay the Ferryman his toll.").c_str(), rx, my + 74, 13, Color{ 235, 220, 180, 255 });
    Ui::Text(Fmt(" Earthly riches cannot cross into the underworld.\"").c_str(), rx, my + 92, 13, Color{ 235, 220, 180, 255 });

    // Ledger frame
    Rectangle ledgerRec = { static_cast<float>(rx), static_cast<float>(my + 118), static_cast<float>(rw), 188.0f };
    DrawRectangleRounded(ledgerRec, 0.05f, 4, Color{ 26, 22, 36, 255 });
    DrawRectangleRoundedLines(ledgerRec, 0.05f, 4, 1.5f, Color{ 90, 85, 115, 255 });

    CalculatedStats st = ctx.ComputePlayerStats();
    int totalGold = ctx.gold;
    int keptGold = static_cast<int>(std::round(totalGold * st.hagglingKeptRatio));
    int tollPaid = std::max(0, totalGold - keptGold);
    int discountPercent = static_cast<int>(std::round(st.hagglingKeptRatio * 100.0f));

    Ui::Text(Fmt("GOLD SETTLEMENT LEDGER").c_str(), rx + 16, my + 130, 13, GOLD);
    DrawLine(rx + 16, my + 148, rx + rw - 16, my + 148, Color{ 70, 65, 88, 255 });

    std::string goldCarried = "Total Gold Carried: " + std::to_string(totalGold) + " G";
    Ui::Text(goldCarried.c_str(), rx + 16, my + 158, 14, Color{ 255, 235, 80, 255 });

    std::string haggleStr = "Haggling Skill Retention: " + std::to_string(discountPercent) + "% (Kept: " + std::to_string(keptGold) + " G)";
    Ui::Text(haggleStr.c_str(), rx + 16, my + 186, 12, (keptGold > 0) ? LIME : Color{ 180, 185, 200, 255 });

    DrawLine(rx + 16, my + 214, rx + rw - 16, my + 214, Color{ 70, 65, 88, 255 });

    std::string tollStr = "Charon's Ferry Fee: -" + std::to_string(tollPaid) + " G";
    Ui::Text(tollStr.c_str(), rx + 16, my + 226, 15, (tollPaid > 0) ? Color{ 255, 110, 110, 255 } : LIME);

    std::string remainingStr = "Gold Remaining in Pocket: " + std::to_string(keptGold) + " G";
    Ui::Text(remainingStr.c_str(), rx + 16, my + 258, 13, GOLD);

    // Action buttons at bottom
    Rectangle enterBtn = { static_cast<float>(rx), static_cast<float>(my + 328), static_cast<float>(rw), 38.0f };
    DrawRectangleRec(enterBtn, Color{ 42, 92, 48, 255 });
    DrawRectangleLinesEx(enterBtn, 2.0f, GOLD);
    const char* enterTxt = "PAY TOLL & ENTER CASTLE [ENTER / SPACE]";
    int etw = Ui::Measure(enterTxt, 12);
    Ui::Text(enterTxt, rx + (rw - etw) / 2, static_cast<int>(enterBtn.y + 12.0f), 12, WHITE);

    Rectangle backBtn = { static_cast<float>(mx + 30), static_cast<float>(my + 328), 170.0f, 38.0f };
    DrawRectangleRec(backBtn, Color{ 48, 38, 55, 255 });
    DrawRectangleLinesEx(backBtn, 1.5f, Color{ 110, 100, 125, 255 });
    const char* backTxt = "CAMP [ESC / TAB]";
    int btw = Ui::Measure(backTxt, 13);
    Ui::Text(backTxt, mx + 30 + (170 - btw) / 2, static_cast<int>(backBtn.y + 11.0f), 13, Color{ 220, 220, 235, 255 });
}