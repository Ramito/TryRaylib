#pragma once

#include "DependencyContainer.h"

#include "ThreadPool/ThreadPool.h"
#include "entt/entt.hpp"
#include <raylib.h>

static constexpr size_t MaxViews = 2;

using GameCameras = std::array<Camera, MaxViews>;
using ViewPorts = std::array<Rectangle, MaxViews>;

struct RenderFlag
{};
using RenderDependencies = DependencyContainer<RenderFlag>;

struct CameraFrustum
{
    Vector3 Target;
    float TopSupport;
    Vector3 TopNormal;
    float LeftSupport;
    Vector3 LeftNormal;
    float BottomSupport;
    Vector3 BottomNormal;
    float RightSupport;
    Vector3 RightNormal;
};

struct RenderTaskSource
{
    const entt::registry* SimFrame;
    Camera MainCamera;
    Rectangle Viewport;
};

struct RenderTaskInput
{
    Camera TargetCamera;
    CameraFrustum Frustum;
};

struct RenderList
{
    std::vector<std::tuple<Vector3, Quaternion, uint32_t>> Spaceships;
    std::vector<std::tuple<Vector3, uint32_t>> Respawners;
    std::vector<std::tuple<Vector3, float>> Asteroids;
    std::vector<std::tuple<Vector3, Color>> Particles;
    std::vector<std::tuple<Vector3, Color>> Bullets;
    std::vector<std::tuple<Vector3, float, float>> Explosions;
    static constexpr uint32_t MaxBakeProgress = 5;
    std::atomic<uint32_t> BakeProgress = 0;
};

struct RenderPayload
{
    Camera MainCamera;
    RenderList MainList;
    Camera BackgroundCamera;
    RenderList BackgroundList;
};

class Render
{
public:
    Render(uint32_t views, RenderDependencies& dependencies);
    ~Render();
    void DrawScreenTexture(const entt::registry& registry);
    const Texture& ScreenTexture() const;


    static void BakeSpaceshipsRenderList(const RenderTaskSource& source,
                                         const RenderTaskInput& input,
                                         RenderList& targetList);
    static void
    BakeAsteroidsRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList);
    static void
    BakeParticlesRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList);
    static void
    BakeBulletsRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList);
    static void BakeExplosionsRenderList(const RenderTaskSource& source,
                                         const RenderTaskInput& input,
                                         RenderList& targetList);

private:
    uint32_t mViews;
    std::array<Camera, MaxViews>& mCameras;
    std::array<Rectangle, MaxViews>& mViewPorts;
    std::array<RenderTexture, MaxViews> mBackgroundTextures;
    std::array<RenderTexture, MaxViews> mBulletTextures;
    std::array<RenderTexture, MaxViews> mViewPortTextures;
    RenderTexture mScreenTexture;

    Texture mGlowTexture;

    ThreadPool mThreadPool;
    std::array<RenderTaskSource, MaxViews> mRenderTaskSources; // One per view port	NOTE: Can remove later, just need the sim frame!
    std::array<RenderTaskInput, 2 * MaxViews> mRenderTaskInputs; // Two per view port
    std::array<RenderPayload, MaxViews> mRenderPayloads;         // One per view port
    std::vector<Task> mRenderTasks;                              // A bunch!
};
