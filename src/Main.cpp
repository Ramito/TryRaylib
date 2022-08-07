#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"

static void SetupWindow() {
	SetTargetFPS(120);
	InitWindow(2400, 1500, "Game");
}

constexpr Vector3 CameraOffset = { -10.f, 25.f, -10.f };

static Camera SetupCamera() {
	Camera camera;
	camera.position = Vector3Negate(CameraOffset);
	camera.target = { 0.f, 0.f, 0.f };
	camera.projection = CAMERA_PERSPECTIVE;
	camera.up = { 0.f, 1.f, 0.f };
	camera.fovy = 72.f;
	SetCameraMode(camera, CAMERA_CUSTOM);
	return camera;
}

struct RelativeInput {
	float Forward;
	float Left;
};

constexpr Vector3 Forward3 = { 0.f, 0.f, 1.f };
constexpr Vector3 Back3 = { 0.f, 0.f, -1.f };
constexpr Vector3 Left3 = { 1.f, 0.f, 0.f };
constexpr Vector3 Up3 = { 0.f, 1.f, 0.f };

namespace SimTimeData {
	constexpr uint32_t TargetFPS = 120;
	constexpr float DeltaTime = 1.f / TargetFPS;
}

namespace SpaceshipData {
	constexpr float MinThrust = 10.f;
	constexpr float Thrust = 15.f;
	constexpr float LinearDrag = 1e-5;
	constexpr float QuadraticDrag = 1e-3;

	constexpr float Yaw = 0.15f;
	constexpr float Pitch = 2.5f;
	constexpr float Roll = 1.25f;

	constexpr float NegativePitch = 1.25f;
	constexpr float NegativeRoll = 2.f;

	constexpr float SteerB = 0.25f;
	constexpr float SteerM = 1.5f;
}

namespace ParticleData {
	constexpr float LinearDrag = 0.f;
	constexpr float QuadraticDrag = 1e-2;
}

struct SteerComponent {
	float Steer;
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
	RelativeInput Input;
};
struct ParticleComponent {
	uint32_t LifeTime;
};

static void DrawSpaceShip(const Vector3& position, const Quaternion orientation) {
	constexpr float Angle = -2.f * PI / 3.f;
	constexpr float Length = 1.25f;
	constexpr float Width = 0.65f;
	constexpr float Height = 0.25f;

	Vector3 forward = Vector3RotateByQuaternion(Forward3, orientation);
	Vector3 left = Vector3RotateByQuaternion(Left3, orientation);
	Vector3 up = Vector3RotateByQuaternion(Up3, orientation);

	Vector3 upShift = Vector3Scale(up, Height);
	Vector3 downShift = Vector3Scale(up, -2.f * Height);

	Quaternion triangleQuat = QuaternionFromAxisAngle(up, Angle);

	Vector3 vertex1 = Vector3Add(Vector3Scale(forward, Length), position);

	Vector3 relVertex = Vector3RotateByQuaternion(forward, triangleQuat);
	Vector3 vertex2 = Vector3Add(Vector3Add(Vector3Scale(relVertex, Width), position), upShift);

	relVertex = Vector3RotateByQuaternion(relVertex, triangleQuat);
	Vector3 vertex3 = Vector3Subtract(Vector3Add(Vector3Scale(relVertex, Width), position), upShift);

	DrawLine3D(vertex1, vertex2, RED);
	DrawLine3D(vertex2, vertex3, RED);
	DrawLine3D(vertex3, vertex1, RED);

	vertex2 = Vector3Add(vertex2, downShift);
	vertex3 = Vector3Subtract(vertex3, downShift);

	DrawLine3D(vertex1, vertex2, RED);
	DrawLine3D(vertex2, vertex3, RED);
	DrawLine3D(vertex3, vertex1, RED);

	Vector3 midBack = Vector3Lerp(vertex2, vertex3, 0.5f);
	DrawLine3D(vertex1, midBack, RED);
}

static void Draw(Camera& camera, entt::registry& registry) {
	for (auto playerEntity : registry.view<PositionComponent>()) {
		auto& position = registry.get<PositionComponent>(playerEntity);
		camera.target = position.Position;
		camera.position = Vector3Add(camera.target, CameraOffset);
	}
	UpdateCamera(&camera);
	BeginDrawing();
	ClearBackground(DARKGRAY);
	BeginMode3D(camera);
	DrawGrid(500, 5.0f);

	for (auto entity : registry.view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {
		auto& position = registry.get<PositionComponent>(entity);
		auto& orientation = registry.get<OrientationComponent>(entity);
		DrawSpaceShip(position.Position, orientation.Quaternion);
	}

	for (entt::entity particle : registry.view<ParticleComponent>()) {
		const Vector3& position = registry.get<PositionComponent>(particle).Position;
		DrawPoint3D(position, ORANGE);
	}

	EndMode3D();
	EndDrawing();
}

void UpdateInput(const Camera& camera, std::array<RelativeInput, 4>& gameInputs) {
	Vector2 pos = { camera.position.x, camera.position.z };
	Vector2 tar = { camera.target.x, camera.target.z };
	Vector2 to = Vector2Subtract(tar, pos);
	Vector2 normalizedTo = Vector2Normalize(to);
	Vector2 normalizedOrthogonal = { -normalizedTo.y, normalizedTo.x };

	size_t idx = 0;
	for (RelativeInput& relativeInput : gameInputs) {
		Vector2 input = { 0.f,0.f };
		if (IsGamepadAvailable(idx)) {
			input.x = GetGamepadAxisMovement(0, 0);
			input.y = -GetGamepadAxisMovement(0, 1);
		}
		else
		{
			if (IsKeyDown(KEY_A)) {
				input.x -= 1.f;
			}
			if (IsKeyDown(KEY_D)) {
				input.x += 1.f;
			}
			if (IsKeyDown(KEY_S)) {
				input.y -= 1.f;
			}
			if (IsKeyDown(KEY_W)) {
				input.y += 1.f;
			}
		}

		if (Vector2LengthSqr(input) > 1.f) {
			input = Vector2Normalize(input);
		}


		float forward = Vector2DotProduct(input, normalizedTo);
		float left = Vector2DotProduct(input, normalizedOrthogonal);

		relativeInput = { forward, left };
	}
}

static entt::registry SetupSim() {
	entt::registry registry;
	registry.reserve(64000);

	entt::entity player = registry.create();
	registry.emplace<SpaceshipInputComponent>(player, 0u, RelativeInput{ 0.f, 0.f });
	registry.emplace<SteerComponent>(player, 0.f);
	registry.emplace<ThrustComponent>(player, 0.f);
	registry.emplace<PositionComponent>(player, 0.f, 2.f, 0.f);
	registry.emplace<VelocityComponent>(player, 0.f, 0.f, 0.f);
	registry.emplace<OrientationComponent>(player, QuaternionIdentity());

	return registry;
}

static Vector3 HorizontalOrthogonal(const Vector3& vector) {
	return { -vector.z, vector.y, vector.x };
}

static void ProcessInput(entt::registry& registry, std::array<RelativeInput, 4> gameInput) {
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
			const RelativeInput& input = inputComponent.Input;
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
				steer -= SpaceshipData::Roll * deltaTime;
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

				Quaternion yawQuaternion = QuaternionFromVector3ToVector3(Forward3, forward);

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

	auto thrustView = registry.view<ThrustComponent, PositionComponent, VelocityComponent, OrientationComponent>();
	auto thrustParticleProcess = [&](ThrustComponent& thrustComponent,
		const PositionComponent& positionComponent,
		const VelocityComponent& velocityComponent,
		const OrientationComponent& orientationComponent) {
			constexpr float ThrustModule = 25.f;
			constexpr float RandomModule = 5.5f;
			constexpr float Offset = 0.4f;
			constexpr uint32_t MinParticles = 0;
			constexpr uint32_t MaxParticles = 5;
			float relativeThrust = thrustComponent.Thrust / SpaceshipData::Thrust;
			uint32_t particles = MinParticles + relativeThrust * relativeThrust * MaxParticles;

			Vector3 baseVelocity = velocityComponent.Velocity;
			Vector3 back = Vector3RotateByQuaternion(Back3, orientationComponent.Quaternion);
			baseVelocity = Vector3Add(baseVelocity, Vector3Scale(back, thrustComponent.Thrust * ThrustModule * deltaTime));

			while (particles-- > 0) {
				entt::entity particleEntity = registry.create();
				registry.emplace<PositionComponent>(particleEntity, Vector3Add(positionComponent.Position, Vector3Scale(back, Offset)));
				float randX = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float randY = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				float randZ = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				Vector3 randomVelocity = Vector3Scale({ randX, randY, randZ }, RandomModule);
				registry.emplace<VelocityComponent>(particleEntity, Vector3Add(baseVelocity, randomVelocity));
				uint32_t lifetime = std::rand() / (RAND_MAX / 1500);
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

			float speed = Vector3Length(velocityComponent.Velocity);
			if (!FloatEquals(speed, 0.f)) {
				float drag = speed * ParticleData::LinearDrag + speed * speed * ParticleData::QuadraticDrag;
				velocityComponent.Velocity = Vector3Add(velocityComponent.Velocity, Vector3Scale(velocityComponent.Velocity, -drag / speed));
			}
	};
	particleView.each(particleProcess);
}

void main() {
	SetupWindow();
	Camera camera = SetupCamera();
	entt::registry registry = SetupSim();
	double gameStartTime = GetTime();
	uint32_t simTicks = 0;
	std::array<RelativeInput, 4> gameInput;
	while (!WindowShouldClose()) {
		UpdateInput(camera, gameInput);
		ProcessInput(registry, gameInput);
		while (gameStartTime + SimTimeData::DeltaTime * simTicks < GetTime()) {
			Simulate(registry);
			simTicks += 1;
		}
		Draw(camera, registry);
	}
	CloseWindow();
}