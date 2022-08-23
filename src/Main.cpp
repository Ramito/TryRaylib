#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"
#include "DependencyContainer.h"
#include "Data.h"
#include "Components.h"
#include "Simulation/Simulation.h"

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
		if (!registry.any_of<OrientationComponent>(particle))
		{
			DrawPoint3D(position, ORANGE);
		}
		else {
			DrawSphere(position, 0.2f, GREEN);
		}
	}

	EndMode3D();
	EndDrawing();
}

void UpdateInput(const Camera& camera, std::array<GameInput, 4>& gameInputs) {
	Vector2 pos = { camera.position.x, camera.position.z };
	Vector2 tar = { camera.target.x, camera.target.z };
	Vector2 to = Vector2Subtract(tar, pos);
	Vector2 normalizedTo = Vector2Normalize(to);
	Vector2 normalizedOrthogonal = { -normalizedTo.y, normalizedTo.x };

	size_t idx = 0;
	for (GameInput& gameInput : gameInputs) {
		bool fire = false;
		Vector2 input = { 0.f,0.f };
		if (IsGamepadAvailable(idx)) {
			input.x = GetGamepadAxisMovement(0, 0);
			input.y = -GetGamepadAxisMovement(0, 1);
			fire = IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
		}
		else if (idx == 0)
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
			fire = IsKeyDown(KEY_SPACE);
		}

		if (Vector2LengthSqr(input) > 1.f) {
			input = Vector2Normalize(input);
		}


		float forward = Vector2DotProduct(input, normalizedTo);
		float left = Vector2DotProduct(input, normalizedOrthogonal);

		gameInput = { forward, left, fire };
	}
}

void main() {
	SetupWindow();
	Camera camera = SetupCamera();

	auto gameInput = std::make_shared<std::array<GameInput, 4>>();

	SimDependencies dependencies;
	entt::registry& simRegistrry = dependencies.CreateDependency<entt::registry>();
	dependencies.AddDependency(gameInput);	// This should be owned elsewhere

	Simulation sim(dependencies);
	sim.Init();

	double gameStartTime = GetTime();
	uint32_t simTicks = 0;
	uint32_t ticksPerPass = 1;
	while (!WindowShouldClose()) {
		UpdateInput(camera, *gameInput);
		uint32_t counter = ticksPerPass;
		while (counter-- > 0) {
			sim.Tick();
			simTicks += 1;
		}
		if (gameStartTime + SimTimeData::DeltaTime * (simTicks + 1) <= GetTime()) {
			ticksPerPass += 1;
		}
		else {
			ticksPerPass = 1;
		}
		Draw(camera, simRegistrry);
	}
	CloseWindow();
}