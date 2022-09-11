#pragma once

#include "Data.h"
namespace SpaceUtil {
	inline float FindCoordinateGap(float coord1, float coord2, float mod) {
		float coordGap = coord2 - coord1;
		if (abs(coordGap + mod) < abs(coordGap)) {
			return coordGap + mod;
		}
		else if (abs(coordGap - mod) < abs(coordGap)) {
			return coordGap - mod;
		}
		return coordGap;
	};

	inline Vector3 FindVectorGap(const Vector3& from, const Vector3& to) {
		const float x1 = from.x;
		const float x2 = to.x;
		const float gapX = FindCoordinateGap(x1, x2, SpaceData::LengthX);

		const float z1 = from.z;
		const float z2 = to.z;
		const float gapZ = FindCoordinateGap(z1, z2, SpaceData::LengthZ);

		return Vector3{ gapX, to.y - from.y, gapZ };
	};
}