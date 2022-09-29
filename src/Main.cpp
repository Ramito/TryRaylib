#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include "entt/entt.hpp"
#include "DependencyContainer.h"
#include "Data.h"
#include "Components.h"
#include "Simulation/Simulation.h"
#include "Render/Render.h"
#include "Menu.h"
#include <mutex>
#include <stack>
#include <queue>
#include <thread>
#include <stop_token>

static void SetupWindow() {
	int targetWidth = GetScreenWidth() * 2 / 3;
	int targetHeight = GetScreenHeight() * 2 / 3;
	SetConfigFlags(FLAG_WINDOW_RESIZABLE);
	InitWindow(targetWidth, targetHeight, "Game");
	SetConfigFlags(ConfigFlags::FLAG_WINDOW_UNDECORATED);
	SetTargetFPS(GetMonitorRefreshRate(GetCurrentMonitor()));
}

void UpdateInput(const std::array<Camera, MaxViews>& cameras, std::array<GameInput, MaxViews>& gameInputs) {
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

static void SetViewports(size_t count, ViewPorts& viewPorts) {
	viewPorts[0] = { 0, 0, (float)GetScreenWidth() / count, (float)GetScreenHeight() };
	viewPorts[1] = { (float)GetScreenWidth() / count, 0, (float)GetScreenWidth() / count, (float)GetScreenHeight() };
}

static void UpdateCameras(entt::registry& registry, GameCameras& gameCameras) {
	for (auto playerEntity : registry.view<PositionComponent, SpaceshipInputComponent>()) {
		auto& input = registry.get<SpaceshipInputComponent>(playerEntity);
		auto& position = registry.get<PositionComponent>(playerEntity);
		const Vector3 target = Vector3Add(position.Position, CameraData::TargetOffset);
		gameCameras[input.InputId].target = target;
		gameCameras[input.InputId].position = Vector3Add(target, CameraData::CameraOffset);
	}
}

int main() {
	SetupWindow();
	auto gameInput = std::make_shared<std::array<GameInput, MaxViews>>();
	auto gameCameras = std::make_shared<GameCameras>();
	auto viewPorts = std::make_shared<ViewPorts>();

	SetViewports(1, *viewPorts);

	SimDependencies simDependencies;
	entt::registry& simRegistry = simDependencies.CreateDependency<entt::registry>();
	simDependencies.AddDependency(gameInput);	// This should be owned elsewhere

	std::unique_ptr<Simulation> sim = std::make_unique<Simulation>(simDependencies);
	sim->Init(0);

	RenderDependencies renderDependencies;
	renderDependencies.AddDependency(gameCameras);
	renderDependencies.AddDependency(viewPorts);


	std::unique_ptr<Render> render = std::make_unique<Render>(1, renderDependencies);

	Menu menu;

	std::mutex transferMutex;
	std::array<entt::registry, 2> simSnapShots;
	std::stack<uint32_t> writeReadySnapshots;
	std::queue<uint32_t> renderReadySnapshots;

	uint32_t renderSnapshot = 1;
	writeReadySnapshots.push(0);

	auto simThreadProcess = [&](std::stop_token sToken) {
		double gameStartTime = GetTime();
		uint32_t simTicks = 0;
		while (!sToken.stop_requested()) {
			UpdateInput(*gameCameras, *gameInput);
			sim->Tick();
			simTicks += 1;

			uint32_t snapshot = simSnapShots.size();
			if (!writeReadySnapshots.empty()) {
				std::scoped_lock lock(transferMutex);
				snapshot = writeReadySnapshots.top();
				writeReadySnapshots.pop();
			}

			if (snapshot < simSnapShots.size()) {
				sim->WriteRenderState(simSnapShots[snapshot]);
				{
					std::scoped_lock lock(transferMutex);
					renderReadySnapshots.push(snapshot);
				}
			}

			double gameTime = gameStartTime + SimTimeData::DeltaTime * simTicks;
			if (auto waitTime = gameTime - GetTime(); waitTime > 0.0) {
				std::chrono::duration<double> sleepDuration(waitTime);
				std::this_thread::sleep_for(sleepDuration);
			}
		}
	};

	std::unique_ptr<std::jthread> simThread = std::make_unique<std::jthread>(simThreadProcess);

	auto startGameAction = [&](uint32_t players) {
		simThread.reset();

		sim = std::make_unique<Simulation>(simDependencies);
		sim->Init(players);
		SetViewports(players, *viewPorts);
		render = std::make_unique<Render>(players, renderDependencies);

		simThread = std::make_unique<std::jthread>(simThreadProcess);
	};

	while (!WindowShouldClose()) {
		menu.UpdateMenu(startGameAction);
		if (!renderReadySnapshots.empty()) {
			simSnapShots[renderSnapshot].clear();
			{
				std::scoped_lock lock(transferMutex);
				writeReadySnapshots.push(renderSnapshot);
				renderSnapshot = renderReadySnapshots.front();
				renderReadySnapshots.pop();
			}
			UpdateCameras(simSnapShots[renderSnapshot], *gameCameras);
			render->DrawScreenTexture(simSnapShots[renderSnapshot]);
		}


		BeginDrawing();
		DrawTextureRec(render->ScreenTexture(), { 0.f, 0.f, (float)GetScreenWidth(), -(float)GetScreenHeight() }, {}, WHITE);
		menu.DrawMenu();
		EndDrawing();
	}

	simThread.reset();

	CloseWindow();
}
