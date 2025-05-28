#include "Components.h"
#include "Data.h"
#include "DependencyContainer.h"
#include "Menu.h"
#include "Render/Render.h"
#include "Simulation/Simulation.h"
#include <tracy/Tracy.hpp>
#include "entt/entt.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <Render/RenderLists.h>
#include <mutex>
#include <queue>
#include <stack>
#include <stop_token>
#include <thread>

static void SetupWindow()
{
    InitWindow(0, 0, "Game");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED | FLAG_VSYNC_HINT);
    const int display = GetCurrentMonitor();
    const int targetWidth = GetMonitorWidth(display);
    const int targetHeight = GetMonitorHeight(display);
    SetWindowPosition(0, 0);
    SetWindowSize(targetWidth, targetHeight);
    SetTargetFPS(GetMonitorRefreshRate(display));
}

void UpdateInput(const std::array<Camera, MaxViews>& cameras, std::array<GameInput, MaxViews>& gameInputs)
{
    ZoneScopedN("Update Input");
    for (int idx = 0; idx < gameInputs.size(); ++idx) {
        GameInput& gameInput = gameInputs[idx];
        const Camera& camera = cameras[idx];

        const Vector2 pos = {camera.position.x, camera.position.z};
        const Vector2 tar = {camera.target.x, camera.target.z};
        const Vector2 to = Vector2Subtract(tar, pos);
        const Vector2 normalizedTo = Vector2Normalize(to);
        const Vector2 normalizedOrthogonal = {-normalizedTo.y, normalizedTo.x};

        bool fire = false;
        Vector2 input = {0.f, 0.f};
        Vector2 secondaryInput = {0.f, 0.f};
        if (IsGamepadAvailable(idx)) {
            input.x = GetGamepadAxisMovement(idx, GAMEPAD_AXIS_LEFT_X);
            input.y = -GetGamepadAxisMovement(idx, GAMEPAD_AXIS_LEFT_Y);

            secondaryInput.x = GetGamepadAxisMovement(idx, GAMEPAD_AXIS_RIGHT_X);
            secondaryInput.y = -GetGamepadAxisMovement(idx, GAMEPAD_AXIS_RIGHT_Y);

            fire = IsGamepadButtonDown(idx, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);
        } else if (idx == 0) {
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

        if (Vector2LengthSqr(secondaryInput) > 1.f) {
            secondaryInput = Vector2Normalize(secondaryInput);
        }

        float forward = Vector2DotProduct(input, normalizedTo);
        float left = Vector2DotProduct(input, normalizedOrthogonal);

        float secondaryForward = Vector2DotProduct(secondaryInput, normalizedTo);
        float secondaryLeft = Vector2DotProduct(secondaryInput, normalizedOrthogonal);

        gameInput = {forward, left, secondaryForward, secondaryLeft, fire};
    }
}

static void SetViewports(size_t count, ViewPorts& viewPorts)
{
    viewPorts[0] = {0, 0, (float)GetScreenWidth() / count, (float)GetScreenHeight()};
    viewPorts[1] = {(float)GetScreenWidth() / count, 0, (float)GetScreenWidth() / count,
                    (float)GetScreenHeight()};
}

static void SetCameraUp(Camera& camera)
{
    const Vector3 toTarget = Vector3Subtract(camera.target, camera.position);
    camera.up =
    Vector3Normalize(Vector3CrossProduct(Vector3CrossProduct(toTarget, {0.f, 1.f, 0.f}), toTarget));
}

static void UpdateCameras(entt::registry& registry, GameCameras& gameCameras)
{
    ZoneScopedN("Update Cameras");
    for (auto playerEntity : registry.view<PositionComponent, SpaceshipInputComponent>()) {
        const auto& input = registry.get<SpaceshipInputComponent>(playerEntity);
        auto& position = registry.get<PositionComponent>(playerEntity);
        const Vector3 target = Vector3Add(position.Position, CameraData::TargetOffset);
        gameCameras[input.InputId].target = target;
        gameCameras[input.InputId].position = Vector3Add(target, CameraData::CameraOffset);
    }
    for (auto playerEntity : registry.view<PositionComponent, RespawnComponent>()) {
        const auto& respawn = registry.get<RespawnComponent>(playerEntity);
        if (respawn.TimeLeft > 0.f) {
            continue;
        }
        auto& position = registry.get<PositionComponent>(playerEntity);
        const Vector3 target = Vector3Add(position.Position, CameraData::TargetOffset);
        gameCameras[respawn.InputId].target = target;
        gameCameras[respawn.InputId].position = Vector3Add(target, CameraData::CameraOffset);
    }

    for (Camera& camera : gameCameras) {
        SetCameraUp(camera);
    }
}

int main()
{
    SetupWindow();
    auto gameInput = std::make_shared<std::array<GameInput, MaxViews>>();
    auto gameCameras = std::make_shared<GameCameras>();
    auto viewPorts = std::make_shared<ViewPorts>();

    SetViewports(1, *viewPorts);

    SimDependencies simDependencies;
    entt::registry& simRegistry = simDependencies.CreateDependency<entt::registry>();
    simDependencies.AddDependency(gameInput); // This should be owned elsewhere

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
    std::queue<uint32_t> presentReadySnapshots;

    uint32_t renderSnapshotId = 1;
    writeReadySnapshots.push(0);

    std::condition_variable presentCondition;

    auto simThreadProcess = [&](std::stop_token sToken) {
        double gameStartTime = GetTime();
        uint32_t simTicks = 0;
        while (!sToken.stop_requested()) {
            UpdateInput(*gameCameras, *gameInput);
            sim->Tick();
            simTicks += 1;

            uint32_t snapshotId = simSnapShots.size();
            if (!writeReadySnapshots.empty()) {
                std::scoped_lock lock(transferMutex);
                snapshotId = writeReadySnapshots.top();
                writeReadySnapshots.pop();
            }

            if (snapshotId < simSnapShots.size()) {
                sim->WriteRenderState(simSnapShots[snapshotId]);
                {
                    std::scoped_lock lock(transferMutex);
                    presentReadySnapshots.push(snapshotId);
                }
                presentCondition.notify_one();
            }

            double gameTime = gameStartTime + SimTimeData::DeltaTime * simTicks;
            if (auto waitTime = gameTime - GetTime(); waitTime > 0.0) {
                ZoneScopedNC("Sleep", 0x7777AAFF);
                std::chrono::duration<double> sleepDuration(waitTime);
                std::this_thread::sleep_for(sleepDuration);
            }
        }
    };

    auto presentThreadProcess = [&](std::stop_token sToken) {
        RenderLists pendingList;
        while (!sToken.stop_requested()) {
            {
                ZoneScopedNC("Wait", 0x7777AAFF);
                std::unique_lock lock(transferMutex);
                presentCondition.wait(lock, [&]() {
                    return sToken.stop_requested() || !presentReadySnapshots.empty();
                });
                if (presentReadySnapshots.empty()) {
                    continue;
                }
                renderSnapshotId = presentReadySnapshots.front();
                presentReadySnapshots.pop();
            }
            UpdateCameras(simSnapShots[renderSnapshotId], *gameCameras);
            while (!render->TryStartRenderTasks(simSnapShots[renderSnapshotId]) && !sToken.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::duration<float>(0.001f));
            }
            simSnapShots[renderSnapshotId].clear();
            {
                std::scoped_lock lock(transferMutex);
                writeReadySnapshots.push(renderSnapshotId);
            }
        }
    };

    std::unique_ptr<std::jthread> simThread = std::make_unique<std::jthread>(simThreadProcess);
    std::unique_ptr<std::jthread> presentThread = std::make_unique<std::jthread>(presentThreadProcess);

    auto startGameAction = [&](uint32_t players) {
        simThread.reset();
        presentThread->request_stop();
        presentCondition.notify_all();
        presentThread.reset();

        sim = std::make_unique<Simulation>(simDependencies);
        sim->Init(players);
        SetViewports(players, *viewPorts);
        render = std::make_unique<Render>(players, renderDependencies);

        simThread = std::make_unique<std::jthread>(simThreadProcess);
        presentThread = std::make_unique<std::jthread>(presentThreadProcess);
    };

    while (!WindowShouldClose()) {
        ZoneScopedN("Main Loop");
        if (!render->DrawScreenTexture()) {
            continue;
        }

        menu.UpdateMenu(startGameAction);

        BeginDrawing();
        DrawTextureRec(render->ScreenTexture(),
                       {0.f, 0.f, (float)GetScreenWidth(), -(float)GetScreenHeight()}, {}, WHITE);
        menu.DrawMenu();
        EndDrawing();
    }

    simThread.reset();
    presentThread->request_stop();
    presentCondition.notify_all();
    presentThread.reset();

    CloseWindow();
}
