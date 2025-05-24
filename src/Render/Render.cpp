#include "Render.h"

#include "Components.h"
#include "Data.h"
#include "FrustumPlaneData.h"
#include "SpaceUtil.h"
#include "Tracy.hpp"
#include <optional>
#include <raymath.h>
#include <rlgl.h>

namespace {
CameraRays ComputeRays(const Camera& camera, const Rectangle& viewPort)
{
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    float minX = (screenWidth - viewPort.width) / 2;
    float minY = (screenHeight - viewPort.height) / 2;
    float maxX = minX + viewPort.width;
    float maxY = minY + viewPort.height;

    // TODO: Ray positions are redundant!

    Ray minMinRay = GetMouseRay(Vector2{minX, minY}, camera);
    Ray minMaxRay = GetMouseRay(Vector2{minX, maxY}, camera);
    Ray maxMaxRay = GetMouseRay(Vector2{maxX, maxY}, camera);
    Ray maxMinRay = GetMouseRay(Vector2{maxX, minY}, camera);

    return {minMinRay, minMaxRay, maxMaxRay, maxMinRay};
}

CameraFrustum ComputeFrustum(const Camera& camera, const CameraRays& cameraRays)
{
    const Ray& minMinRay = cameraRays[0];
    const Ray& minMaxRay = cameraRays[1];
    const Ray& maxMaxRay = cameraRays[2];
    const Ray& maxMinRay = cameraRays[3];

    Vector3 anchor = camera.position;
    Vector3 topNormal = Vector3Normalize(Vector3CrossProduct(maxMinRay.direction, minMinRay.direction));
    Vector3 leftNormal = Vector3Normalize(Vector3CrossProduct(minMinRay.direction, minMaxRay.direction));
    Vector3 bottomNormal = Vector3Normalize(Vector3CrossProduct(minMaxRay.direction, maxMaxRay.direction));
    Vector3 rightNormal = Vector3Normalize(Vector3CrossProduct(maxMaxRay.direction, maxMinRay.direction));

    float topAnchor = Vector3DotProduct(topNormal, anchor);
    float leftAnchor = Vector3DotProduct(leftNormal, anchor);
    float bottomAnchor = Vector3DotProduct(bottomNormal, anchor);
    float rightAnchor = Vector3DotProduct(rightNormal, anchor);

    return {camera.target, topAnchor,    topNormal,   leftAnchor, leftNormal,
            bottomAnchor,  bottomNormal, rightAnchor, rightNormal};
}

constexpr Color SpaceColor = {40, 40, 50, 255};

void DrawSpaceShip(const Vector3& position, const Quaternion& orientation, const Color color)
{
    constexpr float scale = 0.65f;

    std::vector<Vector3> vertices = {
    Vector3{0.f, 0.f, 2.f * scale},               // 0 : nose
    Vector3{-1.25f * scale, 0.f, -scale},         // 1 : wingL
    Vector3{1.25f * scale, 0.f, -scale},          // 2 : wingR
    Vector3{0.f, 0.f, 0.f},                       // 3 : center
    Vector3{0.f, 0.f, -scale},                    // 4 : tail
    Vector3{0.f, scale * 1.5f, -1.5f * scale},    // 5 : finT
    Vector3{0.f, -0.75f * scale, -0.75f * scale}, // 6 : finB
    };

    const std::vector<std::array<int, 3>> triangles = {{0, 1, 2}, {3, 4, 5}, {0, 4, 6}};

    for (Vector3& vertex : vertices) {
        vertex = Vector3RotateByQuaternion(vertex, orientation);
    }

    for (Vector3& vertex : vertices) {
        vertex = Vector3Add(vertex, position);
    }

    rlBegin(RL_TRIANGLES);
    rlColor4ub(SpaceColor.r, SpaceColor.g, SpaceColor.b, SpaceColor.a);

    for (const auto& triangle : triangles) {
        rlVertex3f(vertices[triangle[0]].x, vertices[triangle[0]].y, vertices[triangle[0]].z);
        rlVertex3f(vertices[triangle[1]].x, vertices[triangle[1]].y, vertices[triangle[1]].z);
        rlVertex3f(vertices[triangle[2]].x, vertices[triangle[2]].y, vertices[triangle[2]].z);

        rlVertex3f(vertices[triangle[2]].x, vertices[triangle[2]].y, vertices[triangle[2]].z);
        rlVertex3f(vertices[triangle[1]].x, vertices[triangle[1]].y, vertices[triangle[1]].z);
        rlVertex3f(vertices[triangle[0]].x, vertices[triangle[0]].y, vertices[triangle[0]].z);
    }
    rlEnd();

    rlBegin(RL_LINES);
    rlColor4ub(color.r, color.g, color.b, color.a);
    for (const auto& triangle : triangles) {
        rlVertex3f(vertices[triangle[0]].x, vertices[triangle[0]].y, vertices[triangle[0]].z);
        rlVertex3f(vertices[triangle[1]].x, vertices[triangle[1]].y, vertices[triangle[1]].z);
        rlVertex3f(vertices[triangle[1]].x, vertices[triangle[1]].y, vertices[triangle[1]].z);
        rlVertex3f(vertices[triangle[2]].x, vertices[triangle[2]].y, vertices[triangle[2]].z);
        rlVertex3f(vertices[triangle[2]].x, vertices[triangle[2]].y, vertices[triangle[2]].z);
        rlVertex3f(vertices[triangle[0]].x, vertices[triangle[0]].y, vertices[triangle[0]].z);
    }
    rlEnd();
}

const std::array<Color, 2> PlayerColors = {RED, BLUE};

void DrawRespawns(const RenderLists& lists)
{
    ZoneScoped;
    for (const auto& [position, inputID] : lists.Respawners) {
        DrawCircle3D(position,
                     RespawnData::MarkerRadius * 0.5f * (1.f + sin(GetTime() * RespawnData::MarkerFrequency)),
                     Left3, 90.f, PlayerColors[inputID]);
    }
}

void DrawSpaceships(const RenderLists& lists)
{
    ZoneScoped;
    for (const auto& [position, orientation, inputID] : lists.Spaceships) {
        DrawSpaceShip(position, orientation, PlayerColors[inputID]);
    }
}

void DrawExplosions(const RenderLists& lists)
{
    for (const auto& [position, radius, relativeRadius] : lists.Explosions) {
        const unsigned char alpha = static_cast<unsigned char>(rintf(cbrt(1.f - relativeRadius) * 255));
        Color color = {255, 255, 255, alpha};
        DrawSphere(position, radius, color);
    }
}

void DrawBullets(const Camera& camera, const Texture& glow, const RenderLists& lists)
{
    ZoneScoped;

    BeginBlendMode(BLEND_ADDITIVE);

    const Rectangle source = {0.0f, 0.0f, (float)glow.width, (float)glow.height};
    const Vector2 size = {1.5f, 1.5f};

    const Vector3 toTarget = Vector3Subtract(camera.target, camera.position);
    const Vector3 up = camera.up;

    for (const auto& [position, color] : lists.Bullets) {
        DrawBillboardPro(camera, glow, source, position, up, size, Vector2Scale(size, 0.5f), 0.f, color);
    }

    EndBlendMode();
}

void DrawAsteroids(const RenderLists& lists, const Model& asteroidModel, Shader& shader, const Camera3D& camera)
{
    ZoneScoped;

    SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position.x, SHADER_UNIFORM_VEC3);

    for (const auto& [position, radius] : lists.Asteroids) {
        DrawModel(asteroidModel, position, radius, GRAY);
    }
}

void DrawParticles(const RenderLists& lists)
{
    ZoneScoped;
    for (const auto& [position, color] : lists.Particles) {
        DrawPoint3D(position, color);
    }
}

Vector3 BackgroundOffset(const Camera& camera)
{
    return Vector3Subtract({SpaceData::LengthX * 0.5f, 0.f, SpaceData::LengthX * 0.5f},
                           Vector3Scale(CameraData::CameraOffset, 0.9f));
}

void SetShader(Shader& shader)
{
    shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");

    int ambientLoc = GetShaderLocation(shader, "ambient");
    const float ambienLight = 0.25f;
    constexpr std::array<float, 4> ambientInput = {ambienLight, ambienLight, ambienLight, 1.0f};
    SetShaderValue(shader, ambientLoc, ambientInput.data(), SHADER_UNIFORM_VEC4);

    const float fogDensity = 0.00725f;
    int fogDensityLoc = GetShaderLocation(shader, "fogDensity");
    SetShaderValue(shader, fogDensityLoc, &fogDensity, SHADER_UNIFORM_FLOAT);
}
} // namespace

Render::Render(uint32_t views, RenderDependencies& dependencies)
: mViews(views), mCameras(dependencies.GetDependency<GameCameras>()),
  mViewPorts(dependencies.GetDependency<ViewPorts>()), mThreadPool(2)
{
    for (Camera& camera : mCameras) {
        camera.projection = CAMERA_PERSPECTIVE;
        camera.up = {0.f, 1.f, 0.f};
        camera.fovy = 60.f;
        camera.target = {};
        camera.position = CameraData::CameraOffset;
    }

    for (size_t i = 0; i < mViewPorts.size(); ++i) {
        int width = mViewPorts[i].width;
        int height = mViewPorts[i].height;
        mViewPortTextures[i] = LoadRenderTexture(width, height);
    }

    mScreenTexture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    Image glowImage =
    GenImageGradientRadial(GetScreenWidth() / 16, GetScreenWidth() / 16, 0.05f, WHITE, BLANK);
    mGlowTexture = LoadTextureFromImage(glowImage);

    mFowShader = LoadShader("resources/lighting.vs", "resources/fog.fs");
    SetShader(mFowShader);

    mAsteroidModel = LoadModelFromMesh(GenMeshSphere(1.f, 16, 16));
    mAsteroidModel.materials[0].shader = mFowShader;

    for (auto& renderBundle : mRenderTaskBundles) {
        for (size_t i = 0; i < mViews; ++i) {
            renderBundle.Tasks.push_back([&, i]() {
                renderBundle.Outputs[i].Lists.BakeRespawners(renderBundle.Inputs[i].SimFrame,
                                                             renderBundle.Inputs[i].CameraRays,
                                                             renderBundle.Inputs[i].Frustum);
            });
            renderBundle.Tasks.push_back([&, i]() {
                renderBundle.Outputs[i].Lists.BakeSpaceships(renderBundle.Inputs[i].SimFrame,
                                                             renderBundle.Inputs[i].CameraRays,
                                                             renderBundle.Inputs[i].Frustum);
            });
            renderBundle.Tasks.push_back([&, i]() {
                renderBundle.Outputs[i].Lists.BakeExplosions(renderBundle.Inputs[i].SimFrame,
                                                             renderBundle.Inputs[i].CameraRays,
                                                             renderBundle.Inputs[i].Frustum);
            });
            renderBundle.Tasks.push_back([&, i]() {
                renderBundle.Outputs[i].Lists.BakeAsteroids(renderBundle.Inputs[i].SimFrame,
                                                            renderBundle.Inputs[i].CameraRays,
                                                            renderBundle.Inputs[i].Frustum);
            });
            renderBundle.Tasks.push_back([&, i]() {
                renderBundle.Outputs[i].Lists.BakeParticles(renderBundle.Inputs[i].SimFrame,
                                                            renderBundle.Inputs[i].CameraRays,
                                                            renderBundle.Inputs[i].Frustum);
            });
            renderBundle.Tasks.push_back([&, i]() {
                renderBundle.Outputs[i].Lists.BakeBullets(renderBundle.Inputs[i].SimFrame,
                                                          renderBundle.Inputs[i].CameraRays,
                                                          renderBundle.Inputs[i].Frustum);
            });
        }
    }

    mPassiveTaskBundles.push(1);
    mPassiveTaskBundles.push(0);
}

Render::~Render()
{
    for (size_t i = 0; i < mViewPorts.size(); ++i) {
        UnloadRenderTexture(mViewPortTextures[i]);
    }
    UnloadRenderTexture(mScreenTexture);
    UnloadModel(mAsteroidModel);
    UnloadShader(mFowShader);
}

bool Render::TryStartRenderTasks(const entt::registry& registry)
{
    ZoneScoped;
    uint32_t bundleIndex;
    {
        std::scoped_lock lock(mBundleMutex);
        if (mActiveTaskBundles.size() > 1) {
            ZoneScopedN("Flushing Accumulated Frames");
            bundleIndex = mActiveTaskBundles.front();
            mActiveTaskBundles.pop();
            mPassiveTaskBundles.push(bundleIndex);
        }
        if (mPassiveTaskBundles.empty()) {
            return false;
        }
        bundleIndex = mPassiveTaskBundles.top();
        mPassiveTaskBundles.pop();
    }

    auto& bundle = mRenderTaskBundles[bundleIndex];

    for (size_t i = 0; i < mViews; ++i) {
        auto& input = bundle.Inputs[i];
        input.SimFrame = &registry;
        input.Camera = mCameras[i];
        input.Viewport = mViewPorts[i];
        input.CameraRays = ComputeRays(mCameras[i], mViewPorts[i]);
        input.Frustum = ComputeFrustum(mCameras[i], input.CameraRays);
    }
    for (size_t i = 0; i < mViews; ++i) {
        bundle.Outputs[i].Camera = mCameras[i];
        bundle.Outputs[i].Lists.Clear();
    }

    mThreadPool.PushTasks(bundle.Tasks.begin(), bundle.Tasks.end());
    {
        std::scoped_lock lock(mBundleMutex);
        mActiveTaskBundles.push(bundleIndex);
    }

    for (size_t i = 0; i < mViews; ++i) {
        while (bundle.Outputs[i].Lists.BakeProgressFlags != RenderLists::AllProgressFlags) {
            mThreadPool.TryHelpOneTask();
        }
    }

    return true;
}

inline void WaitOnProgress(ThreadPool& threadPool, const RenderLists& lists, int32_t targetProgress)
{
    while ((lists.BakeProgressFlags & (1 << targetProgress)) == 0) {
        threadPool.TryHelpOneTask();
    }
}

bool Render::DrawScreenTexture()
{
    ZoneScoped;

    uint32_t bundleIndex;
    {
        std::scoped_lock lock(mBundleMutex);
        if (mActiveTaskBundles.empty()) {
            return false;
        }
        if (mActiveTaskBundles.size() > 1) {
            ZoneScopedN("Flushing Accumulated Frames");
            bundleIndex = mActiveTaskBundles.front();
            mActiveTaskBundles.pop();
            mPassiveTaskBundles.push(bundleIndex);
        }
        bundleIndex = mActiveTaskBundles.front();
        mActiveTaskBundles.pop();
    }

    auto& bundle = mRenderTaskBundles[bundleIndex];

    for (size_t i = 0; i < mViews; ++i) {
        const auto& viewPort = mViewPorts[i];
        const auto& output = bundle.Outputs[i];

        BeginTextureMode(mViewPortTextures[i]);
        ClearBackground(SpaceColor);

        BeginMode3D(output.Camera);

        WaitOnProgress(mThreadPool, bundle.Outputs[i].Lists, RenderLists::ProgressRespawners);
        DrawRespawns(bundle.Outputs[i].Lists);
        WaitOnProgress(mThreadPool, bundle.Outputs[i].Lists, RenderLists::ProgressSpaceships);
        DrawSpaceships(bundle.Outputs[i].Lists);
        WaitOnProgress(mThreadPool, bundle.Outputs[i].Lists, RenderLists::ProgressExplosions);
        DrawExplosions(bundle.Outputs[i].Lists);
        WaitOnProgress(mThreadPool, bundle.Outputs[i].Lists, RenderLists::ProgressAsteroids);
        DrawAsteroids(bundle.Outputs[i].Lists, mAsteroidModel, mFowShader, bundle.Outputs[i].Camera);
        WaitOnProgress(mThreadPool, bundle.Outputs[i].Lists, RenderLists::ProgressParticles);
        DrawParticles(bundle.Outputs[i].Lists);
        WaitOnProgress(mThreadPool, bundle.Outputs[i].Lists, RenderLists::ProgressBullets);
        DrawBullets(bundle.Outputs[i].Camera, mGlowTexture, bundle.Outputs[i].Lists);

        EndMode3D();

        EndTextureMode();
    }

    {
        std::scoped_lock lock(mBundleMutex);
        mPassiveTaskBundles.push(bundleIndex);
    }

    BeginTextureMode(mScreenTexture);
    ClearBackground(BLANK);
    for (size_t i = 0; i < mViews; ++i) {
        Rectangle target = {0, 0, mViewPorts[i].width, -mViewPorts[i].height};
        DrawTextureRec(mViewPortTextures[i].texture, target, {mViewPorts[i].x, mViewPorts[i].y}, WHITE);
    }
    if (mViews > 1) {
        DrawLine(mViewPorts[1].x, mViewPorts[1].y, mViewPorts[1].x, mViewPorts[1].height, WHITE);
    }
    EndTextureMode();
    return true;
}

const Texture& Render::ScreenTexture() const
{
    return mScreenTexture.texture;
}
