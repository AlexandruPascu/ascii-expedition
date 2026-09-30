#pragma once

#include "KeyDecoder.h"
#include <stdexcept>
#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#else
#include <termios.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace MyGame
{
    // Configure the terminal once and restore the exact original state on exit.
    class KeyboardInput
    {
        KeyDecoder decoder;
#if defined(_WIN32)
        bool extendedKey = false;
#else
        termios original;
#endif
    public:
        KeyboardInput()
        {
#if defined(_WIN32)
            if (!_isatty(0) || !_isatty(1)) throw std::runtime_error("Run this game in an interactive terminal.");
#else
            if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || tcgetattr(STDIN_FILENO, &original) < 0)
                throw std::runtime_error("Run this game in an interactive terminal.");
            termios raw = original;
            raw.c_lflag &= ~(ICANON | ECHO);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0)
                throw std::runtime_error("Could not configure terminal input.");
#endif
        }
        ~KeyboardInput()
        {
#if !defined(_WIN32)
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
#endif
        }
        KeyboardInput(const KeyboardInput&) = delete;
        KeyboardInput& operator=(const KeyboardInput&) = delete;

        bool Read(char& key)
        {
            // Bound raw byte processing as well as the main loop's command count.
            for (int count = 0; count < 32; ++count)
            {
                unsigned char byte;
#if defined(_WIN32)
                if (!_kbhit()) return false;
                byte = static_cast<unsigned char>(_getch());
                if (extendedKey)
                {
                    extendedKey = false;
                    if (KeyDecoder::WindowsArrow(byte, key)) return true;
                    continue;
                }
                if (byte == 0 || byte == 224) { extendedKey = true; continue; }
#else
                const ssize_t countRead = read(STDIN_FILENO, &byte, 1);
                if (countRead < 0 && errno != EINTR && errno != EAGAIN)
                    throw std::runtime_error("Could not read terminal input.");
                if (countRead <= 0) return false;
#endif
                if (decoder.Feed(byte, key)) return true;
            }
            return false;
        }
    };
}
