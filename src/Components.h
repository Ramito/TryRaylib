#pragma once

#include<stdint.h>
#include "raylib.h"

struct SteerComponent {
	float Steer;
};

struct GunComponent {
	float TimeBeforeNextShot;
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

struct ParticleComponent {
	uint32_t LifeTime;
};