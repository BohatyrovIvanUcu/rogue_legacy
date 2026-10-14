// === FILE: src/gfx/UiText.hpp ===
#pragma once
#include <raylib.h>
#include <string>

// Єдина точка малювання тексту гри.
// Вбудований шрифт raylib — 10-піксельний bitmap, який на розмірах 13/15/17 px
// масштабується нерівномірно й виглядає "рваним". Тут ми підвантажуємо системний
// векторний шрифт (з фолбеком на вбудований) і додаємо перенесення рядків.
namespace Ui {
    void Init();
    void Shutdown();

    void Text(const char* text, int x, int y, int fontSize, Color color);
    inline void Text(const std::string& text, int x, int y, int fontSize, Color color) {
        Text(text.c_str(), x, y, fontSize, color);
    }
    inline void Text(const char* text, float x, float y, int fontSize, Color color) {
        Text(text, static_cast<int>(x), static_cast<int>(y), fontSize, color);
    }
    inline void Text(const std::string& text, float x, float y, int fontSize, Color color) {
        Text(text.c_str(), static_cast<int>(x), static_cast<int>(y), fontSize, color);
    }

    int  Measure(const char* text, int fontSize);
    inline int Measure(const std::string& text, int fontSize) {
        return Measure(text.c_str(), fontSize);
    }

    // Малює текст з перенесенням по словах у межах maxWidth. Повертає висоту блоку в пікселях.
    int  TextWrapped(const char* text, int x, int y, int maxWidth, int fontSize, Color color);
    inline int TextWrapped(const std::string& text, int x, int y, int maxWidth, int fontSize, Color color) {
        return TextWrapped(text.c_str(), x, y, maxWidth, fontSize, color);
    }
    inline int TextWrapped(const char* text, float x, float y, int maxWidth, int fontSize, Color color) {
        return TextWrapped(text, static_cast<int>(x), static_cast<int>(y), maxWidth, fontSize, color);
    }
    inline int TextWrapped(const std::string& text, float x, float y, int maxWidth, int fontSize, Color color) {
        return TextWrapped(text.c_str(), static_cast<int>(x), static_cast<int>(y), maxWidth, fontSize, color);
    }
}


