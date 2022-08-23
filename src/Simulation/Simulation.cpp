#include "Simulation.h"

#include "Components.h"
#include <raymath.h>

Simulation::Simulation(const SimDependencies& dependencies) : mRegistry(dependencies.GetDependency<entt::registry>()),
mGameInput(dependencies.GetDependency<std::remove_reference<decltype(mGameInput)>::type>())
{
}

void Simulation::Init() {
	mRegistry.clear();
	mRegistry.reserve(64000);

	entt::entity player = mRegistry.create();
	mRegistry.emplace<SpaceshipInputComponent>(player, 0u, GameInput{ 0.f, 0.f, false });
	mRegistry.emplace<SteerComponent>(player, 0.f);
	mRegistry.emplace<ThrustComponent>(player, 0.f);
	mRegistry.emplace<PositionComponent>(player, 0.f, 2.f, 0.f);
	mRegistry.emplace<VelocityComponent>(player, 0.f, 0.f, 0.f);
	mRegistry.emplace<OrientationComponent>(player, QuaternionIdentity());
	mRegistry.emplace<GunComponent>(player, 0.f, 0u);
}

static Vector3 HorizontalOrthogonal(const Vector3& vector) {
	return { -vector.z, vector.y, vector.x };
}

static void ProcessInput(entt::registry& registry, const std::array<GameInput, 4>& gameInput) {
	for (SpaceshipInputComponent& inputComponent : registry.view<SpaceshipInputComponent>().storage()) {
		inputComponent.Input = gameInput[inputComponent.InputId];
	}
}

static void Simulate(entt::registry& registry) {
	constexpr float deltaTime = SimTimeData::DeltaTime;

	auto playerView = registry.view<PositionComponent, VelocityComponent, OrientationComponent, SteerComponent, SpaceshipInputComponent, ThrustComponent>();
	auto playerProcess = [deltaTime, &registry](entt::entity entity,
		PositionComponent& positionComponent,
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
			Vector3 displacement = Vector3Scale(velocity, deltaTime);

			positionComponent.Position = Vector3Add(positionComponent.Position, displacement);

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

	auto shootView = registry.view<PositionComponent, VelocityComponent, OrientationComponent, SpaceshipInputComponent, GunComponent>();
	auto shootProcess = [&registry](const PositionComponent& positionComponent,
		const VelocityComponent& velocityComponent,
		const OrientationComponent& orientationComponent,
		const SpaceshipInputComponent& inputComponent,
		GunComponent& gunComponent) {
			gunComponent.TimeBeforeNextShot = std::max(gunComponent.TimeBeforeNextShot - deltaTime, 0.f);
			if (!inputComponent.Input.Fire) {
				return;
			}
			if (gunComponent.TimeBeforeNextShot > 0.f) {
				return;
			}
			Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);
			Vector3 offset = Vector3RotateByQuaternion(WeaponData::ShootBones[gunComponent.NextShotBone], orientationComponent.Quaternion);
			Vector3 shotPosition = Vector3Add(positionComponent.Position, offset);
			Vector3 shotVelocity = Vector3Add(velocityComponent.Velocity, Vector3Scale(forward, WeaponData::BulletSpeed));
			entt::entity bullet = registry.create();
			registry.emplace<PositionComponent>(bullet, shotPosition);
			registry.emplace<OrientationComponent>(bullet, orientationComponent.Quaternion);
			registry.emplace<VelocityComponent>(bullet, shotVelocity);
			registry.emplace<ParticleComponent>(bullet, static_cast<uint32_t>(WeaponData::BulletLifetime / SimTimeData::DeltaTime));
			gunComponent.NextShotBone += 1;
			gunComponent.NextShotBone %= WeaponData::ShootBones.size();
			gunComponent.TimeBeforeNextShot = WeaponData::RateOfFire;

	};
	shootView.each(shootProcess);

	auto thrustView = registry.view<ThrustComponent, PositionComponent, VelocityComponent, OrientationComponent>();
	auto thrustParticleProcess = [&](ThrustComponent& thrustComponent,
		const PositionComponent& positionComponent,
		const VelocityComponent& velocityComponent,
		const OrientationComponent& orientationComponent) {
			constexpr float ThrustModule = 25.f;
			constexpr float RandomModule = 5.5f;
			constexpr float Offset = 0.4f;
			constexpr uint32_t MinParticles = 0;
			constexpr uint32_t MaxParticles = 3;
			float relativeThrust = thrustComponent.Thrust / SpaceshipData::Thrust;
			uint32_t particles = MinParticles + relativeThrust * relativeThrust * MaxParticles;

			Vector3 baseVelocity = velocityComponent.Velocity;
			Vector3 back = Vector3RotateByQuaternion(Back3, orientationComponent.Quaternion);
			baseVelocity = Vector3Add(baseVelocity, Vector3Scale(back, thrustComponent.Thrust * ThrustModule * deltaTime));

			while (particles-- > 0) {
				entt::entity particleEntity = registry.create();
				registry.emplace<PositionComponent>(particleEntity, Vector3Add(positionComponent.Position, Vector3Scale(back, Offset)));
				float randX = 1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float randY = 1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float randZ = 1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				Vector3 randomVelocity = Vector3Scale({ randX, randY, randZ }, RandomModule);
				registry.emplace<VelocityComponent>(particleEntity, Vector3Add(baseVelocity, randomVelocity));
				uint32_t lifetime = (std::rand() + std::rand()) / (2 * RAND_MAX / 1250);
				registry.emplace<ParticleComponent>(particleEntity, lifetime);
			}
	};
	thrustView.each(thrustParticleProcess);

	auto particleView = registry.view<ParticleComponent, PositionComponent, VelocityComponent>();
	auto particleProcess = [deltaTime, &registry](entt::entity particle,
		ParticleComponent& particleComponent,
		PositionComponent& positionComponent,
		VelocityComponent& velocityComponent) {
			if (particleComponent.LifeTime == 0) {
				registry.destroy(particle);
				return;
			}
			particleComponent.LifeTime--;
			positionComponent.Position = Vector3Add(positionComponent.Position, Vector3Scale(velocityComponent.Velocity, deltaTime));

			if (registry.any_of<OrientationComponent>(particle)) {
				// It's a bullet!
				return;
			}

			float speed = Vector3Length(velocityComponent.Velocity);
			if (!FloatEquals(speed, 0.f)) {
				float drag = speed * ParticleData::LinearDrag + speed * speed * ParticleData::QuadraticDrag;
				velocityComponent.Velocity = Vector3Add(velocityComponent.Velocity, Vector3Scale(velocityComponent.Velocity, -drag / speed));
			}
	};
	particleView.each(particleProcess);
}

void Simulation::Tick() {
	ProcessInput(mRegistry, mGameInput);
	Simulate(mRegistry);
}