#pragma once

#include <stdexcept>

namespace MyGame
{
	using Pixel = char;

	class PixelMatrix
	{
		int width = 0, height = 0;
		Pixel pixels[100][100] = {};

		static void Check(int x, int y)
		{
			if (x < 0 || x >= 100 || y < 0 || y >= 100) throw std::out_of_range("Pixel coordinates");
		}

	public:
		int GetWidth() const { return width; }
		int GetHeight() const { return height; }
		Pixel GetPixelAt(int x, int y) const { Check(x, y); return pixels[x][y]; }

		void SetWidth(int w) { if (w < 0 || w > 100) throw std::out_of_range("Pixel width"); width = w; }
		void SetHeight(int h) { if (h < 0 || h > 100) throw std::out_of_range("Pixel height"); height = h; }
		void SetPixelAt(int x, int y, Pixel p) { Check(x, y); pixels[x][y] = p; }
	};
}
