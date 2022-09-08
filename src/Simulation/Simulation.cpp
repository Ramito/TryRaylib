#include "Simulation.h"

#include "Components.h"
#include <raymath.h>

static std::uniform_real_distribution<float> UniformDistribution(0.f, 1.f);
static std::uniform_real_distribution<float> DirectionDistribution(0.f, 2.f * PI);

Simulation::Simulation(const SimDependencies& dependencies) : mRegistry(dependencies.GetDependency<entt::registry>()),
mGameInput(dependencies.GetDependency<std::remove_reference<decltype(mGameInput)>::type>())
{
}

static void MakeAsteroid(entt::registry& registry, float radius, const Vector3 position, const Vector3 velocity) {
	entt::entity asteroid = registry.create();
	registry.emplace<AsteroidComponent>(asteroid, radius);
	registry.emplace<PositionComponent>(asteroid, position);
	registry.emplace<VelocityComponent>(asteroid, velocity);
}

static void SpawnSpaceship(entt::registry& registry, uint32_t inputID) {
	entt::entity player = registry.create();
	registry.emplace<SpaceshipInputComponent>(player, inputID, GameInput{ 0.f, 0.f, false });
	registry.emplace<SteerComponent>(player, 0.f);
	registry.emplace<ThrustComponent>(player, 0.f);
	registry.emplace<PositionComponent>(player, 0.f, 0.f, 0.f);
	registry.emplace<VelocityComponent>(player, 0.f, 0.f, 0.f);
	registry.emplace<OrientationComponent>(player, QuaternionIdentity());
	registry.emplace<GunComponent>(player, 0.f, 0u);
}

void Simulation::Init() {
	mRegistry.clear();
	mRegistry.reserve(64000);

	SpawnSpaceship(mRegistry, 0u);

	std::uniform_real_distribution<float> xDistribution(0.f, SpaceData::LengthX);
	std::uniform_real_distribution<float> zDistribution(0.f, SpaceData::LengthZ);
	std::uniform_real_distribution<float> speedDistribution(0.f, 2.f * SpaceData::AsteroidDriftSpeed);
	std::uniform_real_distribution<float> radiusDistribution(SpaceData::MinAsteroidRadius, SpaceData::MaxAsteroidRadius);
	for (int i = 0; i < SpaceData::AsteroidsCount; ++i) {
		float angle = DirectionDistribution(mRandomGenerator);
		float speed = speedDistribution(mRandomGenerator);
		MakeAsteroid(mRegistry, radiusDistribution(mRandomGenerator)
			, { xDistribution(mRandomGenerator), 0.f, zDistribution(mRandomGenerator) },
			{ cos(angle) * speed, 0.f, sin(angle) * speed });
	}

	mSpatialPartition.InitArea({ SpaceData::LengthX, SpaceData::LengthZ }, SpaceData::CellCountX, SpaceData::CellCountZ);
}

static Vector3 HorizontalOrthogonal(const Vector3& vector) {
	return { -vector.z, vector.y, vector.x };
}

static void ProcessInput(entt::registry& registry, const std::array<GameInput, 4>& gameInput) {
	for (SpaceshipInputComponent& inputComponent : registry.view<SpaceshipInputComponent>().storage()) {
		inputComponent.Input = gameInput[inputComponent.InputId];
	}
}

void Simulation::Simulate() {
	constexpr float deltaTime = SimTimeData::DeltaTime;

	for (auto entity : mRegistry.view<DestroyComponent>()) {
		mRegistry.destroy(entity);
	}

	for (auto respawner : mRegistry.view<RespawnComponent>()) {
		RespawnComponent& respawn = mRegistry.get<RespawnComponent>(respawner);
		respawn.TimeLeft -= deltaTime;
		if (respawn.TimeLeft > 0.f) {
			continue;
		}
		SpawnSpaceship(mRegistry, respawn.InputId);
		mRegistry.destroy(respawner);
	}

	auto playerView = mRegistry.view<VelocityComponent, OrientationComponent, SteerComponent, SpaceshipInputComponent, ThrustComponent>();
	auto playerProcess = [this](entt::entity entity,
		VelocityComponent& velocityComponent,
		OrientationComponent& orientationComponent,
		SteerComponent& steerComponent,
		const SpaceshipInputComponent& inputComponent,
		ThrustComponent& thrustComponent) {
			const GameInput& input = inputComponent.Input;
			Vector3 inputTarget = { input.Left, 0.f, input.Forward };
			float inputLength = Vector3Length(inputTarget);

			Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);

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
			}
			else {
				if (steeringSign != steerSign || steer > targetSteer) {
					steer -= SpaceshipData::NegativeRoll * deltaTime;
				}
				else {
					steer += SpaceshipData::Roll * deltaTime;
					steer = std::min(steer, targetSteer);
				}
			}

			float turnAbility = cosf(steer) * SpaceshipData::Yaw;
			if (steeringSign == steerSign) {
				turnAbility += sinf(steer) * SpaceshipData::Pitch;
			}
			else {
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
				if (FloatEquals(dot, -1.f))
				{
					yawQuaternion = yawQuaternion = QuaternionFromAxisAngle(Up3, PI);
				}
				else {
					yawQuaternion = QuaternionFromVector3ToVector3(Forward3, forward);
				}

				Quaternion turningQuaternion = QuaternionFromAxisAngle(Up3, turnAbility * deltaTime);

				resultingQuaternion = QuaternionMultiply(yawQuaternion, rollQuaternion);
				resultingQuaternion = QuaternionMultiply(turningQuaternion, resultingQuaternion);
			}
			else {
				Quaternion rollQuaternion = QuaternionFromAxisAngle(Forward3, -steer);

				float dot = Vector3DotProduct(Forward3, inputDirection);
				Quaternion yawQuaternion;
				if (FloatEquals(dot, -1.f)) {
					yawQuaternion = QuaternionFromAxisAngle(Up3, PI);
				}
				else {
					yawQuaternion = QuaternionFromVector3ToVector3(Forward3, inputDirection);
				}

				resultingQuaternion = QuaternionMultiply(yawQuaternion, rollQuaternion);
			}
			orientationComponent.Quaternion = resultingQuaternion;
	};
	playerView.each(playerProcess);

	auto shootView = mRegistry.view<PositionComponent, VelocityComponent, OrientationComponent, SpaceshipInputComponent, GunComponent>();
	auto shootProcess = [this](const PositionComponent& positionComponent,
		const VelocityComponent& velocityComponent,
		const OrientationComponent& orientationComponent,
		const SpaceshipInputComponent& inputComponent,
		GunComponent& gunComponent) {
			gunComponent.TimeSinceLastShot += deltaTime;
			if (!inputComponent.Input.Fire) {
				return;
			}
			if (gunComponent.TimeSinceLastShot < WeaponData::RateOfFire) {
				return;
			}
			Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);
			Vector3 offset = Vector3RotateByQuaternion(WeaponData::ShootBones[gunComponent.NextShotBone], orientationComponent.Quaternion);
			Vector3 shotPosition = Vector3Add(positionComponent.Position, offset);
			Vector3 shotVelocity = Vector3Add(velocityComponent.Velocity, Vector3Scale(forward, WeaponData::BulletSpeed));
			entt::entity bullet = mRegistry.create();
			mRegistry.emplace<BulletComponent>(bullet);
			mRegistry.emplace<PositionComponent>(bullet, shotPosition);
			mRegistry.emplace<OrientationComponent>(bullet, orientationComponent.Quaternion);
			mRegistry.emplace<VelocityComponent>(bullet, shotVelocity);
			mRegistry.emplace<ParticleComponent>(bullet, WeaponData::BulletLifetime);
			gunComponent.NextShotBone += 1;
			gunComponent.NextShotBone %= WeaponData::ShootBones.size();
			gunComponent.TimeSinceLastShot = 0.f;

	};
	shootView.each(shootProcess);

	auto thrustView = mRegistry.view<ThrustComponent, PositionComponent, VelocityComponent, OrientationComponent>();
	auto thrustParticleProcess = [this](ThrustComponent& thrustComponent,
		const PositionComponent& positionComponent,
		const VelocityComponent& velocityComponent,
		const OrientationComponent& orientationComponent) {
			constexpr float ThrustModule = 25.f;
			constexpr float RandomModule = 5.5f;
			constexpr float Offset = 0.4f;
			constexpr uint32_t MinParticles = 0;
			constexpr uint32_t MaxParticles = 2;
			float relativeThrust = thrustComponent.Thrust / SpaceshipData::Thrust;
			uint32_t particles = MinParticles + relativeThrust * relativeThrust * MaxParticles;

			Vector3 baseVelocity = velocityComponent.Velocity;
			Vector3 back = Vector3RotateByQuaternion(Back3, orientationComponent.Quaternion);
			baseVelocity = Vector3Add(baseVelocity, Vector3Scale(back, thrustComponent.Thrust * ThrustModule * deltaTime));

			while (particles-- > 0) {
				entt::entity particleEntity = mRegistry.create();
				mRegistry.emplace<ParticleDragComponent>(particleEntity);
				mRegistry.emplace<PositionComponent>(particleEntity, Vector3Add(positionComponent.Position, Vector3Scale(back, Offset)));
				float randX = 1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float randY = 1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float randZ = 1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				Vector3 randomVelocity = Vector3Scale({ randX, randY, randZ }, RandomModule);
				mRegistry.emplace<VelocityComponent>(particleEntity, Vector3Add(baseVelocity, randomVelocity));
				float lifetime = ((std::rand() + std::rand()) / (2 * RAND_MAX / 1250)) * deltaTime;
				mRegistry.emplace<ParticleComponent>(particleEntity, lifetime);
			}
	};
	thrustView.each(thrustParticleProcess);

	auto particleView = mRegistry.view<ParticleComponent>();
	auto particleLifetimeProcess = [this](entt::entity particle,
		ParticleComponent& particleComponent) {
			if (particleComponent.LifeTime <= 0.f) {
				mRegistry.destroy(particle);
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
			velocityComponent.Velocity = Vector3Add(velocityComponent.Velocity, Vector3Scale(velocityComponent.Velocity, -drag / speed));
		}
	};
	particleDragView.each(particleDragProcess);

	auto dynamicView = mRegistry.view<PositionComponent, VelocityComponent>();
	auto dynamicProcess = [](PositionComponent& positionComponent, const VelocityComponent& velocityComponent) {
		positionComponent.Position = Vector3Add(positionComponent.Position, Vector3Scale(velocityComponent.Velocity, deltaTime));
	};
	dynamicView.each(dynamicProcess);

	auto warpView = mRegistry.view<PositionComponent>();
	auto warpProcess = [](PositionComponent& positionComponent) {
		const float x = positionComponent.Position.x;
		const float z = positionComponent.Position.z;
		if (x < 0.f) {
			positionComponent.Position.x += SpaceData::LengthX;
		}
		else if (x > SpaceData::LengthX) {
			positionComponent.Position.x -= SpaceData::LengthX;
		}
		if (z < 0.f) {
			positionComponent.Position.z += SpaceData::LengthZ;
		}
		else if (z > SpaceData::LengthZ) {
			positionComponent.Position.z -= SpaceData::LengthZ;
		}
	};
	warpView.each(warpProcess);

	mSpatialPartition.Clear();

	auto asteroidView = mRegistry.view<PositionComponent, AsteroidComponent>();
	auto partitionAsteroids = [this](entt::entity asteroid, const PositionComponent& positionComponent, const AsteroidComponent& asteroidComponent)
	{
		const float radius = asteroidComponent.Radius;
		const Vector2 flatPosition = { positionComponent.Position.x, positionComponent.Position.z };
		Vector2 min = { flatPosition.x - radius, flatPosition.y - radius };
		Vector2 max = { flatPosition.x + radius, flatPosition.y + radius };
		mSpatialPartition.InsertDeferred(asteroid, min, max);
	};
	asteroidView.each(partitionAsteroids);

	mSpatialPartition.FlushInsertions();

	auto findCoordinateGap = [](float coord1, float coord2, float mod) {
		float coordGap = coord2 - coord1;
		if (abs(coordGap + mod) < abs(coordGap)) {
			return coordGap + mod;
		}
		else if (abs(coordGap - mod) < abs(coordGap)) {
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

		return Vector3{ gapX, to.y - from.y, gapZ };
	};

	auto collisionHandler = [&](entt::entity asteroid1, entt::entity asteroid2) {
		const Vector3& position1 = mRegistry.get<PositionComponent>(asteroid1).Position;
		const Vector3& position2 = mRegistry.get<PositionComponent>(asteroid2).Position;

		const Vector3 gap = findVectorGap(position1, position2);

		Vector3& velocity1 = mRegistry.get<VelocityComponent>(asteroid1).Velocity;
		Vector3& velocity2 = mRegistry.get<VelocityComponent>(asteroid2).Velocity;
		const Vector3 relativeVelocity = Vector3Subtract(velocity2, velocity1);

		float projection = Vector3DotProduct(gap, relativeVelocity);
		if (projection >= 0.f)
		{
			return;
		}

		float radius1 = mRegistry.get<AsteroidComponent>(asteroid1).Radius;
		float radius2 = mRegistry.get<AsteroidComponent>(asteroid2).Radius;
		float minDistance = radius1 + radius2;

		float distanceSq = gap.x * gap.x + gap.z * gap.z;
		if (distanceSq < minDistance * minDistance) {
			const float mass1 = radius1 * radius1 * radius1;
			const float mass2 = radius2 * radius2 * radius2;
			const float massNormalizer = 2.f / (mass1 + mass2);

			const Vector3 transferedvelocity = Vector3Scale(gap, projection / distanceSq);
			velocity1 = Vector3Add(velocity1, Vector3Scale(transferedvelocity, mass2 * massNormalizer));
			velocity2 = Vector3Subtract(velocity2, Vector3Scale(transferedvelocity, mass1 * massNormalizer));
		}
	};
	mSpatialPartition.IteratePairs(collisionHandler);

	auto particleCollisionView = mRegistry.view<ParticleComponent, PositionComponent, VelocityComponent>();
	auto particleCollisionProcess = [&](entt::entity particle, const ParticleComponent&, const PositionComponent& positionComponent, VelocityComponent& velocityComponent)
	{
		auto particleCollisionHandler = [&](entt::entity asteroid) {
			const Vector3& asteroidVelocity = mRegistry.get<VelocityComponent>(asteroid).Velocity;
			const Vector3 impactVelocity = Vector3Subtract(velocityComponent.Velocity, asteroidVelocity);

			const Vector3& asteroidPosition = mRegistry.get<PositionComponent>(asteroid).Position;
			const Vector3 toAsteroid = findVectorGap(positionComponent.Position, asteroidPosition);

			if (Vector3DotProduct(impactVelocity, toAsteroid) <= 0.f) {
				return false;
			}

			const float radius = mRegistry.get<AsteroidComponent>(asteroid).Radius;
			const float distanceSqr = Vector3LengthSqr(toAsteroid);
			if (distanceSqr > radius * radius) {
				return false;
			}

			ParticleCollisionComponent& particleCollision = mRegistry.get_or_emplace<ParticleCollisionComponent>(particle);
			particleCollision.ImpactNormal = Vector3Normalize(toAsteroid);
			particleCollision.ContactSpeed = abs(Vector3DotProduct(impactVelocity, particleCollision.ImpactNormal));
			particleCollision.Collider = asteroid;

			return true;
		};

		const Vector2 flatPosition = { positionComponent.Position.x, positionComponent.Position.z };
		mSpatialPartition.IterateNearby(flatPosition, flatPosition, particleCollisionHandler);
	};
	particleCollisionView.each(particleCollisionProcess);

	auto bulletPostCollisionView = mRegistry.view<ParticleCollisionComponent, BulletComponent, VelocityComponent>();
	auto bulletPostCollisionProcess = [&](entt::entity bullet, ParticleCollisionComponent& collision, VelocityComponent& velocityComponent) {
		if (collision.ContactSpeed <= 0.75f * WeaponData::BulletSpeed) {
			velocityComponent.Velocity = Vector3Scale(velocityComponent.Velocity, 0.5f);
			collision.ContactSpeed *= 0.5f;
			return;
		}
		mRegistry.erase<ParticleCollisionComponent>(bullet);
		mRegistry.get_or_emplace<HitAsteroidComponent>(collision.Collider, std::clamp(collision.ContactSpeed / WeaponData::BulletSpeed, 0.f, 1.f));
		mRegistry.emplace<DestroyComponent>(bullet);
	};
	bulletPostCollisionView.each(bulletPostCollisionProcess);

	auto particlePostCollisionView = mRegistry.view<ParticleCollisionComponent, VelocityComponent>();
	auto particlePostCollisionProcess = [](entt::entity particle, const ParticleCollisionComponent& collision, VelocityComponent& velocityComponent)
	{
		const Vector3 bounceVelocity = Vector3Subtract(velocityComponent.Velocity, Vector3Scale(collision.ImpactNormal, 2.f * collision.ContactSpeed));
		velocityComponent.Velocity = bounceVelocity;
	};
	particlePostCollisionView.each(particlePostCollisionProcess);

	mRegistry.clear<ParticleCollisionComponent>();

	auto makeExplosion = [&](const Vector3& position) {
		for (size_t i = 0; i < 1500; ++i) {
			auto particle = mRegistry.create();
			float x = 2.f * UniformDistribution(mRandomGenerator) - 1.f;
			float y = 2.f * UniformDistribution(mRandomGenerator) - 1.f;
			float z = 2.f * UniformDistribution(mRandomGenerator) - 1.f;
			const Vector3 radial = Vector3Normalize({ x,y,z });
			float radius = cbrt(UniformDistribution(mRandomGenerator));
			mRegistry.emplace<PositionComponent>(particle, position);
			mRegistry.emplace<VelocityComponent>(particle, Vector3Scale(radial, 175.f * radius));
			mRegistry.emplace<ParticleComponent>(particle, 1.f + UniformDistribution(mRandomGenerator) * 20.f);
			mRegistry.emplace<ParticleDragComponent>(particle);
		}
	};

	auto playerCollisionView = mRegistry.view<PositionComponent, SpaceshipInputComponent>();
	for (entt::entity spacechip : playerCollisionView) {
		const Vector3& spaceshipPosition = mRegistry.get<PositionComponent>(spacechip).Position;

		auto collisionChecker = [&](entt::entity asteroid) {
			const float asteroidRadius = mRegistry.get<AsteroidComponent>(asteroid).Radius;
			const Vector3 asteroidPosition = mRegistry.get<PositionComponent>(asteroid).Position;
			float distanceSqr = Vector3DistanceSqr(spaceshipPosition, asteroidPosition);
			const float collisionDistance = asteroidRadius + SpaceshipData::CollisionRadius;
			if (distanceSqr < collisionDistance * collisionDistance) {
				entt::entity respawner = mRegistry.create();
				mRegistry.emplace<RespawnComponent>(respawner, mRegistry.get<SpaceshipInputComponent>(spacechip).InputId, GameData::RespawnTimer);
				mRegistry.emplace<DestroyComponent>(spacechip);

				makeExplosion(spaceshipPosition);

				return true;
			}
			return false;
		};

		const Vector2 flatPosition = { spaceshipPosition.x, spaceshipPosition.z };
		mSpatialPartition.IterateNearby(flatPosition, flatPosition, collisionChecker);
	}

	auto hitAsteroidView = mRegistry.view<AsteroidComponent, HitAsteroidComponent>();
	auto hitAsteroidProcess = [&](entt::entity asteroid, const AsteroidComponent& asteroidComponent, const HitAsteroidComponent& hitComponent) {
		const float radius = asteroidComponent.Radius;
		const float relativeRadius = (radius - SpaceData::MinAsteroidRadius) / (SpaceData::MaxAsteroidRadius - SpaceData::MinAsteroidRadius);
		constexpr float MinDestroyChance = 0.075f;
		constexpr float MaxDestroyChance = 0.25f;
		const float destroyChance = sqrt(std::clamp(relativeRadius, 0.f, 1.f)) * (MinDestroyChance - MaxDestroyChance) + MaxDestroyChance;
		if (UniformDistribution(mRandomGenerator) >= destroyChance * hitComponent.HitCos) {
			return;
		}
		const float breakRadius = 0.5f * radius;
		const Vector3& position = mRegistry.get<PositionComponent>(asteroid).Position;
		if (breakRadius > SpaceData::MinAsteroidRadius * 0.5f) {
			const Vector3& velocity = mRegistry.get<VelocityComponent>(asteroid).Velocity;


			const float axisAngle = DirectionDistribution(mRandomGenerator);
			const Vector3 axis = { cos(axisAngle), 0.f, sin(axisAngle) };

			const float randomSpeedAngle = DirectionDistribution(mRandomGenerator);
			const Vector3 speedDrift = { cos(axisAngle), 0.f, sin(axisAngle) };

			MakeAsteroid(mRegistry, breakRadius, Vector3Add(position, Vector3Scale(axis, breakRadius)), Vector3Add(velocity, speedDrift));
			MakeAsteroid(mRegistry, radius - breakRadius, Vector3Subtract(position, Vector3Scale(axis, radius - breakRadius)), Vector3Subtract(velocity, speedDrift));
		}
		makeExplosion(position);
		mRegistry.emplace<DestroyComponent>(asteroid);
	};
	hitAsteroidView.each(hitAsteroidProcess);

	mRegistry.clear<HitAsteroidComponent>();

	mFrame++;
	mGameTime += deltaTime;
}

void Simulation::Tick() {
	ProcessInput(mRegistry, mGameInput);
	Simulate();
}