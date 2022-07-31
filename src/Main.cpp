#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"

static void SetupWindow() {
	SetTargetFPS(60);
	InitWindow(1200, 600, "Game");
}

static Camera SetupCamera() {
	Camera camera;
	camera.position = { 10.f,10.f,10.f };
	camera.target = { 0.f, 0.f, 0.f };
	camera.projection = CAMERA_PERSPECTIVE;
	camera.up = { 0.f, 1.f, 0.f };
	camera.fovy = 60.f;	// NEAR PLANE IF ORTHOGRAPHIC
	SetCameraMode(camera, CAMERA_THIRD_PERSON);
	return camera;
}

struct RelativeInput {
	float x;
	float z;
};

struct PlayerComponent {};
struct PositionComponent {
	float x;
	float y;
	float z;
};

static void Draw(Camera& camera, entt::registry& registry) {
	for (auto playerEntity : registry.view<PlayerComponent>()) {
		auto position = registry.get<PositionComponent>(playerEntity);
		camera.target = { position.x, position.y, position.z };
	}
	UpdateCamera(&camera);
	BeginDrawing();
	ClearBackground(GRAY);
	BeginMode3D(camera);
	DrawGrid(100, 2.0f);
	for (auto entity : registry.view<PositionComponent>()) {
		auto position = registry.get<PositionComponent>(entity);
		DrawCube({ position.x, position.y, position.z }, 1.f, 1.f, 1.f, RED);
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

	Vector2 input;
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

	return registry;
}

static void Simulate(entt::registry& registry, RelativeInput input) {
	auto playerView = registry.view<PositionComponent, PlayerComponent>();
	auto playerProcess = [input](entt::entity entity, PositionComponent& positionComponent) {
		positionComponent.x += 0.1f * input.x;
		positionComponent.z += 0.1f * input.z;
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