#pragma once

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
            Right = 262,
            Left = 263,
            Down = 264,
            Up = 265
        };
    }

    namespace Mouse {
        enum : int {
            Button0 = 0,
            Button1 = 1,
            Button2 = 2,
            Button3 = 3,
            ButtonLeft = Button0,
            ButtonRight = Button1,
            ButtonMiddle = Button2
        };
    }

} // namespace Leon
