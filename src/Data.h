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

namespace GameData {
	constexpr float RespawnTimer = 2.f;
}

namespace SpaceData {
	constexpr uint32_t AsteroidsCount = 25;
	constexpr float MinAsteroidRadius = 1.0f;
	constexpr float MaxAsteroidRadius = 5.0f;
	constexpr float AsteroidDriftSpeed = 1.25f;
	constexpr float LengthX = 250.f;
	constexpr float LengthZ = 250.f;
	constexpr int CellCountX = 25;
	constexpr int CellCountZ = 25;
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

	constexpr float SteerB = 0.225f;
	constexpr float SteerM = 1.45f;

	constexpr float CollisionRadius = 0.75f;
	constexpr float ParticleCollisionRadius = 1.25f;
}

namespace WeaponData {
	constexpr float RateOfFire = 0.1f;
	constexpr float BulletSpeed = 70.5f;
	constexpr float BulletLifetime = 0.5f;
	constexpr std::array<Vector3, 4> ShootBones = { Vector3 { 0.6f, 0.25f, 0.f }, Vector3 { -0.6f, 0.25f, 0.f }, Vector3 { 0.6f, -0.25f, 0.f }, Vector3 { -0.6f, -0.25f, 0.f } };
}

namespace ParticleData {
	constexpr float LinearDrag = 0.f;
	constexpr float QuadraticDrag = 1e-2;
}

namespace ExplosionData {
	constexpr float Time = 0.2f;
	constexpr float SpaceshipRadius = 3.5f;
	constexpr float AsteroidMultiplier = 1.f;
	constexpr float ParticleForce = 150.f;
}