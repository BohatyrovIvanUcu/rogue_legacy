// === FILE: src/states/ManorState.cpp ===
#include "states/ManorState.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ParticleSystem.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/UiText.hpp"
#include <raylib.h>
#include <string>
#include <cmath>
#include <algorithm>

void ManorState::OnEnter(GameContext& /*ctx*/) {
    animTimer_ = 0.0f;
}

void ManorState::OnExit(GameContext& ctx) {
    SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
}

bool ManorState::IsNodeUnlocked(ManorNodeId id, const GameContext& ctx) const {
    const ManorNodeDef& def = Registry::GetManorNode(id);
    if (def.parentNodeId == ManorNodeId::None) {
        return true;
    }
    return ctx.GetManorLevel(def.parentNodeId) > 0;
}

ManorNodeId ManorState::FindNodeAtGrid(int gx, int gy) const {
    for (int i = 0; i < static_cast<int>(ManorNodeId::COUNT); ++i) {
        ManorNodeId id = static_cast<ManorNodeId>(i);
        const ManorNodeDef& def = Registry::GetManorNode(id);
        if (def.gridX == gx && def.gridY == gy) {
            return id;
        }
    }
    return ManorNodeId::None;
}

void ManorState::Update(float dt, GameContext& ctx) {
    animTimer_ += dt;
    if (ctx.particles) {
        ctx.particles->Update(dt);
    }

    const ManorNodeDef& curDef = Registry::GetManorNode(static_cast<ManorNodeId>(selectedNodeIdx_));
    int curGx = curDef.gridX;
    int curGy = curDef.gridY;

    int moveDx = 0;
    int moveDy = 0;
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  moveDx = -1;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) moveDx =  1;
    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    moveDy = -1;
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  moveDy =  1;

    if (moveDx != 0 || moveDy != 0) {
        // Пошук найближчого вузла в обраному напрямку на сітці 7x5
        int bestIdx = selectedNodeIdx_;
        float bestScore = 99999.0f;

        for (int i = 0; i < static_cast<int>(ManorNodeId::COUNT); ++i) {
            if (i == selectedNodeIdx_) continue;
            const ManorNodeDef& cand = Registry::GetManorNode(static_cast<ManorNodeId>(i));
            int dx = cand.gridX - curGx;
            int dy = cand.gridY - curGy;

            if (moveDx != 0 && dx * moveDx <= 0) continue;
            if (moveDy != 0 && dy * moveDy <= 0) continue;

            float primaryDist = static_cast<float>(std::abs(moveDx != 0 ? dx : dy));
            float orthoDist   = static_cast<float>(std::abs(moveDx != 0 ? dy : dx));
            float score = primaryDist + orthoDist * 2.4f;
            if (score < bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }

        if (bestIdx != selectedNodeIdx_) {
            selectedNodeIdx_ = bestIdx;
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.95f, 0.5f);
        }
    }

    // Купівля рівня обраного вузла маєтку (SPACE / ENTER)
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
        ManorNodeId id = static_cast<ManorNodeId>(selectedNodeIdx_);
        const ManorNodeDef& node = Registry::GetManorNode(id);
        int curLvl = ctx.GetManorLevel(id);
        bool unlocked = IsNodeUnlocked(id, ctx);

        if (unlocked && curLvl < node.maxLevel) {
            int cost = Registry::GetNodeCost(node, curLvl, ctx.GetTotalManorLevels());
            if (ctx.gold >= cost) {
                ctx.gold -= cost;
                ctx.manorLevels[static_cast<size_t>(id)]++;
                // Якщо щойно купили Master Observatory — одразу оновлюємо клас поточного спадкоємця
                if (id == ManorNodeId::Upgrade_AllBasicClasses) {
                    ctx.activeHeir.classType = ctx.ResolveClassWithUpgrade(ctx.activeHeir.classType);
                }
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::ManorBuild);
                if (ctx.particles) {
                    Vector2 nodeScreenPos = {
                        76.0f + node.gridX * 104.0f,
                        126.0f + node.gridY * 108.0f
                    };
                    ctx.particles->SpawnSparks(nodeScreenPos, GOLD, 18);
                }
                SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
            } else {
                if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail, 1.2f, 0.6f);
            }
        } else {
            if (ctx.audio) ctx.audio->PlaySfx(SfxId::FairyFail, 1.2f, 0.6f);
        }
    }

    // Перехід у Передмістя Замку (E / TAB / ESC)
    if (IsKeyPressed(KEY_E) || IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_ESCAPE)) {
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen, 1.1f, 0.7f);
        ctx.RequestStateChange(GameStateId::Outskirts);
    }
}

void ManorState::Draw(GameContext& ctx) {
    // 1. Деталізований готичний замок на фоні, що росте з кожним купленим апгрейдом
    if (ctx.renderer) {
        ctx.renderer->DrawManorCastle(ctx, animTimer_);
    } else {
        ClearBackground(BLACK);
    }

    DrawRectangle(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Fade(BLACK, 0.45f));

    // Заголовок та загальний рівень маєтку
    int totalLvl = ctx.GetTotalManorLevels();
    std::string header = "ANCESTRAL MANOR";
    std::string totalStr = "TOTAL LEVEL: " + std::to_string(totalLvl);
    std::string goldStr = "GOLD: " + std::to_string(ctx.gold) + " G";

    Ui::Text(header.c_str(), 36, 14, 26, Color{ 255, 215, 0, 255 });
    Ui::Text(totalStr.c_str(), 36, 44, 15, Color{ 120, 210, 255, 255 });
    Ui::Text(goldStr.c_str(), 220, 44, 17, Color{ 255, 235, 70, 255 });

    const char* controlsHint = "[WASD] Select Node   [SPACE / ENTER] Upgrade   [E / TAB] Outskirts & Castle";
    int chw = Ui::Measure(controlsHint, 13);
    Ui::Text(controlsHint, Config::VIRTUAL_WIDTH - chw - 36, 20, 13, Color{ 230, 235, 245, 255 });

    auto GetNodeScreenCenter = [](const ManorNodeDef& n) -> Vector2 {
        return Vector2{
            76.0f + n.gridX * 104.0f,
            126.0f + n.gridY * 108.0f
        };
    };

    // 2. Лінії зв'язків між батьківськими та дочірніми вузлами дерева
    for (int i = 0; i < static_cast<int>(ManorNodeId::COUNT); ++i) {
        ManorNodeId id = static_cast<ManorNodeId>(i);
        const ManorNodeDef& def = Registry::GetManorNode(id);
        if (def.parentNodeId != ManorNodeId::None) {
            const ManorNodeDef& pDef = Registry::GetManorNode(def.parentNodeId);
            Vector2 a = GetNodeScreenCenter(pDef);
            Vector2 b = GetNodeScreenCenter(def);
            bool unlocked = ctx.GetManorLevel(def.parentNodeId) > 0;

            if (unlocked) {
                // Golden illuminated conduit
                DrawLineEx(a, b, 4.0f, GOLD);
                DrawLineEx(a, b, 2.0f, Color{ 255, 245, 180, 255 });

                // Animated energy pulse travelling along conduit
                float pulseT = std::fmod(animTimer_ * 1.6f + (i * 0.17f), 1.0f);
                Vector2 pulsePos = { a.x + (b.x - a.x) * pulseT, a.y + (b.y - a.y) * pulseT };
                DrawCircleV(pulsePos, 3.5f, WHITE);
            } else {
                // Dark locked chain link
                DrawLineEx(a, b, 3.0f, Color{ 60, 55, 75, 200 });
            }
        }
    }

    // 3. Вузли дерева прокачки (24 вузли з процедурними іконками)
    float boxW = 96.0f;
    float boxH = 74.0f;

    for (int i = 0; i < static_cast<int>(ManorNodeId::COUNT); ++i) {
        ManorNodeId id = static_cast<ManorNodeId>(i);
        const ManorNodeDef& def = Registry::GetManorNode(id);
        Vector2 c = GetNodeScreenCenter(def);
        int lvl = ctx.GetManorLevel(id);
        bool unlocked = IsNodeUnlocked(id, ctx);
        bool selected = (i == selectedNodeIdx_);
        int cost = Registry::GetNodeCost(def, lvl, totalLvl);
        bool canAfford = unlocked && (lvl < def.maxLevel) && (ctx.gold >= cost);

        Rectangle rec = { c.x - boxW * 0.5f, c.y - boxH * 0.5f, boxW, boxH };

        Color fillCol = Color{ 22, 22, 34, 235 };
        if (!unlocked)                fillCol = Color{ 14, 14, 20, 215 };
        else if (lvl >= def.maxLevel) fillCol = Color{ 24, 48, 32, 245 };
        else if (lvl > 0)             fillCol = Color{ 28, 38, 64, 245 };

        Color borderCol = Color{ 75, 80, 102, 255 };
        if (selected)                 borderCol = GOLD;
        else if (lvl >= def.maxLevel) borderCol = LIME;
        else if (canAfford)           borderCol = SKYBLUE;

        DrawRectangleRounded(rec, 0.20f, 4, fillCol);
        DrawRectangleRoundedLines(rec, 0.20f, 4, selected ? 3.0f : 1.5f, borderCol);

        if (selected) {
            float glow = 0.5f + 0.5f * std::sin(animTimer_ * 6.0f);
            DrawRectangleRoundedLines(rec, 0.20f, 4, 1.0f, Fade(GOLD, glow));
        }

        // Процедурна іконка вузла
        Vector2 iconPos = { c.x, rec.y + 19.0f };
        DrawCircleV(iconPos, 13.0f, Color{ 16, 14, 24, 255 });
        DrawCircleLines(static_cast<int>(iconPos.x), static_cast<int>(iconPos.y), 13.0f, Fade(borderCol, 0.6f));
        if (ctx.renderer) {
            ctx.renderer->DrawManorNodeIcon(iconPos, id, unlocked, lvl >= def.maxLevel, animTimer_);
        }

        // Назва вузла
        std::string dName = def.name;
        size_t sp = dName.find(' ');
        if (sp != std::string::npos && Ui::Measure(dName.c_str(), 10) > static_cast<int>(boxW - 8)) {
            std::string w1 = dName.substr(0, sp);
            std::string w2 = dName.substr(sp + 1);
            int w1W = Ui::Measure(w1.c_str(), 9);
            int w2W = Ui::Measure(w2.c_str(), 9);
            Color textCol = unlocked ? WHITE : Color{ 150, 155, 170, 255 };
            Ui::Text(w1.c_str(), static_cast<int>(c.x - w1W * 0.5f), static_cast<int>(rec.y + 36.0f), 9, textCol);
            Ui::Text(w2.c_str(), static_cast<int>(c.x - w2W * 0.5f), static_cast<int>(rec.y + 47.0f), 9, textCol);
        } else {
            int nw = Ui::Measure(dName.c_str(), 10);
            Color textCol = unlocked ? WHITE : Color{ 150, 155, 170, 255 };
            Ui::Text(dName.c_str(), static_cast<int>(c.x - nw * 0.5f), static_cast<int>(rec.y + 38.0f), 10, textCol);
        }

        // Нижня плашка статусу або ціни
        if (lvl >= def.maxLevel) {
            int mw = Ui::Measure("MAX", 10);
            DrawRectangleRounded({ c.x - mw * 0.5f - 4.0f, rec.y + boxH - 16.0f, mw + 8.0f, 13.0f }, 0.4f, 2, Color{ 30, 80, 42, 255 });
            Ui::Text("MAX", static_cast<int>(c.x - mw * 0.5f), static_cast<int>(rec.y + boxH - 15.0f), 10, LIME);
        } else if (!unlocked) {
            int lw = Ui::Measure("LOCKED", 9);
            Ui::Text("LOCKED", static_cast<int>(c.x - lw * 0.5f), static_cast<int>(rec.y + boxH - 15.0f), 9, Color{ 180, 90, 90, 255 });
        } else {
            std::string statusStr = std::to_string(lvl) + "/" + std::to_string(def.maxLevel);
            if (canAfford) {
                statusStr += " (" + std::to_string(cost) + "G)";
            }
            int sw = Ui::Measure(statusStr.c_str(), 10);
            Color costCol = canAfford ? Color{ 255, 235, 70, 255 } : Color{ 140, 205, 255, 255 };
            Ui::Text(statusStr.c_str(), static_cast<int>(c.x - sw * 0.5f), static_cast<int>(rec.y + boxH - 15.0f), 10, costCol);
        }
    }

    // 4. Бічна інспекційна панель обраного вузла та поточного спадкоємця
    int panelX = 810;
    int panelY = 66;
    int panelW = 445;
    int panelH = 636;
    Rectangle pRec = { static_cast<float>(panelX), static_cast<float>(panelY), static_cast<float>(panelW), static_cast<float>(panelH) };
    DrawRectangleRounded(pRec, 0.04f, 4, Color{ 16, 18, 28, 250 });
    DrawRectangleRoundedLines(pRec, 0.04f, 4, 2.5f, GOLD);

    ManorNodeId selId = static_cast<ManorNodeId>(selectedNodeIdx_);
    const ManorNodeDef& selDef = Registry::GetManorNode(selId);
    int selLvl = ctx.GetManorLevel(selId);
    bool selUnlocked = IsNodeUnlocked(selId, ctx);
    int selCost = Registry::GetNodeCost(selDef, selLvl, totalLvl);

    // Велика вітрина іконки
    Vector2 bigIconC = { static_cast<float>(panelX + 44), static_cast<float>(panelY + 44) };
    DrawCircleV(bigIconC, 24.0f, Color{ 26, 24, 38, 255 });
    DrawCircleLines(static_cast<int>(bigIconC.x), static_cast<int>(bigIconC.y), 24.0f, GOLD);
    if (ctx.renderer) {
        ctx.renderer->DrawManorNodeIcon(bigIconC, selId, selUnlocked, selLvl >= selDef.maxLevel, animTimer_);
    }

    Ui::Text(selDef.name.c_str(), panelX + 78, panelY + 18, 22, Color{ 255, 215, 0, 255 });
    std::string rankLine = "Rank: " + std::to_string(selLvl) + " / " + std::to_string(selDef.maxLevel);
    Ui::Text(rankLine.c_str(), panelX + 78, panelY + 46, 16, Color{ 120, 210, 255, 255 });

    if (!selUnlocked && selDef.parentNodeId != ManorNodeId::None) {
        std::string req = "Requires: " + Registry::GetManorNode(selDef.parentNodeId).name;
        Ui::Text(req.c_str(), panelX + 22, panelY + 80, 15, Color{ 255, 100, 100, 255 });
    } else if (selLvl >= selDef.maxLevel) {
        Ui::Text("MAXIMUM LEVEL REACHED", panelX + 22, panelY + 80, 15, Color{ 100, 255, 130, 255 });
    } else {
        std::string costText = "Upgrade Cost: " + std::to_string(selCost) + " G";
        Color costCol = (ctx.gold >= selCost) ? Color{ 255, 235, 70, 255 } : Color{ 255, 100, 100, 255 };
        Ui::Text(costText.c_str(), panelX + 22, panelY + 80, 16, costCol);
        std::string infText = "(Includes +" + std::to_string(totalLvl * Config::MANOR_INFLATION_PER_LEVEL) + "G Manor Inflation)";
        Ui::Text(infText.c_str(), panelX + 22, panelY + 102, 12, Color{ 180, 185, 205, 255 });
    }

    DrawLine(panelX + 20, panelY + 124, panelX + panelW - 20, panelY + 124, GOLD);
    int descH = Ui::TextWrapped(selDef.description.c_str(), panelX + 22, panelY + 132, panelW - 44, 13, Color{ 235, 240, 250, 255 });

    int statsStartY = panelY + 132 + descH + 14;
    DrawLine(panelX + 20, statsStartY, panelX + panelW - 20, statsStartY, GOLD);
    statsStartY += 10;

    CalculatedStats st = ctx.ComputePlayerStats();
    Ui::Text(("ACTIVE HEIR: " + ctx.activeHeir.name).c_str(), panelX + 22, statsStartY, 16, Color{ 255, 215, 0, 255 });
    Ui::Text(("Class: " + Registry::GetClass(ctx.activeHeir.classType).name).c_str(), panelX + 22, statsStartY + 24, 14, Color{ 120, 210, 255, 255 });

    int sy = statsStartY + 50;
    Ui::Text(("Max HP: " + std::to_string(st.maxHp)).c_str(), panelX + 22, sy, 14, Color{ 100, 255, 130, 255 });
    Ui::Text(("Max MP: " + std::to_string(st.maxMp)).c_str(), panelX + 220, sy, 14, Color{ 120, 210, 255, 255 });
    Ui::Text(("Strength: " + std::to_string(st.str)).c_str(), panelX + 22, sy + 22, 14, Color{ 255, 175, 55, 255 });
    Ui::Text(("Intelligence: " + std::to_string(st.intelligence)).c_str(), panelX + 220, sy + 22, 14, Color{ 255, 120, 220, 255 });
    Ui::Text(("Armor: " + std::to_string(st.armor)).c_str(), panelX + 22, sy + 44, 14, Color{ 220, 225, 235, 255 });
    Ui::Text(("Weight: " + std::to_string(st.currentWeight) + " / " + std::to_string(st.maxWeight)).c_str(), panelX + 220, sy + 44, 14, Color{ 255, 235, 70, 255 });

    char bonusBuf[128];
    std::snprintf(
        bonusBuf, sizeof(bonusBuf),
        "Crit: %d%% (x%.2f)   |   Gold Gain: %d%%",
        static_cast<int>(std::round(st.critChance * 100.0f)),
        st.critDmg,
        static_cast<int>(std::round(st.goldGainMult * 100.0f))
    );
    Ui::Text(bonusBuf, panelX + 22, sy + 70, 13, Color{ 240, 245, 255, 255 });

    // Кнопка покупки рівня
    Rectangle buyBtn = { static_cast<float>(panelX + 22), static_cast<float>(panelY + panelH - 104), static_cast<float>(panelW - 44), 40.0f };
    bool canBuy = selUnlocked && (selLvl < selDef.maxLevel) && (ctx.gold >= selCost);
    Color buyBtnCol = canBuy ? Color{ 45, 95, 50, 255 } : Color{ 55, 45, 50, 255 };
    DrawRectangleRec(buyBtn, buyBtnCol);
    DrawRectangleLinesEx(buyBtn, 2.0f, canBuy ? GOLD : Color{ 85, 75, 80, 255 });

    std::string buyTxt = (selLvl >= selDef.maxLevel) ? "MAX LEVEL ACHIEVED" : (canBuy ? ("UPGRADE RANK [SPACE / ENTER] (" + std::to_string(selCost) + " G)") : ("UPGRADE RANK [SPACE] (" + std::to_string(selCost) + " G)"));
    int btw = Ui::Measure(buyTxt.c_str(), 14);
    Ui::Text(buyTxt.c_str(), panelX + (panelW - btw) / 2, static_cast<int>(buyBtn.y + 12.0f), 14, canBuy ? WHITE : Color{ 180, 175, 180, 255 });

    // Кнопка переходу у Передмістя
    Rectangle goBtn = { static_cast<float>(panelX + 22), static_cast<float>(panelY + panelH - 54), static_cast<float>(panelW - 44), 38.0f };
    DrawRectangleRec(goBtn, Color{ 36, 52, 75, 255 });
    DrawRectangleLinesEx(goBtn, 2.0f, SKYBLUE);
    const char* goTxt = "ENTER OUTSKIRTS & CASTLE [E]";
    int gtw = Ui::Measure(goTxt, 14);
    Ui::Text(goTxt, panelX + (panelW - gtw) / 2, static_cast<int>(goBtn.y + 11.0f), 14, WHITE);

    if (ctx.particles) {
        ctx.particles->Draw();
    }
}