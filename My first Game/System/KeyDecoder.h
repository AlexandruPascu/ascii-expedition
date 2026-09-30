#pragma once

#include <string>

namespace MyGame
{
    // Escape sequences can arrive over several frames; emit one command per complete key.
    class KeyDecoder
    {
        enum class State { Plain, Escape, CSI, SS3 };
        State state = State::Plain;
        std::string parameters;
        bool discard = false;

        static bool Arrow(unsigned char final, bool precise, char& key)
        {
            switch (final)
            {
                case 'A': key = precise ? 'W' : 'w'; return true;
                case 'B': key = precise ? 'S' : 's'; return true;
                case 'C': key = precise ? 'D' : 'd'; return true;
                case 'D': key = precise ? 'A' : 'a'; return true;
                default: return false;
            }
        }

    public:
        bool Feed(unsigned char byte, char& key)
        {
            if (byte == 27)
            {
                state = State::Escape;
                parameters.clear();
                discard = false;
                return false;
            }
            if (state == State::Escape)
            {
                if (byte == '[') { state = State::CSI; return false; }
                if (byte == 'O') { state = State::SS3; return false; }
                state = State::Plain; // An unrelated key after Escape still works.
            }
            if (state == State::SS3)
            {
                state = State::Plain;
                return Arrow(byte, false, key);
            }
            if (state == State::CSI)
            {
                if (byte >= 0x40 && byte <= 0x7e)
                {
                    state = State::Plain;
                    const bool supported = parameters.empty() || parameters == "1" || parameters == "1;2";
                    return !discard && supported && Arrow(byte, parameters == "1;2", key);
                }
                if (byte >= 0x20 && byte <= 0x3f)
                {
                    if (parameters.size() < 16) parameters += static_cast<char>(byte);
                    else discard = true;
                }
                else { state = State::Plain; parameters.clear(); }
                return false;
            }
            key = static_cast<char>(byte);
            return true;
        }

        static bool WindowsArrow(unsigned char scanCode, char& key)
        {
            switch (scanCode)
            {
                case 72: key = 'w'; return true;
                case 80: key = 's'; return true;
                case 77: key = 'd'; return true;
                case 75: key = 'a'; return true;
                default: return false;
            }
        }
    };
}
