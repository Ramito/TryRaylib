#pragma once

#include<stdint.h>

#include "Data.h"
#include "raylib.h"

struct SteerComponent {
	float Steer;
};

struct GunComponent {
	float TimeSinceLastShot;
	uint32_t NextShotBone;
};

struct ThrustComponent {
	float Thrust;
};

struct PositionComponent {
	Vector3 Position;
};

struct OrientationComponent {
	Quaternion Quaternion;
};

struct VelocityComponent {
	Vector3 Velocity;
};

struct SpaceshipInputComponent {
	uint32_t InputId;
	GameInput Input;
};

struct AsteroidComponent {
	float Radius;
};

struct ParticleComponent {
	float LifeTime;
};

struct BulletComponent {};

struct ParticleDragComponent {};

struct DestroyComponent {};

struct HitAsteroidComponent {
	float HitCos;
};

struct RespawnComponent {
	uint32_t InputId;
	float TimeLeft;
};

struct ParticleCollisionComponent {
	Vector3 ImpactNormal;
	float ContactSpeed;
	entt::entity Collider;
};

struct ExplosionComponent {
	float StartTime;
	float Radius;
};
