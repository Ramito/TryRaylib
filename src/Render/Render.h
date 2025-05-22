#pragma once

#include "DependencyContainer.h"

#include "ThreadPool/ThreadPool.h"
#include "entt/entt.hpp"
#include <Render/CameraFrustm.h>
#include <Render/RenderLists.h>
#include <raylib.h>
#include <stack>

static constexpr size_t MaxViews = 2;

using GameCameras = std::array<Camera, MaxViews>;
using ViewPorts = std::array<Rectangle, MaxViews>;

struct RenderFlag
{};
using RenderDependencies = DependencyContainer<RenderFlag>;

struct RenderTaskInput
{
    const entt::registry* SimFrame;
    Camera Camera;
    CameraRays CameraRays;
    CameraFrustum Frustum;
    Rectangle Viewport;
};

struct RenderTaskOutput
{
    Camera Camera;
    RenderLists Lists;
};

class Render
{
public:
    Render(uint32_t views, RenderDependencies& dependencies);
    ~Render();
    bool DrawScreenTexture();
    const Texture& ScreenTexture() const;
    bool TryStartRenderTasks(const entt::registry& registry);

private:
    uint32_t mViews;
    std::array<Camera, MaxViews>& mCameras;
    std::array<Rectangle, MaxViews>& mViewPorts;
    std::array<RenderTexture, MaxViews> mViewPortTextures;
    RenderTexture mScreenTexture;

    Texture mGlowTexture;

    ThreadPool mThreadPool;
    struct RenderTaskBundle
    {
        std::array<RenderTaskInput, MaxViews> Inputs;
        std::array<RenderTaskOutput, MaxViews> Outputs;
        std::vector<Task> Tasks;
    };
    std::array<RenderTaskBundle, 2> mRenderTaskBundles;

    std::stack<uint32_t> mPassiveTaskBundles;
    std::queue<uint32_t> mActiveTaskBundles;

    std::mutex mBundleMutex;
};
