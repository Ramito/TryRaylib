#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"

static void SetupWindow() {
	SetTargetFPS(120);
	InitWindow(1600, 1000, "Game");
}

constexpr Vector3 CameraOffset = { -8.f, 20.f, -8.f };

static Camera SetupCamera() {
	Camera camera;
	camera.position = Vector3Negate(CameraOffset);
	camera.target = { 0.f, 0.f, 0.f };
	camera.projection = CAMERA_PERSPECTIVE;
	camera.up = { 0.f, 1.f, 0.f };
	camera.fovy = 70.f;
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

struct PlayerComponent {};
struct PositionComponent {
	Vector3 Position;
};
struct OrientationComponent {
	Quaternion Quaternion;
};

static void DrawSpaceShip(const Vector3& position, const Quaternion orientation) {
	constexpr float Angle = -2.f * PI / 3.f;
	static float TwoThirdsPiSin = sinf(-2.f * PI / 3.f);
	constexpr float Length = 1.25f;
	constexpr float Width = 0.6f;
	constexpr float Height = 0.6f;
	constexpr Vector3 UpShift = { 0.f, Height, 0.f };
	constexpr Vector3 DownShift = { 0.f, 2.f * Height, 0.f };

	Vector3 forward = Vector3RotateByQuaternion(Forward3, orientation);
	Vector3 left = Vector3RotateByQuaternion(Left3, orientation);
	Vector3 up = Vector3RotateByQuaternion(Up3, orientation);

	Quaternion triangleQuat = QuaternionFromAxisAngle(up, Angle);

	Vector3 vertex1 = Vector3Add(Vector3Scale(forward, Length), position);

	Vector3 relVertex = Vector3RotateByQuaternion(forward, triangleQuat);
	Vector3 vertex2 = Vector3Add(Vector3Add(Vector3Scale(relVertex, Width), position), UpShift);

	relVertex = Vector3RotateByQuaternion(relVertex, triangleQuat);
	Vector3 vertex3 = Vector3Subtract(Vector3Add(Vector3Scale(relVertex, Width), position), UpShift);

	DrawLine3D(vertex1, vertex2, RED);
	DrawLine3D(vertex2, vertex3, RED);
	DrawLine3D(vertex3, vertex1, RED);

	vertex2 = Vector3Subtract(vertex2, DownShift);
	vertex3 = Vector3Add(vertex3, DownShift);

	DrawLine3D(vertex1, vertex2, RED);
	DrawLine3D(vertex2, vertex3, RED);
	DrawLine3D(vertex3, vertex1, RED);

	Vector3 midBack = Vector3Lerp(vertex2, vertex3, 0.5f);
	DrawLine3D(vertex1, midBack, RED);
}

static void Draw(Camera& camera, entt::registry& registry) {
	for (auto playerEntity : registry.view<PlayerComponent>()) {
		auto& position = registry.get<PositionComponent>(playerEntity);
		camera.target = position.Position;
		camera.position = Vector3Add(camera.target, CameraOffset);
	}
	UpdateCamera(&camera);
	BeginDrawing();
	ClearBackground(DARKGRAY);
	BeginMode3D(camera);
	DrawGrid(500, 2.0f);
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
	registry.emplace<PlayerComponent>(player);
	registry.emplace<PositionComponent>(player, 0.f, 2.f, 0.f);
	registry.emplace<OrientationComponent>(player, QuaternionIdentity());

	return registry;
}

static void Simulate(entt::registry& registry, RelativeInput input) {
	constexpr float Speed = 12.f;
	auto playerView = registry.view<PositionComponent, OrientationComponent, PlayerComponent>();
	auto playerProcess = [input](entt::entity entity, PositionComponent& positionComponent, OrientationComponent& orientationComponent) {
		Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);
		Vector3 displacement = Vector3Scale(forward, GetFrameTime() * Speed);

		positionComponent.Position = Vector3Add(positionComponent.Position, displacement);

		Vector3 inputTarget = { input.Left, 0.f, input.Forward };
		Quaternion correctingQuaternion = QuaternionFromVector3ToVector3(forward, inputTarget);

		float delta = 2.5f * GetFrameTime();

		Quaternion clampedQuaternion = QuaternionSlerp(QuaternionIdentity(), correctingQuaternion, delta);

		orientationComponent.Quaternion = QuaternionMultiply(clampedQuaternion, orientationComponent.Quaternion);
	};
	playerView.each(playerProcess);
}

void main() {
	SetupWindow();
	Camera camera = SetupCamera();
	entt::registry registry = SetupSim();
	while (!WindowShouldClose()) {
		auto input = UpdateInput(camera);
		Simulate(registry, input);
		Draw(camera, registry);
	}
	CloseWindow();
}