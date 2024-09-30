#pragma once

#include <stdint.h>

#include "Data.h"
#include "entt/entt.hpp"
#include "raylib.h"

struct SteerComponent
{
    float Steer;
};

struct GunComponent
{
    float TimeSinceLastShot;
    uint32_t NextShotBone;
};

struct ThrustComponent
{
    float Thrust;
};

struct PositionComponent
{
    Vector3 Position;
};

struct OrientationComponent
{
    Quaternion Rotation;
};

struct VelocityComponent
{
    Vector3 Velocity;
};

struct AngularComponent
{
    float YawMomentum;
};

struct SpecialManeuver
{
    float SideProgress;
    float SideMomentum;
};

struct SpaceshipInputComponent
{
    uint32_t InputId;
    GameInput Input;
};

struct AsteroidComponent
{
    float Radius;
};

struct ParticleComponent
{
    float LifeTime;
    Color Color;
};

struct BulletComponent
{};

struct ParticleDragComponent
{};

struct DestroyComponent
{};

struct BulletHitComponent
{
    float HitCos;
};

struct RespawnComponent
{
    uint32_t InputId;
    float TimeLeft;
    bool Primed = false;
};

struct ParticleCollisionComponent
{
    Vector3 ImpactNormal;
    float NormalContactSpeed;
    entt::entity Collider;
};

struct ExplosionComponent
{
    float StartTime;
    float CurrentRadius;
    float TerminalRadius;
};
