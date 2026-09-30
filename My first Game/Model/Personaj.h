#pragma once

#include "Geometry/Pinocchio.h"

namespace MyGame
{
	class Personaj
	{
	public:
		enum Orientare
		{
			SPRE_STANGA = 0,
			SPRE_DREAPTA
		};

		Personaj() : mPosX(0), mPosY(0), mOrientare(SPRE_STANGA) {}

		const Shape& GetShape() const
		{
			if (mOrientare == SPRE_STANGA) return mShapeLeft;
			else return mShapeRight;
		}

		int GetX() const { return mPosX; }
		int GetY() const { return mPosY; }

		void SetX(int x) { mPosX = x; }
		void SetY(int y) { mPosY = y; }

		void SetOrientare(Orientare orientare) { mOrientare = orientare; }

	private:

		PinocchioLeft mShapeLeft;
		PinocchioRight mShapeRight;
		int mPosX, mPosY;
		Orientare mOrientare;
	};
}
