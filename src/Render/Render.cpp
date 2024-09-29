#include "Render.h"

#include "Components.h"
#include "Data.h"
#include "SpaceUtil.h"
#include "Tracy.hpp"
#include <optional>
#include <raymath.h>
#include <rlgl.h>

namespace {

CameraFrustum ComputeFrustum(const Camera& camera, const Rectangle& viewPort)
{

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    float minX = (screenWidth - viewPort.width) / 2;
    float minY = (screenHeight - viewPort.height) / 2;
    float maxX = minX + viewPort.width;
    float maxY = minY + viewPort.height;

    Ray minMinRay = GetMouseRay(Vector2{minX, minY}, camera);
    Ray minMaxRay = GetMouseRay(Vector2{minX, maxY}, camera);
    Ray maxMaxRay = GetMouseRay(Vector2{maxX, maxY}, camera);
    Ray maxMinRay = GetMouseRay(Vector2{maxX, minY}, camera);

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

inline std::optional<Vector3>
FindFrustumVisiblePosition(const CameraFrustum& frustum, const Vector3& position, float radius)
{
    const Vector3 renderPosition =
    Vector3Add(SpaceUtil::FindVectorGap(frustum.Target, position), frustum.Target);

    float topSupport = Vector3DotProduct(frustum.TopNormal, renderPosition) - radius;
    float leftSupport = Vector3DotProduct(frustum.LeftNormal, renderPosition) - radius;
    float bottomSupport = Vector3DotProduct(frustum.BottomNormal, renderPosition) - radius;
    float rightSupport = Vector3DotProduct(frustum.RightNormal, renderPosition) - radius;

    if (topSupport <= frustum.TopSupport && leftSupport <= frustum.LeftSupport &&
        bottomSupport <= frustum.BottomSupport && rightSupport <= frustum.RightSupport) {
        return renderPosition;
    }
    return {};
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

void DrawToCurrentTarget(const RenderList& list)
{
    constexpr std::array<Color, 2> PlayerColors = {RED, BLUE};

    for (const auto& [position, orientation, inputID] : list.Spaceships) {
        DrawSpaceShip(position, orientation, PlayerColors[inputID]);
    }

    for (const auto& [position, inputID] : list.Respawners) {
        DrawCircle3D(position,
                     RespawnData::MarkerRadius * 0.5f * (1.f + sin(GetTime() * RespawnData::MarkerFrequency)),
                     Left3, 90.f, PlayerColors[inputID]);
    }

    for (const auto& [position, radius] : list.Asteroids) {
        DrawSphereEx(position, radius, 5, 6, SpaceColor);
        DrawSphereWires(position, radius, 5, 6, YELLOW);
    }

    for (const auto& [position, color] : list.Particles) {
        DrawPoint3D(position, color);
    }
}

void DrawBulletsToCurrentTarget(const Camera& camera, const Texture& glow, const RenderList& list)
{
    rlDisableDepthMask();

    for (const auto& [position, color] : list.Bullets) {
        Vector3 up = Vector3Normalize({1.f, 0.f, 1.f});
        Rectangle source = {0.0f, 0.0f, (float)glow.width, (float)glow.height};
        Vector2 size = {2.25f, 2.25f};
        DrawBillboardPro(camera, glow, source, position, up, size, {}, 0.f, color);
    }

    for (const auto& [position, radius, relativeRadius] : list.Explosions) {
        const unsigned char alpha = static_cast<unsigned char>(rintf(cbrt(1.f - relativeRadius) * 255));
        Color color = {255, 255, 255, alpha};
        DrawSphere(position, radius, color);
    }

    rlEnableDepthMask();
}

Camera MakeBackgroundCamera(const Camera& camera)
{
    Camera backgroundCamera = camera;
    float targetX = camera.target.x + SpaceData::LengthX * 0.5f;
    float targetZ = camera.target.z + SpaceData::LengthZ * 0.5f;
    if (targetX >= SpaceData::LengthX) {
        targetX -= SpaceData::LengthX;
    }
    if (targetZ >= SpaceData::LengthZ) {
        targetZ -= SpaceData::LengthZ;
    }
    backgroundCamera.target = {targetX, 0.f, targetZ};
    backgroundCamera.position =
    Vector3Add(backgroundCamera.target, Vector3Scale(CameraData::CameraOffset, 1.75f));
    return backgroundCamera;
}

void RenderBackground(RenderTexture& backgroundTexture,
                      RenderTexture& bulletTexture,
                      const RenderPayload& payload,
                      const Texture& glow)
{
    ZoneScoped;
    BeginTextureMode(bulletTexture);
    ClearBackground(BLANK);
    BeginMode3D(payload.BackgroundCamera);
    DrawBulletsToCurrentTarget(payload.MainCamera, glow, payload.BackgroundList);
    EndBlendMode();
    EndMode3D();
    EndTextureMode();

    BeginTextureMode(backgroundTexture);
    ClearBackground(BLANK);
    BeginMode3D(payload.BackgroundCamera);
    DrawToCurrentTarget(payload.BackgroundList);
    EndMode3D();
    EndTextureMode();
}

void RenderView(const Rectangle& viewPort,
                RenderTexture& viewTexture,
                RenderTexture& backgroundTexture,
                RenderTexture& bulletTexture,
                const RenderPayload& payload,
                const Texture& glow)
{
    ZoneScoped;
    BeginTextureMode(bulletTexture);
    BeginMode3D(payload.MainCamera);
    BeginBlendMode(BLEND_ALPHA);
    DrawBulletsToCurrentTarget(payload.MainCamera, glow, payload.MainList);
    EndBlendMode();
    EndMode3D();
    EndTextureMode();

    Rectangle target = {0, 0, viewPort.width, -viewPort.height};

    BeginTextureMode(viewTexture);
    ClearBackground(SpaceColor);
    DrawTextureRec(backgroundTexture.texture, target, Vector2Zero(), GRAY);
    BeginMode3D(payload.MainCamera);
    DrawToCurrentTarget(payload.MainList);
    EndMode3D();
    BeginBlendMode(BLEND_ADDITIVE);
    DrawTextureRec(bulletTexture.texture, target, Vector2Zero(), WHITE);
    EndBlendMode();
    EndTextureMode();
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
        mBackgroundTextures[i] = LoadRenderTexture(width, height);
        mBulletTextures[i] = LoadRenderTexture(width, height);
        mViewPortTextures[i] = LoadRenderTexture(width, height);
    }

    mScreenTexture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    Image glowImage =
    GenImageGradientRadial(GetScreenWidth() / 16, GetScreenWidth() / 16, 0.05f, WHITE, BLANK);
    mGlowTexture = LoadTextureFromImage(glowImage);
}

Render::~Render()
{
    for (size_t i = 0; i < mViewPorts.size(); ++i) {
        UnloadRenderTexture(mBackgroundTextures[i]);
        UnloadRenderTexture(mBulletTextures[i]);
        UnloadRenderTexture(mViewPortTextures[i]);
    }
    UnloadRenderTexture(mScreenTexture);
}

void Render::DrawScreenTexture(const entt::registry& registry)
{
    ZoneScoped;
    for (size_t i = 0; i < mViews; ++i) {
        RenderTaskSource& source = mRenderTaskSources[i];
        source.SimFrame = &registry;
        source.MainCamera = mCameras[i];
        source.Viewport = mViewPorts[i];
    }

    for (size_t i = 0; i < 2 * mViews; ++i) {
        RenderTaskInput& input = mRenderTaskInputs[i];
        const Camera& mainCamera = mCameras[i / 2];
        if (i % 2 == 0) {
            input.TargetCamera = mainCamera;
        } else {
            input.TargetCamera = MakeBackgroundCamera(mainCamera);
        }
        input.Frustum = ComputeFrustum(input.TargetCamera, mRenderTaskSources[i / 2].Viewport);
    }

    auto clearRenderList = [](RenderList& list) {
        ZoneScopedN("ClearLists");
        list.Spaceships.clear();
        list.Respawners.clear();
        list.Asteroids.clear();
        list.Particles.clear();
        list.Bullets.clear();
        list.Explosions.clear();
        list.BakeProgress = 0;
    };

    for (size_t i = 0; i < mViews; ++i) {
        RenderPayload& payload = mRenderPayloads[i];
        payload.MainCamera = mRenderTaskInputs[2 * i].TargetCamera;
        clearRenderList(payload.MainList);
        payload.BackgroundCamera = mRenderTaskInputs[2 * i + 1].TargetCamera;
        clearRenderList(payload.BackgroundList);
    }

    if (mRenderTasks.empty()) {
        auto makeMainTask = [this](size_t i, auto&& baker) {
            return [&, i]() {
                baker(mRenderTaskSources[i], mRenderTaskInputs[2 * i], mRenderPayloads[i].MainList);
            };
        };

        auto makeBackgroundTask = [this](size_t i, auto&& baker) {
            return [&, i]() {
                baker(mRenderTaskSources[i], mRenderTaskInputs[2 * i + 1], mRenderPayloads[i].BackgroundList);
            };
        };

        for (size_t i = 0; i < mViews; ++i) {
            mRenderTasks.push_back(makeBackgroundTask(i, BakeParticlesRenderList));
            mRenderTasks.push_back(makeBackgroundTask(i, BakeSpaceshipsRenderList));
            mRenderTasks.push_back(makeBackgroundTask(i, BakeAsteroidsRenderList));
            mRenderTasks.push_back(makeBackgroundTask(i, BakeBulletsRenderList));
            mRenderTasks.push_back(makeBackgroundTask(i, BakeExplosionsRenderList));
        }

        for (size_t i = 0; i < mViews; ++i) {
            mRenderTasks.push_back(makeMainTask(i, BakeParticlesRenderList));
            mRenderTasks.push_back(makeMainTask(i, BakeSpaceshipsRenderList));
            mRenderTasks.push_back(makeMainTask(i, BakeAsteroidsRenderList));
            mRenderTasks.push_back(makeMainTask(i, BakeBulletsRenderList));
            mRenderTasks.push_back(makeMainTask(i, BakeExplosionsRenderList));
        }
    }

    mThreadPool.PushTasks(mRenderTasks.begin(), mRenderTasks.end());

    for (size_t i = 0; i < mViews; ++i) {
        while (mRenderPayloads[i].BackgroundList.BakeProgress < RenderList::MaxBakeProgress) {
            mThreadPool.TryHelpOneTask();
        }
        RenderBackground(mBackgroundTextures[i], mBulletTextures[i], mRenderPayloads[i], mGlowTexture);
    }

    for (size_t i = 0; i < mViews; ++i) {
        while (mRenderPayloads[i].MainList.BakeProgress < RenderList::MaxBakeProgress) {
            mThreadPool.TryHelpOneTask();
        }
        RenderView(mViewPorts[i], mViewPortTextures[i], mBackgroundTextures[i], mBulletTextures[i],
                   mRenderPayloads[i], mGlowTexture);
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
    DrawFPS(10, 10);
    EndTextureMode();
}

const Texture& Render::ScreenTexture() const
{
    return mScreenTexture.texture;
}

void Render::BakeSpaceshipsRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList)
{
    ZoneScoped;
    targetList.Spaceships.clear();
    for (auto entity :
         source.SimFrame->view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {
        const auto& position = source.SimFrame->get<PositionComponent>(entity).Position;
        if (auto renderPosition =
            FindFrustumVisiblePosition(input.Frustum, position, SpaceshipData::CollisionRadius)) {
            const auto& orientation = source.SimFrame->get<OrientationComponent>(entity).Rotation;
            const uint32_t inputID = source.SimFrame->get<SpaceshipInputComponent>(entity).InputId;
            targetList.Spaceships.emplace_back(renderPosition.value(), orientation, inputID);
        }
    }

    for (auto respawner : source.SimFrame->view<RespawnComponent, PositionComponent>()) {
        const auto& respawnComponent = source.SimFrame->get<RespawnComponent>(respawner);
        if (respawnComponent.TimeLeft > 0.f) {
            continue;
        }
        const auto& position = source.SimFrame->get<PositionComponent>(respawner).Position;
        if (auto renderPosition =
            FindFrustumVisiblePosition(input.Frustum, position, SpaceshipData::CollisionRadius)) {
            targetList.Respawners.emplace_back(renderPosition.value(), respawnComponent.InputId);
        }
    }

    targetList.BakeProgress += 1;
}

void Render::BakeAsteroidsRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList)
{
    ZoneScoped;
    targetList.Asteroids.clear();
    for (auto asteroid : source.SimFrame->view<AsteroidComponent>()) {
        const float radius = source.SimFrame->get<AsteroidComponent>(asteroid).Radius;
        const Vector3 position = source.SimFrame->get<PositionComponent>(asteroid).Position;
        if (auto renderPosition = FindFrustumVisiblePosition(input.Frustum, position, radius)) {
            targetList.Asteroids.emplace_back(renderPosition.value(), radius);
        }
    }
    targetList.BakeProgress += 1;
}

void Render::BakeParticlesRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList)
{
    ZoneScoped;
    targetList.Particles.clear();
    for (entt::entity particle : source.SimFrame->view<ParticleComponent>(entt::exclude<BulletComponent>)) {
        const Vector3 position = source.SimFrame->get<PositionComponent>(particle).Position;
        if (auto renderPosition = FindFrustumVisiblePosition(input.Frustum, position, 0.f)) {
            const Color color = source.SimFrame->get<ParticleComponent>(particle).Color;
            targetList.Particles.emplace_back(renderPosition.value(), color);
        }
    }
    targetList.BakeProgress += 1;
}

void Render::BakeBulletsRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList)
{
    ZoneScoped;
    targetList.Bullets.clear();
    for (entt::entity particle : source.SimFrame->view<BulletComponent>()) {
        const Vector3 position = source.SimFrame->get<PositionComponent>(particle).Position;
        if (auto renderPosition = FindFrustumVisiblePosition(input.Frustum, position, 0.f)) {
            const Color color = source.SimFrame->get<ParticleComponent>(particle).Color;
            targetList.Bullets.emplace_back(renderPosition.value(), color);
        }
    }
    targetList.BakeProgress += 1;
}

void Render::BakeExplosionsRenderList(const RenderTaskSource& source, const RenderTaskInput& input, RenderList& targetList)
{
    ZoneScoped;
    targetList.Explosions.clear();
    for (auto explosion : source.SimFrame->view<ExplosionComponent>()) {
        const Vector3& position = source.SimFrame->get<PositionComponent>(explosion).Position;
        const ExplosionComponent& explosionComponent = source.SimFrame->get<ExplosionComponent>(explosion);
        const float radius = explosionComponent.CurrentRadius;
        const float relativeRadius = radius / explosionComponent.TerminalRadius;
        if (auto renderPosition = FindFrustumVisiblePosition(input.Frustum, position, radius)) {
            targetList.Explosions.emplace_back(renderPosition.value(), radius,
                                               std::clamp(relativeRadius, 0.f, 1.f));
        }
    }
    targetList.BakeProgress += 1;
}
