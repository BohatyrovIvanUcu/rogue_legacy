#pragma once
#include "core/Types.hpp"
#include <raylib.h>
#include <algorithm>

struct Ramp {
    Color c[5];

    constexpr Color operator[](int idx) const {
        return c[idx < 0 ? 0 : (idx > 4 ? 4 : idx)];
    }
};

struct BiomePalette {
    Ramp stoneBg;
    Ramp stoneTile;
    Ramp accent;
};

struct Palette {
    // 1. Матеріальні рампи (5 відтінків: від найтемнішого [0] до найсвітлішого [4])
    // Тіні зміщені у синьо-фіолетовий (#10152A), світло — у теплий жовтий

    // Камінь (фон): виразний сланцево-бірюзовий камінь замку
    Ramp stoneBg = { {
        Color{ 22, 30, 34, 255 },  // Шви / розчин (#161E22)
        Color{ 30, 40, 46, 255 },  // Тіньовий камінь
        Color{ 36, 48, 54, 255 },  // Базовий сланцево-бірюзовий камінь (#243036)
        Color{ 44, 58, 65, 255 },  // Світліші цегляні ряди
        Color{ 56, 74, 82, 255 }   // Відблиск рядів
    } };

    // Камінь (тайли твердих стін): світлий тепло-сірий камінь Rogue Legacy
    Ramp stoneTile = { {
        Color{ 74, 72, 66, 255 },   // Темний розчин / шви (#4A4842)
        Color{ 122, 120, 112, 255 },// Тіньові цеглини (#7A7870)
        Color{ 142, 140, 132, 255 },// Базовий колір цегли (#8E8C84)
        Color{ 158, 156, 148, 255 },// Світлі цеглини (#9E9C94)
        Color{ 210, 214, 218, 255 } // Зовнішня рельєфна фаска / бордюр (#D2D6DA)
    } };

    // Зручний аліас pal.stone -> stoneTile
    const Ramp& stone = stoneTile;

    // Небезпека (шипи, снаряди, зона атаки) — єдиний "гарячий" акцент
    Ramp danger = { {
        Color{ 90, 18, 14, 255 },
        Color{ 170, 32, 20, 255 },
        Color{ 232, 50, 32, 255 },
        Color{ 255, 95, 45, 255 },
        Color{ 255, 180, 110, 255 }
    } };

    // Дерево: платформи, скрині, держаки
    Ramp wood = { {
        Color{ 42, 26, 20, 255 },   // #2A1A14
        Color{ 63, 42, 30, 255 },   // #3F2A1E
        Color{ 92, 61, 42, 255 },   // #5C3D2A
        Color{ 133, 88, 58, 255 },  // #85583A
        Color{ 176, 122, 78, 255 }  // #B07A4E
    } };

    // Метал: обладунки, леза, скоби, шипи
    Ramp metal = { {
        Color{ 27, 29, 38, 255 },   // #1B1D26
        Color{ 46, 49, 63, 255 },   // #2E313F
        Color{ 74, 79, 99, 255 },   // #4A4F63
        Color{ 122, 129, 154, 255 },// #7A819A
        Color{ 181, 187, 208, 255 } // #B5BBD0
    } };

    // Вогонь / золото: полум'я, монети, золоті ворота, акценти
    Ramp fireGold = { {
        Color{ 90, 30, 14, 255 },   // #5A1E0E
        Color{ 163, 58, 18, 255 },  // #A33A12
        Color{ 232, 115, 28, 255 }, // #E8731C
        Color{ 255, 179, 71, 255 }, // #FFB347
        Color{ 255, 230, 168, 255 } // #FFE6A8
    } };

    // Місяць / магія: закляття, телепорти, холодні кристали
    Ramp moonMagic = { {
        Color{ 29, 58, 107, 255 },  // #1D3A6B
        Color{ 60, 111, 181, 255 }, // #3C6FB5
        Color{ 127, 176, 240, 255 },// #7FB0F0
        Color{ 191, 223, 255, 255 },// #BFDFFF
        Color{ 240, 248, 255, 255 } // #F0F8FF
    } };

    // Тканина / кров: плащ, плюмаж, кров ворогів, прапори
    Ramp bloodCloth = { {
        Color{ 58, 12, 20, 255 },   // #3A0C14
        Color{ 110, 21, 34, 255 },  // #6E1522
        Color{ 163, 32, 47, 255 },  // #A3202F
        Color{ 214, 64, 63, 255 },  // #D6403F
        Color{ 242, 122, 107, 255 } // #F27A6B
    } };

    // Кістка: скелети, черепи, останки
    Ramp bone = { {
        Color{ 74, 66, 54, 255 },   // #4A4236
        Color{ 140, 128, 104, 255 },// #8C8068
        Color{ 201, 191, 164, 255 },// #C9BFA4
        Color{ 239, 230, 204, 255 },// #EFE6CC
        Color{ 250, 243, 224, 255 } // #FAF3E0
    } };

    // Отруйний слиз / мох
    Ramp poisonSlime = { {
        Color{ 16, 36, 18, 255 },
        Color{ 28, 66, 32, 255 },
        Color{ 48, 115, 54, 255 },
        Color{ 85, 185, 88, 255 },
        Color{ 155, 240, 145, 255 }
    } };

    // 2. Єдиний майже-чорний контур сцени (чистий #000000 у сцені заборонений)
    Color outline = Color{ 12, 10, 18, 255 }; // #0C0A12

    // 3. Рампи для біомів
    BiomePalette castle = {
        stoneBg,
        stoneTile,
        moonMagic
    };

    BiomePalette garden = {
        // Зеленувато-смарагдовий моховитий камінь саду (Forest Abkhazia)
        { { Color{ 18, 28, 24, 255 }, Color{ 26, 42, 34, 255 }, Color{ 34, 56, 46, 255 }, Color{ 46, 74, 60, 255 }, Color{ 60, 96, 78, 255 } } },
        { { Color{ 48, 68, 56, 255 }, Color{ 78, 114, 92, 255 }, Color{ 102, 146, 118, 255 }, Color{ 126, 176, 144, 255 }, Color{ 168, 218, 184, 255 } } },
        poisonSlime
    };

    BiomePalette tower = {
        // Вивітрений теплий пісковик / золото вежі Майя (The Maya)
        { { Color{ 42, 34, 26, 255 }, Color{ 58, 46, 36, 255 }, Color{ 74, 58, 46, 255 }, Color{ 96, 76, 58, 255 }, Color{ 122, 98, 76, 255 } } },
        { { Color{ 82, 64, 46, 255 }, Color{ 134, 108, 78, 255 }, Color{ 168, 136, 98, 255 }, Color{ 198, 164, 122, 255 }, Color{ 236, 204, 160, 255 } } },
        fireGold
    };

    BiomePalette dungeon = {
        // Темні обсидіанові катакомби з контрастними фіолетовими гранями (Land of Darkness)
        { { Color{ 16, 14, 28, 255 }, Color{ 24, 20, 42, 255 }, Color{ 34, 28, 58, 255 }, Color{ 46, 38, 78, 255 }, Color{ 62, 52, 102, 255 } } },
        { { Color{ 44, 36, 64, 255 }, Color{ 74, 62, 108, 255 }, Color{ 98, 82, 140, 255 }, Color{ 122, 104, 170, 255 }, Color{ 164, 144, 214, 255 } } },
        { { Color{ 64, 10, 10, 255 }, Color{ 117, 21, 21, 255 }, Color{ 181, 43, 24, 255 }, Color{ 240, 85, 36, 255 }, Color{ 255, 155, 66, 255 } } }
    };

    const BiomePalette& GetBiome(BiomeType b) const {
        switch (b) {
            case BiomeType::ForestAbkhazia: return garden;
            case BiomeType::TheMaya:        return tower;
            case BiomeType::LandOfDarkness: return dungeon;
            case BiomeType::CastleHamson:
            default:                        return castle;
        }
    }

    // Тіні та відблиски
    Color Shadow(Color c, float factor = 0.65f) const {
        return Color{
            static_cast<unsigned char>(c.r * factor * 0.85f),
            static_cast<unsigned char>(c.g * factor * 0.90f),
            static_cast<unsigned char>(std::min(255.0f, c.b * factor * 1.15f)),
            c.a
        };
    }

    Color Highlight(Color c, float factor = 1.35f) const {
        return Color{
            static_cast<unsigned char>(std::min(255.0f, c.r * factor * 1.10f)),
            static_cast<unsigned char>(std::min(255.0f, c.g * factor * 1.05f)),
            static_cast<unsigned char>(c.b * factor * 0.90f),
            c.a
        };
    }
};

inline const Palette& GetPalette() {
    static const Palette instance{};
    return instance;
}
