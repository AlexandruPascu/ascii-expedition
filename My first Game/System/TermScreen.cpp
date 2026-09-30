#include "TermScreen.h"
#include "Geometry/PixelMatrix.h"
#include "Geometry/Shape.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#if !defined(_WIN32)
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace MyGame
{
    TerminalScreen::TerminalScreen() : lines(37, std::string(80, ' ')), colors(37, std::vector<Color>(80, Color::Default))
    {
#if defined(_WIN32)
        output = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!GetConsoleMode(output, &originalMode) ||
            !SetConsoleMode(output, originalMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            throw std::runtime_error("Use a terminal that supports ANSI escape sequences.");
#endif
        std::cout << "\033[?1049h\033[?25l\033[2J";
    }

    TerminalScreen::~TerminalScreen()
    {
        std::cout << "\033[0m\033[?25h\033[?1049l" << std::flush;
#if defined(_WIN32)
        SetConsoleMode(output, originalMode);
#endif
    }

    TerminalSize TerminalScreen::Size() const
    {
#if defined(_WIN32)
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(output, &info))
            return {info.srWindow.Right - info.srWindow.Left + 1,
                    info.srWindow.Bottom - info.srWindow.Top + 1};
#else
        winsize size{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col && size.ws_row)
            return {size.ws_col, size.ws_row};
#endif
        return {80, 37}; // Keep terminals without size reporting usable.
    }

    void TerminalScreen::ResizeNotice(TerminalSize size)
    {
        const std::string messages[] = {
            "WINDOW TOO SMALL",
            "Need 80 columns x 37 rows. Current: " + std::to_string(size.columns) + " x " + std::to_string(size.rows),
            "Resize to continue. Play stays paused; P resumes.",
            "Q: quit"
        };
        std::string frame = "\033[0m\033[2J";
        for (int row = 0; row < std::min(size.rows, 4); ++row)
        {
            frame += "\033[" + std::to_string(row + 1) + ";1H";
            // Leave the final column empty so even a tiny terminal cannot wrap.
            frame += messages[row].substr(0, static_cast<std::size_t>(std::max(0, size.columns - 1)));
        }
        std::cout << frame << std::flush;
    }

    void TerminalScreen::Clear()
    {
        for (std::string& line : lines) line.assign(80, ' ');
        for (auto& row : colors) std::fill(row.begin(), row.end(), Color::Default);
    }

    void TerminalScreen::Text(int x, int y, const std::string& text, Color color)
    {
        if (y < 0 || y >= static_cast<int>(lines.size())) return;
        for (char c : text)
        {
            if (x >= 0 && x < 80) { lines[y][x] = c; colors[y][x] = color; }
            ++x;
        }
    }

    void TerminalScreen::Draw(const Shape& shape, int x, int y, Color color)
    {
        PixelMatrix pixels;
        shape.FillPixels(pixels);
        for (int row = 0; row < pixels.GetHeight(); ++row)
        {
            std::string line;
            for (int col = 0; col < pixels.GetWidth(); ++col) line += pixels.GetPixelAt(col, row);
            Text(x, y + row, line, color);
        }
    }

    void TerminalScreen::Present()
    {
        const char* palette[] = {"\033[0m", "\033[90m", "\033[96m", "\033[93m", "\033[91m", "\033[92m", "\033[95m", "\033[94m"};
        Color active = Color::Default;
        std::string frame = palette[0];
        for (std::size_t row = 0; row < lines.size(); ++row)
        {
            frame += "\033[" + std::to_string(row + 1) + ";1H";
            for (std::size_t col = 0; col < lines[row].size(); ++col)
            {
                if (active != colors[row][col])
                {
                    active = colors[row][col];
                    frame += palette[static_cast<int>(active)];
                }
                frame += lines[row][col];
            }
        }
        std::cout << frame << "\033[0m" << std::flush;
    }
}
