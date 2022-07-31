#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"

static void SetupWindow() {
	SetTargetFPS(120);
	InitWindow(1600, 1000, "Game");
}

constexpr Vector3 CameraOffset = { -6.f, 12.5f, -6.f };

static Camera SetupCamera() {
	Camera camera;
	camera.position = Vector3Negate(CameraOffset);
	camera.target = { 0.f, 0.f, 0.f };
	camera.projection = CAMERA_PERSPECTIVE;
	camera.up = { 0.f, 1.f, 0.f };
	camera.fovy = 75.f;
	SetCameraMode(camera, CAMERA_CUSTOM);
	return camera;
}

struct RelativeInput {
	float X;
	float Z;
};

struct PlayerComponent {};
struct PositionComponent {
	float X;
	float Y;
	float Z;
};
struct OrientationComponent {
	float Angle;
};

static void DrawSpaceShip(const Vector3& position, float orientation) {
	static float TwoThirdsPiCos = cosf(-2.f * PI / 3.f);
	static float TwoThirdsPiSin = sinf(-2.f * PI / 3.f);
	constexpr float Size = 2.f;

	float x1 = cosf(orientation);
	float y1 = sinf(orientation);
	Vector3 forward = { x1 * Size, 0.f, y1 * Size };

	float x2 = (TwoThirdsPiCos * x1) - (TwoThirdsPiSin * y1);
	float y2 = (TwoThirdsPiSin * x1) + (TwoThirdsPiCos * y1);
	Vector3 left = { x2 * Size, 0.f, y2 * Size };

	float x3 = (TwoThirdsPiCos * x2) - (TwoThirdsPiSin * y2);
	float y3 = (TwoThirdsPiSin * x2) + (TwoThirdsPiCos * y2);
	Vector3 right = { x3 * Size, 0.f, y3 * Size };

	Vector3 vertex1 = Vector3Add(forward, position);
	Vector3 vertex2 = Vector3Add(left, position);
	Vector3 vertex3 = Vector3Add(right, position);

	DrawLine3D(vertex1, position, RED);
	DrawLine3D(vertex2, position, GREEN);
	DrawLine3D(vertex3, position, BLUE);

	DrawCube(position, 0.25f, 0.25f, 0.25f, WHITE);
}

static void Draw(Camera& camera, entt::registry& registry) {
	for (auto playerEntity : registry.view<PlayerComponent>()) {
		auto& position = registry.get<PositionComponent>(playerEntity);
		camera.target = { position.X, position.Y, position.Z };
		camera.position = Vector3Add(camera.target, CameraOffset);
	}
	UpdateCamera(&camera);
	BeginDrawing();
	ClearBackground(BLACK);
	BeginMode3D(camera);
	DrawGrid(500, 2.0f);
	for (auto entity : registry.view<PositionComponent>()) {
		auto& position = registry.get<PositionComponent>(entity);
		auto& orientation = registry.get<OrientationComponent>(entity);
		DrawSpaceShip({ position.X, position.Y, position.Z }, orientation.Angle);
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

	Vector2 input = {};
	if (IsGamepadAvailable(0)) {
		input.x = -GetGamepadAxisMovement(0, 0);
		input.y = GetGamepadAxisMovement(0, 1);
		// No way to get camera pan working with pad?
	}
	else {
		if (IsKeyDown(KEY_A)) {
			input.x += 1.f;
		}
		if (IsKeyDown(KEY_D)) {
			input.x -= 1.f;
		}
		if (IsKeyDown(KEY_S)) {
			input.y += 1.f;
		}
		if (IsKeyDown(KEY_W)) {
			input.y -= 1.f;
		}
		input = Vector2Normalize(input);
	}


	float cos = Vector2DotProduct(input, normalizedTo);
	float sin = Vector2DotProduct(input, normalizedOrthogonal);

	return { -sin, -cos };
}

static entt::registry SetupSim() {
	entt::registry registry;
	registry.reserve(2000);

	entt::entity player = registry.create();
	registry.emplace<PlayerComponent>(player);
	registry.emplace<PositionComponent>(player, 0.f, 0.f, 0.f);
	registry.emplace<OrientationComponent>(player, 0.f);

	return registry;
}

static void Simulate(entt::registry& registry, RelativeInput input) {
	constexpr float Speed = 15.f;
	auto playerView = registry.view<PositionComponent, OrientationComponent, PlayerComponent>();
	auto playerProcess = [input](entt::entity entity, PositionComponent& positionComponent, OrientationComponent& orientationComponent) {
		float dirX = cosf(orientationComponent.Angle);
		float dirZ = sinf(orientationComponent.Angle);

		float speed = Speed * Vector2Length({ input.X, input.Z }) * GetFrameTime();

		positionComponent.X += speed * dirX;
		positionComponent.Z += speed * dirZ;

		float lateralProjection = Vector2DotProduct({ input.X, input.Z }, { -dirZ, dirX });
		if (FloatEquals(lateralProjection, 0.f)) {

			return;
		}
		float delta = 1.5f * GetFrameTime();
		if (lateralProjection < 0.f)
		{
			delta *= -1;
		}
		orientationComponent.Angle += delta;
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