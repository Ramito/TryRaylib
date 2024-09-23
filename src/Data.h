#pragma once

#include <array>
#include <stdint.h>

#include "raylib.h"

struct GameInput
{
    float Forward; // Direction is camera relative
    float Left;
    bool Fire;
};

constexpr Vector3 Forward3 = {0.f, 0.f, 1.f};
constexpr Vector3 Back3 = {0.f, 0.f, -1.f};
constexpr Vector3 Left3 = {1.f, 0.f, 0.f};
constexpr Vector3 Up3 = {0.f, 1.f, 0.f};

namespace SimTimeData {
constexpr uint32_t TargetFPS = 60;
constexpr float DeltaTime = 1.f / TargetFPS;
} // namespace SimTimeData

namespace RespawnData {
constexpr float Timer = 3.f;
constexpr float MarkerRadius = 1.5f;
constexpr float MarkerFrequency = 6.5f;
constexpr float MarkerMoveSpeed = 15.f;
} // namespace RespawnData

namespace CameraData {
constexpr Vector3 TargetOffset = {-3.5f, 0.f, -3.5f};
constexpr Vector3 CameraOffset = {-11.f, 42.f, -11.f};
} // namespace CameraData

namespace SpaceData {
constexpr uint32_t AsteroidsCount = 100;
constexpr float MinAsteroidRadius = 1.0f;
constexpr float MaxAsteroidRadius = 4.5f;
constexpr float AsteroidDriftSpeed = 1.25f;
constexpr float AsteroidBounce = 0.975f;
constexpr float RelativeAsteroidDensity = 8.f;
constexpr float LengthX = 250.f;
constexpr float LengthZ = 250.f;
constexpr int CellCountX = 25;
constexpr int CellCountZ = 25;
} // namespace SpaceData

namespace SpaceshipData {
constexpr float MinThrust = 8.f;
constexpr float Thrust = 15.f;
constexpr float LinearDrag = 1e-5;
constexpr float QuadraticDrag = 1e-3;

constexpr float Yaw = 0.4f;
constexpr float Pitch = 2.5f;
constexpr float Roll = 1.5f;

constexpr float NegativePitch = 0.5f;
constexpr float NegativeRoll = 2.75f;

constexpr float SpecialRoll = 1.75f * SpaceshipData::NegativeRoll;
constexpr float SpecialRollPitchMultiplier = 2.5f;

constexpr float SteerB = 0.225f;
constexpr float SteerM = (0.8f * PI - SteerB) * 0.5f;

constexpr float CollisionRadius = 0.75f;
constexpr float ParticleCollisionRadius = 1.25f;

constexpr float AngularMomentumTransfer = 0.745f;
constexpr float AngularMomentumDrag = 0.975f;
constexpr float LethalImpactSq = 120.f;
} // namespace SpaceshipData

namespace WeaponData {
constexpr float RateOfFire = 0.1f;
constexpr float BulletSpeed = 70.5f;
constexpr float BulletLifetime = 0.5f;
constexpr std::array<Vector3, 4> ShootBones = {Vector3{0.6f, 0.25f, 0.f}, Vector3{-0.6f, 0.25f, 0.f},
                                               Vector3{0.6f, -0.25f, 0.f}, Vector3{-0.6f, -0.25f, 0.f}};
} // namespace WeaponData

namespace ParticleData {
constexpr float LinearDrag = 0.f;
constexpr float QuadraticDrag = 1e-2;
} // namespace ParticleData

namespace ExplosionData {
constexpr float Time = 0.2f;
constexpr float SpaceshipRadius = 3.5f;
constexpr float AsteroidMultiplier = 1.2f;
constexpr float ParticleForce = 150.f;
} // namespace ExplosionData