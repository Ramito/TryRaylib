#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"
#include "DependencyContainer.h"
#include "Data.h"
#include "Components.h"
#include "Simulation/Simulation.h"
#include "Render/Render.h"

static void SetupWindow() {
	SetTargetFPS(SimTimeData::TargetFPS);
	InitWindow(2560, 1440, "Game");
	SetConfigFlags(ConfigFlags::FLAG_WINDOW_UNDECORATED | ConfigFlags::FLAG_WINDOW_MAXIMIZED | ConfigFlags::FLAG_WINDOW_TOPMOST);
}

void UpdateInput(const std::array<Camera, 4>& cameras, std::array<GameInput, 4>& gameInputs) {
	for (size_t idx = 0; idx < gameInputs.size(); ++idx) {
		GameInput& gameInput = gameInputs[idx];
		const Camera& camera = cameras[idx];

		Vector2 pos = { camera.position.x, camera.position.z };
		Vector2 tar = { camera.target.x, camera.target.z };
		Vector2 to = Vector2Subtract(tar, pos);
		Vector2 normalizedTo = Vector2Normalize(to);
		Vector2 normalizedOrthogonal = { -normalizedTo.y, normalizedTo.x };

		bool fire = false;
		Vector2 input = { 0.f,0.f };
		if (IsGamepadAvailable(idx)) {
			input.x = GetGamepadAxisMovement(idx, 0);
			input.y = -GetGamepadAxisMovement(idx, 1);
			fire = IsGamepadButtonDown(idx, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
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
	auto gameInput = std::make_shared<std::array<GameInput, 4>>();
	auto gameCameras = std::make_shared<GameCameras>();
	auto viewPorts = std::make_shared<ViewPorts>();

	constexpr uint32_t Players = 2;

	(*viewPorts)[0] = { 0, 0, (float)GetScreenWidth() / Players, (float)GetScreenHeight() };
	(*viewPorts)[1] = { (float)GetScreenWidth() / Players, 0, (float)GetScreenWidth() / Players, (float)GetScreenHeight() };

	SimDependencies simDependencies;
	entt::registry& simRegistrry = simDependencies.CreateDependency<entt::registry>();
	simDependencies.AddDependency(gameInput);	// This should be owned elsewhere

	Simulation sim(simDependencies);
	sim.Init(Players);

	RenderDependencies renderDependencies;
	simDependencies.ShareDependencyWith<entt::registry>(renderDependencies);
	renderDependencies.AddDependency(gameCameras);
	renderDependencies.AddDependency(viewPorts);

	Render render(Players, renderDependencies);

	double gameStartTime = GetTime();
	uint32_t simTicks = 0;
	uint32_t ticksPerPass = 1;
	while (!WindowShouldClose()) {
		double currentGameTime = GetTime();
		double lastSimTickTime = gameStartTime + SimTimeData::DeltaTime * simTicks;
		if (currentGameTime <= lastSimTickTime) {
			if (ticksPerPass > 1) {
				ticksPerPass -= 1;
			}
			continue;
		}
		UpdateInput(*gameCameras, *gameInput);
		uint32_t counter = ticksPerPass;
		while (counter-- > 0) {
			sim.Tick();
		}
		simTicks += ticksPerPass;
		lastSimTickTime += SimTimeData::DeltaTime * ticksPerPass;
		if (lastSimTickTime < currentGameTime) {
			ticksPerPass += 1;
		}
		render.Draw(sim.GameTime);
	}
	CloseWindow();
}