#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"

static void SetupWindow() {
	SetTargetFPS(120);
	InitWindow(2400, 1500, "Game");
}

constexpr Vector3 CameraOffset = { -8.f, 27.f, -8.f };

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
constexpr Vector3 Left3 = { 1.f, 0.f, 0.f };
constexpr Vector3 Up3 = { 0.f, 1.f, 0.f };

namespace SimTimeData {
	constexpr uint32_t TargetFPS = 120;
	constexpr float DeltaTime = 1.f / TargetFPS;
}

namespace SpaceshipData {
	constexpr float MinThrust = 1.f;
	constexpr float Thrust = 3.f;
	constexpr float LinearDrag = 1e-6;
	constexpr float QuadraticDrag = 1e-2;

	constexpr float Yaw = 0.01f;
	constexpr float Pitch = 1.5f;
	constexpr float Roll = 1.25f;

	constexpr float SteerB = 0.25f;
	constexpr float SteerM = 1.f;
}

struct SteerComponent {
	float Steer;
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
	for (auto entity : registry.view<PositionComponent>()) {
		auto& position = registry.get<PositionComponent>(entity);
		auto& orientation = registry.get<OrientationComponent>(entity);
		DrawSpaceShip(position.Position, orientation.Quaternion);
	}
	EndMode3D();
	EndDrawing();
}

RelativeInput UpdateInput(const Camera& camera) {
	Vector2 pos = { camera.position.x, camera.position.z };
	Vector2 tar = { camera.target.x, camera.target.z };
	Vector2 to = Vector2Subtract(tar, pos);
	Vector2 normalizedTo = Vector2Normalize(to);
	Vector2 normalizedOrthogonal = { -normalizedTo.y, normalizedTo.x };

	Vector2 input = { 0.f, 0.f };
	if (IsGamepadAvailable(0)) {
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

	return { forward, left };
}

static entt::registry SetupSim() {
	entt::registry registry;
	registry.reserve(2000);

	entt::entity player = registry.create();
	registry.emplace<SteerComponent>(player, 0.f);
	registry.emplace<PositionComponent>(player, 0.f, 2.f, 0.f);
	registry.emplace<VelocityComponent>(player, 0.f, 0.f, 0.f);
	registry.emplace<OrientationComponent>(player, QuaternionIdentity());

	return registry;
}

static Vector3 HorizontalOrthogonal(const Vector3& vector) {
	return { -vector.z, vector.y, vector.x };
}

static void Simulate(entt::registry& registry, RelativeInput input) {
	const float deltaTime = SimTimeData::DeltaTime;
	constexpr float Speed = 12.f;

	auto playerView = registry.view<PositionComponent, VelocityComponent, OrientationComponent, SteerComponent>();
	auto playerProcess = [input, deltaTime](entt::entity entity,
		PositionComponent& positionComponent,
		VelocityComponent& velocityComponent,
		OrientationComponent& orientationComponent,
		SteerComponent& steerComponent) {
			Vector3 inputTarget = { input.Left, 0.f, input.Forward };
			float inputLength = Vector3Length(inputTarget);

			Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);

			Vector3 inputDirection = forward;
			if (!FloatEquals(inputLength, 0.f)) {
				inputDirection = Vector3Scale(inputTarget, 1.f / inputLength);
			}

			Vector3 acceleration = Vector3Scale(forward, (SpaceshipData::MinThrust + SpaceshipData::Thrust * inputLength) * deltaTime);
			Vector3 velocity = Vector3Add(velocityComponent.Velocity, acceleration);

			float speed = Vector3Length(velocity);
			if (!FloatEquals(speed, 0.f)) {
				float drag = speed * SpaceshipData::LinearDrag + speed * speed * SpaceshipData::QuadraticDrag;
				velocity = Vector3Add(velocity, Vector3Scale(velocity, -drag / speed));
			}

			velocityComponent.Velocity = velocity;
			Vector3 displacement = Vector3Scale(velocity, deltaTime * Speed);

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
				if (steeringSign != steerSign) {
					steer -= SpaceshipData::Roll * deltaTime;
				}
				else {
					steer += SpaceshipData::Roll * deltaTime;
					steer = std::min(steer, targetSteer);
				}
			}

			float turnAbility = cosf(steer) * SpaceshipData::Yaw + sinf(steer) * SpaceshipData::Pitch;

			steer *= steerSign;
			turnAbility *= steerSign;
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
}

void main() {
	SetupWindow();
	Camera camera = SetupCamera();
	entt::registry registry = SetupSim();
	double gameStartTime = GetTime();
	uint32_t simTicks = 0;
	while (!WindowShouldClose()) {
		auto input = UpdateInput(camera);
		while (gameStartTime + SimTimeData::DeltaTime * simTicks < GetTime()) {
			Simulate(registry, input);
			simTicks += 1;
		}
		Draw(camera, registry);
	}
	CloseWindow();
}