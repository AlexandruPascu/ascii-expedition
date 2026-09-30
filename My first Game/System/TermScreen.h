#pragma once

#include <string>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace MyGame
{
    enum class Color { Default, Dim, Cyan, Yellow, Red, Green, Magenta, Blue };
    struct TerminalSize
    {
        int columns, rows;
        bool FitsGame() const { return columns >= 80 && rows >= 37; }
    };
    class Shape;
    class TerminalScreen
    {
        std::vector<std::string> lines;
        std::vector<std::vector<Color>> colors;
#if defined(_WIN32)
        HANDLE output;
        DWORD originalMode;
#endif
    public:
        TerminalScreen();
        ~TerminalScreen();
        TerminalScreen(const TerminalScreen&) = delete;
        TerminalScreen& operator=(const TerminalScreen&) = delete;
        TerminalSize Size() const;
        void ResizeNotice(TerminalSize size);
        void Clear();
        void Draw(const Shape& shape, int x, int y, Color color = Color::Default);
        void Text(int x, int y, const std::string& text, Color color = Color::Default);
        void Present();
    };
}
