#include "System/KeyDecoder.h"
#include <iostream>
#include <stdexcept>
#include <string>

using MyGame::KeyDecoder;

void Check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

std::string Decode(KeyDecoder& decoder, const std::string& input)
{
    std::string out;
    for (unsigned char byte : input)
    {
        char key;
        if (decoder.Feed(byte, key)) out += key;
    }
    return out;
}

int main()
{
    try
    {
        KeyDecoder decoder;
        Check(Decode(decoder, "wAsDf pq\r") == "wAsDf pq\r", "WASD, precision, fire and menu keys must pass through");
        Check(Decode(decoder, "\033[A\033[B\033[C\033[D") == "wsda", "ANSI arrows must map to matching directions");
        Check(Decode(decoder, "\033OA\033OB\033OC\033OD") == "wsda", "Application-mode arrows must also work");
        Check(Decode(decoder, "\033[1;2A\033[1;2B\033[1;2C\033[1;2D") == "WSDA", "Shift arrows must preserve precision in ANSI terminals");
        std::cout << "PASS standard arrows, application arrows, Shift arrows, and existing controls\n";

        Check(Decode(decoder, "\033").empty(), "Escape prefix must not generate a movement");
        Check(Decode(decoder, "[").empty(), "Incomplete CSI must wait for the final byte");
        Check(Decode(decoder, "D") == "a", "Split left-arrow sequence must move left, not right");
        Check(Decode(decoder, "\033O").empty() && Decode(decoder, "A") == "w", "Split application arrow must decode once");
        Check(Decode(decoder, "\033[1;").empty() && Decode(decoder, "2C") == "D", "Split Shift arrow must retain its modifier");
        std::cout << "PASS sequences split across reads\n";

        Check(Decode(decoder, "\033[3~\033[H\033[15~\033[1;5D").empty(), "Unsupported escape keys must not leak game commands");
        Check(Decode(decoder, "\033[" + std::string(100, '1') + "Dw") == "w", "Oversized sequence must be discarded without losing the next normal key");
        Check(Decode(decoder, "\033[1;\033[Bq") == "sq", "A new Escape must recover from an incomplete sequence");
        Check(Decode(decoder, "\033q") == "q", "An unrelated key after Escape must still work");
        std::cout << "PASS ignored sequences and decoder recovery\n";

        const unsigned char codes[] = {72, 80, 77, 75};
        const char expected[] = {'w', 's', 'd', 'a'};
        for (int i = 0; i < 4; ++i)
        {
            char key = 0;
            Check(KeyDecoder::WindowsArrow(codes[i], key) && key == expected[i], "Windows scan codes must map to matching directions");
        }
        char unused;
        Check(!KeyDecoder::WindowsArrow(59, unused), "Other Windows special keys must be ignored");
        std::cout << "PASS Windows arrow scan codes\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
