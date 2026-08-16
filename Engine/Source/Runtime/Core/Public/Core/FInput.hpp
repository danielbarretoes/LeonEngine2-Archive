#pragma once

#include "Core/Base.hpp"
#include <string>
#include <utility>

namespace Leon {

    // Common Key Codes (aligned with standard GLFW codes)
    namespace Key {
        enum : int {
            Space = 32,
            Apostrophe = 39,
            Comma = 44,
            Minus = 45,
            Period = 46,
            Slash = 47,
            D0 = 48,
            D1 = 49,
            D2 = 50,
            D3 = 51,
            D4 = 52,
            D5 = 53,
            D6 = 54,
            D7 = 55,
            D8 = 56,
            D9 = 57,
            A = 65,
            B = 66,
            C = 67,
            D = 68,
            E = 69,
            F = 70,
            G = 71,
            H = 72,
            I = 73,
            J = 74,
            K = 75,
            L = 76,
            M = 77,
            N = 78,
            O = 79,
            P = 80,
            Q = 81,
            R = 82,
            S = 83,
            T = 84,
            U = 85,
            V = 86,
            W = 87,
            X = 88,
            Y = 89,
            Z = 90,
            Escape = 256,
            Enter = 257,
            Tab = 258,
            Backspace = 259,
            Insert = 260,
            Delete = 261,
            Right = 262,
            Left = 263,
            Down = 264,
            Up = 265,
            PageUp = 266,
            PageDown = 267,
            Home = 268,
            End = 269,
            CapsLock = 280,
            ScrollLock = 281,
            NumLock = 282,
            PrintScreen = 283,
            Pause = 284,
            F1 = 290,
            F2 = 291,
            F3 = 292,
            F4 = 293,
            F5 = 294,
            F6 = 295,
            F7 = 296,
            F8 = 297,
            F9 = 298,
            F10 = 299,
            F11 = 300,
            F12 = 301,
            LeftShift = 340,
            LeftControl = 341,
            LeftAlt = 342,
            LeftSuper = 343,
            RightShift = 344,
            RightControl = 345,
            RightAlt = 346,
            RightSuper = 347
        };
    } // namespace Key

    namespace Mouse {
        enum : int {
            Button0 = 0,
            Button1 = 1,
            Button2 = 2,
            Button3 = 3,
            Button4 = 4,
            Button5 = 5,
            ButtonLeft = Button0,
            ButtonRight = Button1,
            ButtonMiddle = Button2
        };
    } // namespace Mouse

    namespace GamepadButton {
        enum : int {
            A = 0,
            B = 1,
            X = 2,
            Y = 3,
            LeftBumper = 4,
            RightBumper = 5,
            Back = 6,
            Start = 7,
            Guide = 8,
            LeftThumb = 9,
            RightThumb = 10,
            DPadUp = 11,
            DPadRight = 12,
            DPadDown = 13,
            DPadLeft = 14
        };
    } // namespace GamepadButton

    namespace GamepadAxis {
        enum : int { LeftX = 0, LeftY = 1, RightX = 2, RightY = 3, LeftTrigger = 4, RightTrigger = 5 };
    } // namespace GamepadAxis

    class FInput {
    public:
        // Keyboard & Mouse Queries
        static bool IsKeyPressed(int InKeyCode);
        static bool IsMouseButtonPressed(int InButton);
        static std::pair<float, float> GetMousePosition();
        static float GetMouseX();
        static float GetMouseY();

        // Gamepad / Xbox Controller Queries
        static bool IsGamepadConnected(int InGamepadID = 0);
        static std::string GetGamepadName(int InGamepadID = 0);
        static bool IsGamepadButtonPressed(int InButton, int InGamepadID = 0);
        static float GetGamepadAxis(int InAxis, int InGamepadID = 0, float InDeadzone = 0.15f);
        static std::pair<float, float> GetGamepadLeftStick(int InGamepadID = 0, float InDeadzone = 0.15f);
        static std::pair<float, float> GetGamepadRightStick(int InGamepadID = 0, float InDeadzone = 0.15f);
    };

} // namespace Leon
