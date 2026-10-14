// === FILE: src/states/LineageState.cpp ===
#include "states/LineageState.hpp"
#include "core/Config.hpp"
#include "core/GameContext.hpp"
#include "core/SaveManager.hpp"
#include "data/Registry.hpp"
#include "audio/SoundSynth.hpp"
#include "gfx/ProceduralRenderer.hpp"
#include "gfx/UiText.hpp"
#include <raylib.h>
#include <string>

void LineageState::OnEnter(GameContext& ctx) {
    selectedHeirIndex_ = 1; // За замовчуванням центральна картка спадкоємця
    animTimer_ = 0.0f;
    // Оновлюємо класи кандидатів з урахуванням можливої купівлі Master Observatory
    for (auto& cand : ctx.heirCandidates) {
        cand.classType = ctx.ResolveClassWithUpgrade(cand.classType);
    }
}

void LineageState::OnExit(GameContext& /*ctx*/) {}

void LineageState::Update(float dt, GameContext& ctx) {
    animTimer_ += dt;

    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
        selectedHeirIndex_ = (selectedHeirIndex_ + 2) % 3;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 0.95f, 0.6f);
    }
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
        selectedHeirIndex_ = (selectedHeirIndex_ + 1) % 3;
        if (ctx.audio) ctx.audio->PlaySfx(SfxId::CoinPickup, 1.05f, 0.6f);
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) {
        ctx.activeHeir = ctx.heirCandidates[static_cast<size_t>(selectedHeirIndex_)];
        ctx.generationCount = ctx.activeHeir.generation;
        ctx.lichBonusMaxHp = 0;
        ctx.runKills = 0;
        ctx.runGoldCollected = 0;
        ctx.runSlainByType.fill(0);

        if (ctx.audio) ctx.audio->PlaySfx(SfxId::ChestOpen);
        SaveManager::SaveGame(ctx, Config::SAVE_FILE_PATH);
        ctx.RequestStateChange(GameStateId::Manor);
    }
}

void LineageState::Draw(GameContext& ctx) {
    DrawRectangleGradientV(0, 0, Config::VIRTUAL_WIDTH, Config::VIRTUAL_HEIGHT, Color{ 18, 14, 28, 255 }, Color{ 10, 8, 16, 255 });

    // Decorative castle wall brick seams
    for (int by = 0; by < Config::VIRTUAL_HEIGHT; by += 48) {
        DrawLine(0, by, Config::VIRTUAL_WIDTH, by, Color{ 28, 22, 38, 180 });
    }

    const char* header = "CHOOSE YOUR HEIR";
    int hw = Ui::Measure(header, 32);
    Ui::Text(header, (Config::VIRTUAL_WIDTH - hw) / 2 + 2, 14 + 2, 32, BLACK);
    Ui::Text(header, (Config::VIRTUAL_WIDTH - hw) / 2, 14, 32, GOLD);

    const char* sub = "Select a brave descendant to carry on your family legacy and claim the castle.";
    int sw = Ui::Measure(sub, 14);
    Ui::Text(sub, (Config::VIRTUAL_WIDTH - sw) / 2, 48, 14, Color{ 180, 185, 205, 255 });

    int cardW = 390;
    int cardH = 468;
    int spacing = 20;
    int startX = (Config::VIRTUAL_WIDTH - (cardW * 3 + spacing * 2)) / 2;
    int cardY = 76;

    for (int i = 0; i < 3; ++i) {
        const HeirCandidate& h = ctx.heirCandidates[static_cast<size_t>(i)];
        const ClassDef& cls = Registry::GetClass(h.classType);
        const SpellDef& sp  = Registry::GetSpell(h.spell);
        bool isSelected = (i == selectedHeirIndex_);

        int cx = startX + i * (cardW + spacing);
        int cy = isSelected ? (cardY - 5) : cardY;

        Color bgCol = isSelected ? Color{ 32, 28, 48, 245 } : Color{ 20, 18, 30, 230 };
        Color borderCol = isSelected ? GOLD : Color{ 75, 70, 95, 255 };

        // Card frame
        Rectangle cardRec = { static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cardW), static_cast<float>(cardH) };
        DrawRectangleRounded(cardRec, 0.04f, 4, bgCol);
        DrawRectangleRoundedLines(cardRec, 0.04f, 4, isSelected ? 3.5f : 1.5f, borderCol);

        if (isSelected) {
            // Gold corner accents
            DrawLineEx({ cardRec.x, cardRec.y + 16.0f }, { cardRec.x, cardRec.y }, 3.5f, GOLD);
            DrawLineEx({ cardRec.x, cardRec.y }, { cardRec.x + 16.0f, cardRec.y }, 3.5f, GOLD);
            DrawLineEx({ cardRec.x + cardRec.width - 16.0f, cardRec.y }, { cardRec.x + cardRec.width, cardRec.y }, 3.5f, GOLD);
            DrawLineEx({ cardRec.x + cardRec.width, cardRec.y }, { cardRec.x + cardRec.width, cardRec.y + 16.0f }, 3.5f, GOLD);
        }

        // 1. Heir Name & Class Header Banner
        int nameW = Ui::Measure(h.name.c_str(), 20);
        Ui::Text(h.name.c_str(), cx + (cardW - nameW) / 2, cy + 12, 20, isSelected ? GOLD : RAYWHITE);

        std::string clsTitle = "Class: " + cls.name;
        int cw = Ui::Measure(clsTitle.c_str(), 14);
        Ui::Text(clsTitle.c_str(), cx + (cardW - cw) / 2, cy + 34, 14, SKYBLUE);

        // 2. Portrait Box with Stone Pedestal & Spotlight
        Rectangle portraitBox = { static_cast<float>(cx + 18), static_cast<float>(cy + 54), static_cast<float>(cardW - 36), 116.0f };
        DrawRectangleRounded(portraitBox, 0.08f, 4, Color{ 14, 12, 22, 255 });
        DrawRectangleRoundedLines(portraitBox, 0.08f, 4, 1.5f, borderCol);

        // Ambient spotlight on pedestal
        Vector2 heroPos = { portraitBox.x + portraitBox.width * 0.5f, portraitBox.y + 66.0f };
        DrawCircleGradient(static_cast<int>(heroPos.x), static_cast<int>(heroPos.y - 10.0f), 45.0f, Fade(Color{ 80, 110, 160, 255 }, 0.28f), BLANK);
        // Stone pedestal on the floor beneath feet
        float pedestalY = heroPos.y + 35.0f;
        DrawEllipse(static_cast<int>(heroPos.x), static_cast<int>(pedestalY), 48.0f, 11.0f, Color{ 36, 40, 56, 255 });
        DrawEllipseLines(static_cast<int>(heroPos.x), static_cast<int>(pedestalY), 48.0f, 11.0f, borderCol);

        if (ctx.renderer) {
            ctx.renderer->DrawHeroPreview(heroPos, 1.35f, h, ctx, isSelected ? animTimer_ : 0.4f);
        }

        // 3. Four Compact Graphical Stat Badges
        float statBadgeW = (cardW - 48.0f) / 4.0f;
        float statY = cy + 178.0f;
        if (ctx.renderer) {
            ctx.renderer->DrawStatBadge({ cx + 18.0f + 0.0f * (statBadgeW + 4.0f), statY }, statBadgeW, 26.0f, "HP",  static_cast<int>(cls.hpMult * 100.0f), RED);
            ctx.renderer->DrawStatBadge({ cx + 18.0f + 1.0f * (statBadgeW + 4.0f), statY }, statBadgeW, 26.0f, "MP",  static_cast<int>(cls.mpMult * 100.0f), SKYBLUE);
            ctx.renderer->DrawStatBadge({ cx + 18.0f + 2.0f * (statBadgeW + 4.0f), statY }, statBadgeW, 26.0f, "STR", static_cast<int>(cls.strMult * 100.0f), ORANGE);
            ctx.renderer->DrawStatBadge({ cx + 18.0f + 3.0f * (statBadgeW + 4.0f), statY }, statBadgeW, 26.0f, "INT", static_cast<int>(cls.intMult * 100.0f), Color{ 190, 120, 245, 255 });
        }

        // 4. Ability Slots (Spell [L] & Talent [Q])
        float slotW = cardW - 36.0f;
        float slotY = cy + 212.0f;
        if (ctx.renderer) {
            ctx.renderer->DrawAbilitySlot({ static_cast<float>(cx + 18), slotY }, slotW, 32.0f, "L", sp.name.c_str(), sp.manaCost, Color{ 255, 185, 55, 255 });
            ctx.renderer->DrawAbilitySlot({ static_cast<float>(cx + 18), slotY + 38.0f }, slotW, 32.0f, "Q", cls.talentName.c_str(), 0, Color{ 100, 255, 130, 255 });
        }

        // 5. Genetic Traits Badges
        float traitsY = cy + 292.0f;
        DrawLine(cx + 18, static_cast<int>(traitsY), cx + cardW - 18, static_cast<int>(traitsY), Fade(borderCol, 0.7f));
        traitsY += 8.0f;

        Ui::Text("GENETIC TRAITS", static_cast<float>(cx + 18), traitsY, 12, GOLD);
        traitsY += 18.0f;

        if (h.traits.empty()) {
            Rectangle pureRec = { static_cast<float>(cx + 18), traitsY, slotW, 30.0f };
            DrawRectangleRounded(pureRec, 0.4f, 4, Color{ 24, 32, 28, 230 });
            DrawRectangleRoundedLines(pureRec, 0.4f, 4, 1.2f, LIME);
            Ui::Text("- Pure Bloodline (No Afflictions) -", static_cast<float>(cx + 28), traitsY + 8.0f, 12, Color{ 130, 245, 160, 255 });
        } else {
            // Render trait pill badges
            float badgeH = 28.0f;
            for (size_t tIdx = 0; tIdx < h.traits.size() && tIdx < 3; ++tIdx) {
                TraitType t = h.traits[tIdx];
                if (ctx.renderer) {
                    ctx.renderer->DrawTraitBadge(
                        { static_cast<float>(cx + 18), traitsY + tIdx * (badgeH + 6.0f) },
                        slotW, badgeH, t, isSelected, animTimer_
                    );
                }
            }
        }
    }

    // ========================================================================
    // Bottom Inspector: HERITAGE CODEX & TRAIT DETAILS (Uncluttered)
    // ========================================================================
    const HeirCandidate& selHeir = ctx.heirCandidates[static_cast<size_t>(selectedHeirIndex_)];
    const ClassDef& selCls = Registry::GetClass(selHeir.classType);
    const SpellDef& selSp  = Registry::GetSpell(selHeir.spell);

    int codexX = startX;
    int codexY = 554;
    int codexW = cardW * 3 + spacing * 2;
    int codexH = 152;

    Rectangle codexRec = { static_cast<float>(codexX), static_cast<float>(codexY), static_cast<float>(codexW), static_cast<float>(codexH) };
    DrawRectangleRounded(codexRec, 0.05f, 4, Color{ 18, 16, 26, 245 });
    DrawRectangleRoundedLines(codexRec, 0.05f, 4, 2.0f, GOLD);

    // Header bar
    DrawRectangle(codexX + 2, codexY + 2, codexW - 4, 28, Color{ 32, 26, 44, 255 });
    DrawLine(codexX, codexY + 30, codexX + codexW, codexY + 30, GOLD);

    std::string codexTitle = "HERITAGE CODEX: " + selHeir.name + " (" + selCls.name + ")";
    Ui::Text(codexTitle.c_str(), codexX + 16, codexY + 7, 14, GOLD);

    const char* confirmHint = "[ENTER / SPACE] Claim Your Birthright   |   [A / D] Inspect Descendants";
    int chw = Ui::Measure(confirmHint, 12);
    Ui::Text(confirmHint, codexX + codexW - chw - 16, codexY + 8, 12, Color{ 255, 235, 120, 255 });

    // Left Column: Ability Descriptions (Spell & Talent)
    int colLeftW = 420;
    int contentY = codexY + 38;

    std::string spellInfo = "[L] " + selSp.name + " (" + std::to_string(selSp.manaCost) + " MP): " + selSp.description;
    Ui::TextWrapped(spellInfo.c_str(), codexX + 16, contentY, colLeftW, 12, Color{ 255, 215, 140, 255 });

    std::string talentInfo = "[Q] " + selCls.talentName + ": " + selCls.talentDesc;
    Ui::TextWrapped(talentInfo.c_str(), codexX + 16, contentY + 44, colLeftW, 12, Color{ 150, 245, 170, 255 });

    // Vertical Divider
    DrawLine(codexX + colLeftW + 28, codexY + 34, codexX + colLeftW + 28, codexY + codexH - 10, Color{ 60, 55, 75, 255 });

    // Right Column: Full Genetic Trait Descriptions
    int colRightX = codexX + colLeftW + 44;
    int colRightW = codexW - colLeftW - 60;
    int traitTextY = contentY;

    if (selHeir.traits.empty()) {
        Ui::Text("GENETIC PROFILE: Pure Bloodline", colRightX, traitTextY, 13, LIME);
        Ui::Text("This heir suffers from no biological afflictions or perceptual oddities.", colRightX, traitTextY + 20, 12, Color{ 210, 220, 235, 255 });
    } else {
        for (TraitType t : selHeir.traits) {
            const TraitDef& td = Registry::GetTrait(t);
            std::string tHeader = td.name + ": ";
            int thw = Ui::Measure(tHeader.c_str(), 12);

            // Draw mini trait icon in codex
            if (ctx.renderer) {
                ctx.renderer->DrawTraitIcon({ static_cast<float>(colRightX + 6), static_cast<float>(traitTextY + 7) }, t, 6.0f, GOLD);
            }
            Ui::Text(tHeader.c_str(), colRightX + 18, traitTextY, 12, GOLD);
            Ui::TextWrapped(td.description.c_str(), colRightX + 18 + thw, traitTextY, colRightW - thw - 20, 12, Color{ 225, 230, 242, 255 });
            traitTextY += 32;
        }
    }
}