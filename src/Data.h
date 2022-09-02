#pragma once

#include <array>
#include <stdint.h>

#include "raylib.h"

struct GameInput {
	float Forward;	// Direction is camera relative
	float Left;
	bool Fire;
};

constexpr Vector3 Forward3 = { 0.f, 0.f, 1.f };
constexpr Vector3 Back3 = { 0.f, 0.f, -1.f };
constexpr Vector3 Left3 = { 1.f, 0.f, 0.f };
constexpr Vector3 Up3 = { 0.f, 1.f, 0.f };

namespace SimTimeData {
	constexpr uint32_t TargetFPS = 120;
	constexpr float DeltaTime = 1.f / TargetFPS;
}

namespace SpaceData {
	constexpr uint32_t AsteroidsCount = 250;
	constexpr float MinAsteroidRadius = 1.25f;
	constexpr float MaxAsteroidRadius = 4.5f;
	constexpr float AsteroidDriftSpeed = 3.f;
	constexpr float LengthX = 400.f;
	constexpr float LengthZ = 200.f;
	constexpr int CellCountX = 120;
	constexpr int CellCountZ = 40;
}

namespace SpaceshipData {
	constexpr float MinThrust = 10.f;
	constexpr float Thrust = 15.f;
	constexpr float LinearDrag = 1e-5;
	constexpr float QuadraticDrag = 1e-3;

	constexpr float Yaw = 0.4f;
	constexpr float Pitch = 2.5f;
	constexpr float Roll = 1.5f;

	constexpr float NegativePitch = 1.25f;
	constexpr float NegativeRoll = 2.75f;

	constexpr float SteerB = 0.25f;
	constexpr float SteerM = 1.5f;
}

namespace WeaponData {
	constexpr float RateOfFire = 0.125f;
	constexpr float BulletSpeed = 75.f;
	constexpr float BulletLifetime = 1.f;
	constexpr std::array<Vector3, 2> ShootBones = { Vector3 { 0.65f, 0.f, 0.f }, Vector3 { -0.65f, 0.f, 0.f } };
}

namespace ParticleData {
	constexpr float LinearDrag = 0.f;
	constexpr float QuadraticDrag = 1e-2;
}