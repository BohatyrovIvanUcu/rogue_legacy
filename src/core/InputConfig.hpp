// === FILE: src/core/InputConfig.hpp ===
#pragma once
#include <raylib.h>
#include <string>
#include <array>

enum class InputAction : int {
    MoveLeft = 0,
    MoveRight,
    MoveUp,
    MoveDown,
    Jump,
    Attack,
    CastSpell,
    ClassTalent,
    Dash,
    Interact,
    OpenMap,
    COUNT
};

inline constexpr int MOUSE_CODE_OFFSET = 1000;

struct ActionBinding {
    int primary = 0;
    int secondary = 0;
};

class InputConfig {
public:
    std::array<ActionBinding, static_cast<size_t>(InputAction::COUNT)> bindings{};

    InputConfig() {
        SetDefaults();
    }

    void SetDefaults() {
        Set(InputAction::MoveLeft, KEY_A, KEY_LEFT);
        Set(InputAction::MoveRight, KEY_D, KEY_RIGHT);
        Set(InputAction::MoveUp, KEY_W, KEY_UP);
        Set(InputAction::MoveDown, KEY_S, KEY_DOWN);
        Set(InputAction::Jump, KEY_SPACE, KEY_NULL);
        Set(InputAction::Attack, KEY_J, MOUSE_CODE_OFFSET + MOUSE_BUTTON_LEFT);
        Set(InputAction::CastSpell, KEY_L, MOUSE_CODE_OFFSET + MOUSE_BUTTON_RIGHT);
        Set(InputAction::ClassTalent, KEY_Q, KEY_LEFT_CONTROL);
        Set(InputAction::Dash, KEY_K, KEY_LEFT_SHIFT);
        Set(InputAction::Interact, KEY_E, KEY_W);
        Set(InputAction::OpenMap, KEY_M, KEY_TAB);
    }

    void Set(InputAction act, int prim, int sec = 0) {
        size_t idx = static_cast<size_t>(act);
        if (idx < bindings.size()) {
            bindings[idx].primary = prim;
            bindings[idx].secondary = sec;
        }
    }

    static bool IsKeyOrButtonDown(int code) {
        if (code <= 0) return false;
        if (code >= MOUSE_CODE_OFFSET) {
            return IsMouseButtonDown(code - MOUSE_CODE_OFFSET);
        }
        return IsKeyDown(code);
    }

    static bool IsKeyOrButtonPressed(int code) {
        if (code <= 0) return false;
        if (code >= MOUSE_CODE_OFFSET) {
            return IsMouseButtonPressed(code - MOUSE_CODE_OFFSET);
        }
        return IsKeyPressed(code);
    }

    static bool IsKeyOrButtonReleased(int code) {
        if (code <= 0) return false;
        if (code >= MOUSE_CODE_OFFSET) {
            return IsMouseButtonReleased(code - MOUSE_CODE_OFFSET);
        }
        return IsKeyReleased(code);
    }

    bool IsActionDown(InputAction act) const {
        size_t idx = static_cast<size_t>(act);
        if (idx >= bindings.size()) return false;
        return IsKeyOrButtonDown(bindings[idx].primary) || IsKeyOrButtonDown(bindings[idx].secondary);
    }

    bool IsActionPressed(InputAction act) const {
        size_t idx = static_cast<size_t>(act);
        if (idx >= bindings.size()) return false;
        return IsKeyOrButtonPressed(bindings[idx].primary) || IsKeyOrButtonPressed(bindings[idx].secondary);
    }

    bool IsActionReleased(InputAction act) const {
        size_t idx = static_cast<size_t>(act);
        if (idx >= bindings.size()) return false;
        return IsKeyOrButtonReleased(bindings[idx].primary) || IsKeyOrButtonReleased(bindings[idx].secondary);
    }

    static const char* GetActionName(InputAction act) {
        switch (act) {
            case InputAction::MoveLeft:    return "MOVE LEFT";
            case InputAction::MoveRight:   return "MOVE RIGHT";
            case InputAction::MoveUp:      return "AIM UP / CLIMB";
            case InputAction::MoveDown:    return "CROUCH / DOWN-STRIKE";
            case InputAction::Jump:        return "JUMP";
            case InputAction::Attack:      return "SWORD ATTACK";
            case InputAction::CastSpell:   return "CAST SPELL";
            case InputAction::ClassTalent: return "CLASS TALENT";
            case InputAction::Dash:        return "DASH";
            case InputAction::Interact:    return "INTERACT / ENTER";
            case InputAction::OpenMap:     return "MAP / TELEPORT";
            default:                       return "UNKNOWN";
        }
    }

    static std::string GetKeyDisplayName(int code) {
        if (code <= 0) return "---";
        if (code == MOUSE_CODE_OFFSET + MOUSE_BUTTON_LEFT)   return "LMB";
        if (code == MOUSE_CODE_OFFSET + MOUSE_BUTTON_RIGHT)  return "RMB";
        if (code == MOUSE_CODE_OFFSET + MOUSE_BUTTON_MIDDLE) return "MMB";
        if (code >= MOUSE_CODE_OFFSET) {
            return "M" + std::to_string(code - MOUSE_CODE_OFFSET);
        }

        switch (code) {
            case KEY_SPACE:         return "SPACE";
            case KEY_ESCAPE:        return "ESC";
            case KEY_ENTER:         return "ENTER";
            case KEY_TAB:           return "TAB";
            case KEY_BACKSPACE:     return "BKSP";
            case KEY_INSERT:        return "INS";
            case KEY_DELETE:        return "DEL";
            case KEY_RIGHT:         return "RIGHT";
            case KEY_LEFT:          return "LEFT";
            case KEY_DOWN:          return "DOWN";
            case KEY_UP:            return "UP";
            case KEY_PAGE_UP:       return "PGUP";
            case KEY_PAGE_DOWN:     return "PGDN";
            case KEY_HOME:          return "HOME";
            case KEY_END:           return "END";
            case KEY_CAPS_LOCK:     return "CAPS";
            case KEY_SCROLL_LOCK:   return "SCRLK";
            case KEY_NUM_LOCK:      return "NUMLK";
            case KEY_PRINT_SCREEN:  return "PRTSC";
            case KEY_PAUSE:         return "PAUSE";
            case KEY_LEFT_SHIFT:    return "L-SHIFT";
            case KEY_LEFT_CONTROL:  return "L-CTRL";
            case KEY_LEFT_ALT:      return "L-ALT";
            case KEY_LEFT_SUPER:    return "L-WIN";
            case KEY_RIGHT_SHIFT:   return "R-SHIFT";
            case KEY_RIGHT_CONTROL: return "R-CTRL";
            case KEY_RIGHT_ALT:     return "R-ALT";
            case KEY_RIGHT_SUPER:   return "R-WIN";
            default:
                if (code >= 32 && code <= 126) {
                    return std::string(1, static_cast<char>(code));
                }
                return "KEY_" + std::to_string(code);
        }
    }
};
