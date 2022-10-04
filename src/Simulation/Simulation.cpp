#include "Simulation.h"
#include "Tracy.hpp"

#include "Components.h"
#include <raymath.h>

static std::uniform_real_distribution<float> UniformDistribution(0.f, 1.f);
static std::uniform_real_distribution<float> DirectionDistribution(0.f, 2.f * PI);

Simulation::Simulation(const SimDependencies& dependencies)
: mRegistry(dependencies.GetDependency<entt::registry>()),
  mGameInput(dependencies.GetDependency<std::remove_reference<decltype(mGameInput)>::type>())
{}

static void MakeAsteroid(entt::registry& registry, float radius, const Vector3 position, const Vector3 velocity)
{
    entt::entity asteroid = registry.create();
    registry.emplace<AsteroidComponent>(asteroid, radius);
    registry.emplace<PositionComponent>(asteroid, position);
    registry.emplace<VelocityComponent>(asteroid, velocity);
}

static void SpawnSpaceship(entt::registry& registry, uint32_t inputID)
{
    const float x = inputID * SpaceData::LengthX / 2.f;
    const float z = inputID * SpaceData::LengthZ / 2.f;
    entt::entity player = registry.create();
    registry.emplace<SpaceshipInputComponent>(player, inputID, GameInput{0.f, 0.f, false});
    registry.emplace<SteerComponent>(player, 0.f);
    registry.emplace<ThrustComponent>(player, 0.f);
    registry.emplace<PositionComponent>(player, x, 0.f, z);
    registry.emplace<VelocityComponent>(player, 0.f, 0.f, 0.f);
    registry.emplace<OrientationComponent>(player, QuaternionIdentity());
    registry.emplace<GunComponent>(player, 0.f, 0u);
}

void Simulation::Init(uint32_t players)
{
    mRegistry.clear();
    mRegistry.reserve(64000);

    for (uint32_t player = 0; player < players; ++player) {
        SpawnSpaceship(mRegistry, player);
    }

    std::uniform_real_distribution<float> xDistribution(0.f, SpaceData::LengthX);
    std::uniform_real_distribution<float> zDistribution(0.f, SpaceData::LengthZ);
    std::uniform_real_distribution<float> speedDistribution(0.f, 2.f * SpaceData::AsteroidDriftSpeed);
    std::uniform_real_distribution<float> radiusDistribution(SpaceData::MinAsteroidRadius,
                                                             SpaceData::MaxAsteroidRadius);
    for (int i = 0; i < SpaceData::AsteroidsCount; ++i) {
        float angle = DirectionDistribution(mRandomGenerator);
        float speed = speedDistribution(mRandomGenerator);
        MakeAsteroid(mRegistry, radiusDistribution(mRandomGenerator),
                     {xDistribution(mRandomGenerator), 0.f, zDistribution(mRandomGenerator)},
                     {cos(angle) * speed, 0.f, sin(angle) * speed});
    }

    mSpatialPartition.InitArea({SpaceData::LengthX, SpaceData::LengthZ}, SpaceData::CellCountX,
                               SpaceData::CellCountZ);
}

static Vector3 HorizontalOrthogonal(const Vector3& vector)
{
    return {-vector.z, vector.y, vector.x};
}

static void ProcessInput(entt::registry& registry, const std::array<GameInput, 2>& gameInput)
{
    for (SpaceshipInputComponent& inputComponent : registry.view<SpaceshipInputComponent>().storage()) {
        inputComponent.Input = gameInput[inputComponent.InputId];
    }
}

void Simulation::MakeExplosion(const Vector3& position, const Vector3& velocity, float radius)
{
    auto explosion = mRegistry.create();
    mRegistry.emplace<PositionComponent>(explosion, position);
    mRegistry.emplace<VelocityComponent>(explosion, velocity);
    mRegistry.emplace<ExplosionComponent>(explosion, GameTime, 0.f, radius);
    for (size_t i = 0; i < 500; ++i) {
        auto particle = mRegistry.create();
        std::normal_distribution normal(0.f, 1.f);
        float x = normal(mRandomGenerator);
        float y = normal(mRandomGenerator);
        float z = normal(mRandomGenerator);
        const Vector3 radial = Vector3Normalize({x, y, z});
        mRegistry.emplace<PositionComponent>(particle, Vector3Add(position, Vector3Scale(radial, 0.1f)));
        mRegistry.emplace<VelocityComponent>(particle,
                                             Vector3Add(velocity, Vector3Scale(radial, ExplosionData::ParticleForce)));
        mRegistry.emplace<ParticleComponent>(particle, (UniformDistribution(mRandomGenerator) +
                                                        UniformDistribution(mRandomGenerator)) *
                                                       14.f);
        mRegistry.emplace<ParticleDragComponent>(particle);
    }
};

template <typename TComponent>
static void CopyStorage(const entt::registry& source, entt::registry& target)
{
    auto view = source.view<TComponent>();
    if constexpr (std::is_empty<TComponent>()) {
        for (auto entity : view) {
            target.emplace<TComponent>(entity);
        }
    } else {
        auto process = [&](auto entity, const auto& component) {
            target.emplace<TComponent>(entity, component);
        };
        view.each(process);
    }
}

void Simulation::WriteRenderState(entt::registry& target) const
{
    ZoneScopedN("WriteRenderState");
    assert(target.empty());
    target.reserve(mRegistry.size());
    target.assign(mRegistry.data(), mRegistry.data() + mRegistry.size(), mRegistry.released());
    {
        ZoneScopedN("Positions");
        CopyStorage<PositionComponent>(mRegistry, target);
    }
    {
        ZoneScopedN("Orientations");
        CopyStorage<OrientationComponent>(mRegistry, target);
    }
    {
        ZoneScopedN("Explosions");
        CopyStorage<ExplosionComponent>(mRegistry, target);
    }
    {
        ZoneScopedN("Particles");
        CopyStorage<ParticleComponent>(mRegistry, target);
    }
    {
        ZoneScopedN("Bullets");
        CopyStorage<BulletComponent>(mRegistry, target);
    }
    {
        ZoneScopedN("Asteroids");
        CopyStorage<AsteroidComponent>(mRegistry, target);
    }
    {
        ZoneScopedN("Spaceships");
        CopyStorage<SpaceshipInputComponent>(mRegistry, target);
    }
}

void Simulation::Simulate()
{
    constexpr float deltaTime = SimTimeData::DeltaTime;

    auto destroyView = mRegistry.view<DestroyComponent>();
    mRegistry.destroy(destroyView.begin(), destroyView.end());

    for (auto respawner : mRegistry.view<RespawnComponent>()) {
        RespawnComponent& respawn = mRegistry.get<RespawnComponent>(respawner);
        respawn.TimeLeft -= deltaTime;
        if (respawn.TimeLeft > 0.f) {
            continue;
        }
        SpawnSpaceship(mRegistry, respawn.InputId);
        mRegistry.destroy(respawner);
    }

    auto explosionView = mRegistry.view<ExplosionComponent, PositionComponent>();
    auto explosionProcess = [&](auto explosion, ExplosionComponent& explosionComponent,
                                const PositionComponent& positionComponent) {
        const float elapsedTime = GameTime - explosionComponent.StartTime;
        if (elapsedTime >= ExplosionData::Time) {
            mRegistry.emplace<DestroyComponent>(explosion);
        }
        explosionComponent.CurrentRadius =
        cbrt(std::clamp(elapsedTime / ExplosionData::Time, 0.f, 1.f)) * explosionComponent.TerminalRadius;
    };
    explosionView.each(explosionProcess);

    auto playerView =
    mRegistry.view<VelocityComponent, OrientationComponent, SteerComponent, SpaceshipInputComponent, ThrustComponent>();
    auto playerProcess = [this](entt::entity entity, VelocityComponent& velocityComponent,
                                OrientationComponent& orientationComponent, SteerComponent& steerComponent,
                                const SpaceshipInputComponent& inputComponent, ThrustComponent& thrustComponent) {
        const GameInput& input = inputComponent.Input;
        Vector3 inputTarget = {input.Left, 0.f, input.Forward};
        float inputLength = Vector3Length(inputTarget);

        Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Rotation);

        Vector3 inputDirection = forward;
        if (!FloatEquals(inputLength, 0.f)) {
            inputDirection = Vector3Scale(inputTarget, 1.f / inputLength);
        }

        float thrust = SpaceshipData::MinThrust;
        float directionProjection = Vector3DotProduct(forward, inputTarget);
        if (directionProjection > 0.f) {
            thrust += SpaceshipData::Thrust * directionProjection;
        }

        thrustComponent.Thrust = thrust;

        Vector3 acceleration = Vector3Scale(forward, deltaTime * thrustComponent.Thrust);
        Vector3 velocity = Vector3Add(velocityComponent.Velocity, acceleration);

        float speed = Vector3Length(velocity);
        if (!FloatEquals(speed, 0.f)) {
            float drag = speed * SpaceshipData::LinearDrag + speed * speed * SpaceshipData::QuadraticDrag;
            velocity = Vector3Add(velocity, Vector3Scale(velocity, -drag / speed));
        }

        velocityComponent.Velocity = velocity;

        float steer = steerComponent.Steer;
        float steerSign = (steer >= 0.f) ? 1.f : -1.f;
        steer *= steerSign;

        float turnCos = Vector3DotProduct(forward, inputDirection);
        static const float minCos = cosf(std::max(SpaceshipData::Yaw, SpaceshipData::Pitch) * deltaTime);
        bool turn = minCos > turnCos;

        float turnDistance = std::clamp(1.f - turnCos, 0.f, 2.f);
        float targetSteer = SpaceshipData::SteerB + turnDistance * SpaceshipData::SteerM;

        float steeringSign = 1.f;
        if (Vector3DotProduct(forward, HorizontalOrthogonal(inputDirection)) < 0.f) {
            steeringSign = -1.f;
        }
        if (!turn) {
            steer -= SpaceshipData::NegativeRoll * deltaTime;
            steer = std::max(steer, 0.f);
        } else {
            if (steeringSign != steerSign || steer > targetSteer) {
                steer -= SpaceshipData::NegativeRoll * deltaTime;
            } else {
                steer += SpaceshipData::Roll * deltaTime;
                steer = std::min(steer, targetSteer);
            }
        }

        float turnAbility = cosf(steer) * SpaceshipData::Yaw;
        if (steeringSign == steerSign) {
            turnAbility += sinf(steer) * SpaceshipData::Pitch;
        } else {
            turnAbility += sinf(steer) * 0.25f * SpaceshipData::NegativePitch;
        }

        steer *= steerSign;
        turnAbility *= steeringSign;
        steerComponent.Steer = steer;

        Quaternion resultingQuaternion;
        if (turn) {
            Quaternion rollQuaternion = QuaternionFromAxisAngle(Forward3, -steer);

            float dot = Vector3DotProduct(Forward3, forward);
            Quaternion yawQuaternion;
            if (FloatEquals(dot, -1.f)) {
                yawQuaternion = yawQuaternion = QuaternionFromAxisAngle(Up3, PI);
            } else {
                yawQuaternion = QuaternionFromVector3ToVector3(Forward3, forward);
            }

            Quaternion turningQuaternion = QuaternionFromAxisAngle(Up3, turnAbility * deltaTime);

            resultingQuaternion = QuaternionMultiply(yawQuaternion, rollQuaternion);
            resultingQuaternion = QuaternionMultiply(turningQuaternion, resultingQuaternion);
        } else {
            Quaternion rollQuaternion = QuaternionFromAxisAngle(Forward3, -steer);

            float dot = Vector3DotProduct(Forward3, inputDirection);
            Quaternion yawQuaternion;
            if (FloatEquals(dot, -1.f)) {
                yawQuaternion = QuaternionFromAxisAngle(Up3, PI);
            } else {
                yawQuaternion = QuaternionFromVector3ToVector3(Forward3, inputDirection);
            }

            resultingQuaternion = QuaternionMultiply(yawQuaternion, rollQuaternion);
        }
        orientationComponent.Rotation = resultingQuaternion;
    };
    playerView.each(playerProcess);

    auto thrustView =
    mRegistry.view<ThrustComponent, PositionComponent, VelocityComponent, OrientationComponent>();
    auto thrustParticleProcess = [this](ThrustComponent& thrustComponent, const PositionComponent& positionComponent,
                                        const VelocityComponent& velocityComponent,
                                        const OrientationComponent& orientationComponent) {
        constexpr float ThrustModule = 25.f;
        constexpr float RandomModule = 2.5f;
        constexpr float Offset = 0.4f;
        constexpr uint32_t MinParticles = 1;
        constexpr uint32_t MaxParticles = 2;
        float relativeThrust = thrustComponent.Thrust / SpaceshipData::Thrust;
        uint32_t particles = MinParticles + relativeThrust * relativeThrust * MaxParticles;

        Vector3 baseVelocity = velocityComponent.Velocity;
        Vector3 back = Vector3RotateByQuaternion(Back3, orientationComponent.Rotation);
        baseVelocity =
        Vector3Add(baseVelocity, Vector3Scale(back, thrustComponent.Thrust * ThrustModule * deltaTime));

        while (particles-- > 0) {
            entt::entity particleEntity = mRegistry.create();
            mRegistry.emplace<ParticleDragComponent>(particleEntity);
            mRegistry.emplace<PositionComponent>(particleEntity, Vector3Add(positionComponent.Position,
                                                                            Vector3Scale(back, Offset)));
            std::normal_distribution normal(0.f, 1.f);
            float randX = normal(mRandomGenerator);
            float randY = normal(mRandomGenerator);
            float randZ = normal(mRandomGenerator);
            Vector3 randomVelocity = Vector3Scale({randX, randY, randZ}, RandomModule);
            mRegistry.emplace<VelocityComponent>(particleEntity, Vector3Add(baseVelocity, randomVelocity));
            float lifetime =
            14.f * (UniformDistribution(mRandomGenerator) + UniformDistribution(mRandomGenerator));
            mRegistry.emplace<ParticleComponent>(particleEntity, lifetime);
        }
    };
    thrustView.each(thrustParticleProcess);

    auto particleView = mRegistry.view<ParticleComponent>();
    auto particleLifetimeProcess = [this](entt::entity particle, ParticleComponent& particleComponent) {
        if (particleComponent.LifeTime <= 0.f) {
            mRegistry.emplace<DestroyComponent>(particle);
            return;
        }
        particleComponent.LifeTime -= deltaTime;
    };
    particleView.each(particleLifetimeProcess);

    auto particleDragView = mRegistry.view<ParticleDragComponent, VelocityComponent>();
    auto particleDragProcess = [](VelocityComponent& velocityComponent) {
        float speed = Vector3Length(velocityComponent.Velocity);
        if (!FloatEquals(speed, 0.f)) {
            float drag = speed * ParticleData::LinearDrag + speed * speed * ParticleData::QuadraticDrag;
            velocityComponent.Velocity =
            Vector3Add(velocityComponent.Velocity, Vector3Scale(velocityComponent.Velocity, -drag / speed));
        }
    };
    particleDragView.each(particleDragProcess);

    auto dynamicView = mRegistry.view<PositionComponent, VelocityComponent>();
    auto dynamicProcess = [](PositionComponent& positionComponent, const VelocityComponent& velocityComponent) {
        positionComponent.Position =
        Vector3Add(positionComponent.Position, Vector3Scale(velocityComponent.Velocity, deltaTime));
    };
    dynamicView.each(dynamicProcess);

    auto wrapView = mRegistry.view<PositionComponent>();
    auto wrapProcess = [](PositionComponent& positionComponent) {
        const float x = positionComponent.Position.x;
        const float z = positionComponent.Position.z;
        if (x < 0.f) {
            positionComponent.Position.x += SpaceData::LengthX;
        } else if (x > SpaceData::LengthX) {
            positionComponent.Position.x -= SpaceData::LengthX;
        }
        if (z < 0.f) {
            positionComponent.Position.z += SpaceData::LengthZ;
        } else if (z > SpaceData::LengthZ) {
            positionComponent.Position.z -= SpaceData::LengthZ;
        }
    };
    wrapView.each(wrapProcess);

    mSpatialPartition.Clear();

    {
        ZoneScopedN("Partition");
        auto asteroidView = mRegistry.view<PositionComponent, AsteroidComponent>();
        auto partitionAsteroids = [this](entt::entity asteroid, const PositionComponent& positionComponent,
                                         const AsteroidComponent& asteroidComponent) {
            const float radius = asteroidComponent.Radius;
            const Vector2 flatPosition = {positionComponent.Position.x, positionComponent.Position.z};
            const Vector2 min = {flatPosition.x - radius, flatPosition.y - radius};
            const Vector2 max = {flatPosition.x + radius, flatPosition.y + radius};
            mSpatialPartition.InsertDeferred({asteroid, radius}, min, max);
        };
        asteroidView.each(partitionAsteroids);

        auto playerCollisionView = mRegistry.view<PositionComponent, SpaceshipInputComponent>();
        for (auto spaceship : playerCollisionView) {
            const Vector3& position = mRegistry.get<PositionComponent>(spaceship).Position;
            const Vector2 flatPosition = {position.x, position.z};
            constexpr float radius =
            std::max(SpaceshipData::CollisionRadius, SpaceshipData::ParticleCollisionRadius);
            const Vector2 min = {flatPosition.x - radius, flatPosition.y - radius};
            const Vector2 max = {flatPosition.x + radius, flatPosition.y + radius};
            mSpatialPartition.InsertDeferred({spaceship, radius}, min, max);
        }
    }

    {
        ZoneScopedN("FlushInsertions");
        mSpatialPartition.FlushInsertions();
    }

    auto findCoordinateGap = [](float coord1, float coord2, float mod) {
        float coordGap = coord2 - coord1;
        if (abs(coordGap + mod) < abs(coordGap)) {
            return coordGap + mod;
        } else if (abs(coordGap - mod) < abs(coordGap)) {
            return coordGap - mod;
        }
        return coordGap;
    };

    auto findVectorGap = [findCoordinateGap](const Vector3& from, const Vector3& to) {
        const float x1 = from.x;
        const float x2 = to.x;
        const float gapX = findCoordinateGap(x1, x2, SpaceData::LengthX);

        const float z1 = from.z;
        const float z2 = to.z;
        const float gapZ = findCoordinateGap(z1, z2, SpaceData::LengthZ);

        return Vector3{gapX, to.y - from.y, gapZ};
    };

    {
        ZoneScopedN("Collide");
        auto collisionHandler = [&](CollisionPayload collider1, CollisionPayload collider2) {
            const Vector3& position1 = mRegistry.get<PositionComponent>(collider1.Entity).Position;
            const Vector3& position2 = mRegistry.get<PositionComponent>(collider2.Entity).Position;

            const Vector3 gap = findVectorGap(position1, position2);

            Vector3& velocity1 = mRegistry.get<VelocityComponent>(collider1.Entity).Velocity;
            Vector3& velocity2 = mRegistry.get<VelocityComponent>(collider2.Entity).Velocity;
            const Vector3 relativeVelocity = Vector3Subtract(velocity2, velocity1);

            float projection = Vector3DotProduct(gap, relativeVelocity);
            if (projection >= 0.f) {
                return;
            }

            const float minDistance = collider1.Radius + collider2.Radius;

            float distanceSq = gap.x * gap.x + gap.z * gap.z;
            if (distanceSq > minDistance * minDistance) {
                return;
            }

            bool isSpaceship1 = mRegistry.all_of<SpaceshipInputComponent>(collider1.Entity);
            bool isSpaceship2 = mRegistry.all_of<SpaceshipInputComponent>(collider2.Entity);

            if (!isSpaceship1 && !isSpaceship2) {
                assert(mRegistry.all_of<AsteroidComponent>(collider1.Entity));
                assert(mRegistry.all_of<AsteroidComponent>(collider2.Entity));
                // Asteroid vs asteroid
                const float mass1 = collider1.Radius * collider1.Radius * collider1.Radius;
                const float mass2 = collider2.Radius * collider2.Radius * collider2.Radius;
                const float massNormalizer = 2.f / (mass1 + mass2);

                const Vector3 transferedvelocity = Vector3Scale(gap, projection / distanceSq);
                velocity1 = Vector3Add(velocity1, Vector3Scale(transferedvelocity, mass2 * massNormalizer));
                velocity2 =
                Vector3Subtract(velocity2, Vector3Scale(transferedvelocity, mass1 * massNormalizer));
                return;
            }

            static_assert(SpaceshipData::CollisionRadius < SpaceshipData::ParticleCollisionRadius);
            float revizedMinDistance = 0.f;
            revizedMinDistance += (isSpaceship1) ? SpaceshipData::CollisionRadius : collider1.Radius;
            revizedMinDistance += (isSpaceship2) ? SpaceshipData::CollisionRadius : collider2.Radius;

            if (distanceSq > revizedMinDistance * revizedMinDistance) {
                return;
            }
            if (isSpaceship1) {
                mRegistry.emplace<DestroyComponent>(collider1.Entity);
            }
            if (isSpaceship2) {
                mRegistry.emplace<DestroyComponent>(collider2.Entity);
            }
        };
        mSpatialPartition.IteratePairs(collisionHandler);
    }

    auto particleCollisionView =
    mRegistry.view<ParticleComponent, PositionComponent, VelocityComponent>(entt::exclude<BulletComponent>);

    {
        ZoneScopedN("ExplosionPush");
        // Assuming the number of simultaneous explosions is low
        for (auto explosion : explosionView) {
            const Vector3& explosionPosition = mRegistry.get<PositionComponent>(explosion).Position;
            const float explosionRadius = mRegistry.get<ExplosionComponent>(explosion).CurrentRadius;
            auto particleExplosionProcess = [&](entt::entity particle, const ParticleComponent&,
                                                const PositionComponent& positionComponent,
                                                VelocityComponent& velocityComponent) {
                const Vector3& particlePosition = mRegistry.get<PositionComponent>(particle).Position;
                const float distanceSqr = Vector3DistanceSqr(explosionPosition, particlePosition);
                if (distanceSqr < explosionRadius * explosionRadius && !FloatEquals(distanceSqr, 0.f)) {
                    const Vector3 radial =
                    Vector3Normalize(Vector3Subtract(particlePosition, explosionPosition));
                    const Vector3 push = Vector3Scale(radial, deltaTime * ExplosionData::ParticleForce);
                    velocityComponent.Velocity = Vector3Add(velocityComponent.Velocity, push);
                }
            };
            particleCollisionView.each(particleExplosionProcess);
        }
    }

    {

        ZoneScopedN("ParticleCollision");
        auto particleCollisionProcess = [&](entt::entity particle, const ParticleComponent&,
                                            const PositionComponent& positionComponent,
                                            VelocityComponent& velocityComponent) {
            auto particleCollisionHandler = [&](CollisionPayload collider) {
                const Vector3& colliderVelocity = mRegistry.get<VelocityComponent>(collider.Entity).Velocity;
                const Vector3 impactVelocity = Vector3Subtract(velocityComponent.Velocity, colliderVelocity);

                const Vector3& colliderPosition = mRegistry.get<PositionComponent>(collider.Entity).Position;
                const Vector3 toCollider = findVectorGap(positionComponent.Position, colliderPosition);

                if (Vector3DotProduct(impactVelocity, toCollider) <= 0.f) {
                    return false;
                }

                const float distanceSqr = Vector3LengthSqr(toCollider);
                if (distanceSqr > collider.Radius * collider.Radius) {
                    return false;
                }

                static_assert(SpaceshipData::CollisionRadius < SpaceshipData::ParticleCollisionRadius);
                if (mRegistry.all_of<SpaceshipInputComponent>(collider.Entity) &&
                    mRegistry.all_of<BulletComponent>(particle)) {
                    if (distanceSqr > SpaceshipData::CollisionRadius * SpaceshipData::CollisionRadius) {
                        return false;
                    }
                }

                ParticleCollisionComponent& particleCollision =
                mRegistry.get_or_emplace<ParticleCollisionComponent>(particle);
                particleCollision.ImpactNormal = Vector3Normalize(toCollider);
                particleCollision.NormalContactSpeed =
                abs(Vector3DotProduct(impactVelocity, particleCollision.ImpactNormal));
                particleCollision.Collider = collider.Entity;

                return true;
            };

            const Vector2 flatPosition = {positionComponent.Position.x, positionComponent.Position.z};
            mSpatialPartition.IterateNearby(flatPosition, flatPosition, particleCollisionHandler);
        };
        particleCollisionView.each(particleCollisionProcess);
    }

    {
        ZoneScopedN("BulletCollision");
        auto bulletCollisionView = mRegistry.view<BulletComponent, PositionComponent, VelocityComponent>();
        auto bulletCollisionProcess = [&](entt::entity bullet, const PositionComponent& positionComponent,
                                          VelocityComponent& velocityComponent) {
            auto bulletCollisionHandler = [&](CollisionPayload collider) {
                const float radius = mRegistry.all_of<SpaceshipInputComponent>(collider.Entity) ?
                                     SpaceshipData::CollisionRadius :
                                     collider.Radius;

                const Vector3& colliderPosition = mRegistry.get<PositionComponent>(collider.Entity).Position;
                const Vector3 dp = findVectorGap(positionComponent.Position, colliderPosition);

                const Vector3& colliderVelocity = mRegistry.get<VelocityComponent>(collider.Entity).Velocity;
                const Vector3 dv = Vector3Subtract(colliderVelocity, velocityComponent.Velocity);

                // Quadratic terms:
                const float a = Vector3LengthSqr(dv);
                const float b = 2.f * Vector3DotProduct(dp, dv);
                const float c = Vector3LengthSqr(dp) - (radius * radius);

                const float determinant = b * b - (4.f * a * c);
                if (determinant < 0.f) {
                    return false;
                }

                const float sqrtDeterminant = sqrt(determinant);

                const float contactTime = -0.5f * (b + sqrtDeterminant) / a;

                if (contactTime > 0.f || contactTime < -deltaTime) {
                    return false;
                }

                const Vector3 relativeContactPosition = Vector3Add(dp, Vector3Scale(dv, contactTime));

                ParticleCollisionComponent& particleCollision =
                mRegistry.get_or_emplace<ParticleCollisionComponent>(bullet);
                particleCollision.ImpactNormal = Vector3Normalize(relativeContactPosition);
                particleCollision.NormalContactSpeed =
                abs(Vector3DotProduct(dv, particleCollision.ImpactNormal));
                particleCollision.Collider = collider.Entity;

                return true;
            };

            const Vector2 flatPosition = {positionComponent.Position.x, positionComponent.Position.z};
            mSpatialPartition.IterateNearby(flatPosition, flatPosition, bulletCollisionHandler);
        };
        bulletCollisionView.each(bulletCollisionProcess);
    }

    auto bulletPostCollisionView =
    mRegistry.view<ParticleCollisionComponent, BulletComponent, VelocityComponent>();
    auto bulletPostCollisionProcess = [&](entt::entity bullet, ParticleCollisionComponent& collision,
                                          VelocityComponent& velocityComponent) {
        if (collision.NormalContactSpeed <= 0.7f * WeaponData::BulletSpeed) {
            velocityComponent.Velocity = Vector3Scale(velocityComponent.Velocity, 0.5f);
            collision.NormalContactSpeed *= 0.5f; // So bounce velocity computation respects the 0.5 scaling
            return;
        }
        mRegistry.get_or_emplace<BulletHitComponent>(
        collision.Collider, std::clamp(collision.NormalContactSpeed / WeaponData::BulletSpeed, 0.f, 1.f));
        mRegistry.get_or_emplace<DestroyComponent>(bullet);
        mRegistry.erase<ParticleCollisionComponent>(bullet);
    };
    bulletPostCollisionView.each(bulletPostCollisionProcess);

    auto particlePostCollisionView = mRegistry.view<ParticleCollisionComponent, VelocityComponent>();
    auto particlePostCollisionProcess = [](entt::entity particle, const ParticleCollisionComponent& collision,
                                           VelocityComponent& velocityComponent) {
        const Vector3 bounceVelocity =
        Vector3Subtract(velocityComponent.Velocity,
                        Vector3Scale(collision.ImpactNormal, 2.f * collision.NormalContactSpeed));
        velocityComponent.Velocity = bounceVelocity;
    };
    particlePostCollisionView.each(particlePostCollisionProcess);

    mRegistry.clear<ParticleCollisionComponent>();

    auto hitAsteroidView = mRegistry.view<AsteroidComponent, BulletHitComponent>();
    auto hitAsteroidProcess = [&](entt::entity asteroid, const AsteroidComponent& asteroidComponent,
                                  const BulletHitComponent& hitComponent) {
        const float radius = asteroidComponent.Radius;
        const float relativeRadius = (radius - SpaceData::MinAsteroidRadius) /
                                     (SpaceData::MaxAsteroidRadius - SpaceData::MinAsteroidRadius);
        constexpr float MinDestroyChance = 0.075f;
        constexpr float MaxDestroyChance = 0.25f;
        const float destroyChance =
        sqrt(std::clamp(relativeRadius, 0.f, 1.f)) * (MinDestroyChance - MaxDestroyChance) + MaxDestroyChance;
        if (UniformDistribution(mRandomGenerator) >= destroyChance * hitComponent.HitCos) {
            return;
        }
        mRegistry.emplace<DestroyComponent>(asteroid);
    };
    hitAsteroidView.each(hitAsteroidProcess);

    auto hitSpaceshipView = mRegistry.view<SpaceshipInputComponent, BulletHitComponent>();
    auto hitSpaceshipProcess = [&](entt::entity spaceship, const SpaceshipInputComponent& spaceshipComponent,
                                   const BulletHitComponent& hitComponent) {
        if (UniformDistribution(mRandomGenerator) < 0.8f) {
            return;
        }
        mRegistry.emplace<DestroyComponent>(spaceship);
    };
    hitSpaceshipView.each(hitSpaceshipProcess);

    mRegistry.clear<BulletHitComponent>();

    auto shootView =
    mRegistry.view<PositionComponent, VelocityComponent, OrientationComponent, SpaceshipInputComponent, GunComponent>();
    auto shootProcess = [this](const PositionComponent& positionComponent, const VelocityComponent& velocityComponent,
                               const OrientationComponent& orientationComponent,
                               const SpaceshipInputComponent& inputComponent, GunComponent& gunComponent) {
        gunComponent.TimeSinceLastShot += deltaTime;
        if (!inputComponent.Input.Fire) {
            return;
        }
        if (gunComponent.TimeSinceLastShot < WeaponData::RateOfFire) {
            return;
        }
        Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Rotation);
        Vector3 offset = Vector3RotateByQuaternion(WeaponData::ShootBones[gunComponent.NextShotBone],
                                                   orientationComponent.Rotation);
        Vector3 shotPosition = Vector3Add(positionComponent.Position, offset);
        Vector3 shotVelocity =
        Vector3Add(velocityComponent.Velocity, Vector3Scale(forward, WeaponData::BulletSpeed));
        entt::entity bullet = mRegistry.create();
        mRegistry.emplace<BulletComponent>(bullet);
        mRegistry.emplace<PositionComponent>(bullet, shotPosition);
        mRegistry.emplace<OrientationComponent>(bullet, orientationComponent.Rotation);
        mRegistry.emplace<VelocityComponent>(bullet, shotVelocity);
        mRegistry.emplace<ParticleComponent>(bullet, WeaponData::BulletLifetime);
        gunComponent.NextShotBone += 1;
        gunComponent.NextShotBone %= WeaponData::ShootBones.size();
        gunComponent.TimeSinceLastShot = 0.f;
    };
    shootView.each(shootProcess);

    auto destroyedAsteroidsView =
    mRegistry.view<AsteroidComponent, PositionComponent, VelocityComponent, DestroyComponent>();
    auto destroyedAsteroidProcess = [this](const AsteroidComponent& asteroidComponent,
                                           const PositionComponent& positionComponent,
                                           const VelocityComponent& velocityComponent) {
        const float radius = asteroidComponent.Radius;
        const Vector3& position = positionComponent.Position;
        const Vector3& velocity = velocityComponent.Velocity;
        MakeExplosion(position, velocity, radius * ExplosionData::AsteroidMultiplier);
        const float breakRadius = 0.5f * radius;
        if (breakRadius > SpaceData::MinAsteroidRadius * 0.5f) {

            const float axisAngle = DirectionDistribution(mRandomGenerator);
            const Vector3 axis = {cos(axisAngle), 0.f, sin(axisAngle)};

            const float randomSpeedAngle = DirectionDistribution(mRandomGenerator);
            const Vector3 speedDrift = {cos(axisAngle), 0.f, sin(axisAngle)};

            MakeAsteroid(mRegistry, breakRadius, Vector3Add(position, Vector3Scale(axis, breakRadius)),
                         Vector3Add(velocity, speedDrift));
            MakeAsteroid(mRegistry, radius - breakRadius,
                         Vector3Subtract(position, Vector3Scale(axis, radius - breakRadius)),
                         Vector3Subtract(velocity, speedDrift));
        }
    };
    destroyedAsteroidsView.each(destroyedAsteroidProcess);

    auto destroyedSpaceshipView =
    mRegistry.view<SpaceshipInputComponent, PositionComponent, VelocityComponent, DestroyComponent>();
    auto destroyedSpaceshipProcess = [&](entt::entity spaceship, const SpaceshipInputComponent& inputComponent,
                                         const PositionComponent& positionComponent,
                                         const VelocityComponent& velocityComponent) {
        entt::entity respawner = mRegistry.create();
        mRegistry.emplace<RespawnComponent>(respawner, inputComponent.InputId, GameData::RespawnTimer);

        MakeExplosion(positionComponent.Position, velocityComponent.Velocity, ExplosionData::SpaceshipRadius);
    };
    destroyedSpaceshipView.each(destroyedSpaceshipProcess);

    mFrame++;
    GameTime = deltaTime * mFrame;
}

void Simulation::Tick()
{
    ZoneScoped;
    ProcessInput(mRegistry, mGameInput);
    Simulate();
}
