#include "Render.h"

#include "Components.h"
#include "Data.h"
#include "SpaceUtil.h"
#include <raymath.h>
#include <rlgl.h>
#include <optional>

constexpr Vector3 TargetOffset = { -3.5f, 0.f, -3.5f };
constexpr Vector3 CameraOffset = { -11.f, 42.f, -11.f };

Render::Render(uint32_t views, RenderDependencies& dependencies) : mViews(views)
, mRegistry(dependencies.GetDependency<entt::registry>())
, mCameras(dependencies.GetDependency<GameCameras>())
, mViewPorts(dependencies.GetDependency<ViewPorts>())
{
	for (Camera& camera : mCameras) {
		camera.target = TargetOffset;
		camera.position = CameraOffset;
		camera.projection = CAMERA_PERSPECTIVE;
		camera.up = { 0.f, 1.f, 0.f };
		camera.fovy = 60.f;
		SetCameraMode(camera, CAMERA_CUSTOM);
	}

	for (size_t i = 0; i < mViewPorts.size(); ++i) {
		int width = mViewPorts[i].width;
		int height = mViewPorts[i].height;
		mBackgroundTextures[i] = LoadRenderTexture(width, height);
		mBulletTextures[i] = LoadRenderTexture(width, height);
		mViewPortTextures[i] = LoadRenderTexture(width, height);
	}

	mScreenTexture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
}

Render::~Render() {
	for (size_t i = 0; i < mViewPorts.size(); ++i) {
		UnloadRenderTexture(mBackgroundTextures[i]);
		UnloadRenderTexture(mBulletTextures[i]);
		UnloadRenderTexture(mViewPortTextures[i]);
	}
	UnloadRenderTexture(mScreenTexture);
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

namespace {
	struct CameraFrustum {
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

	CameraFrustum ComputeFrustum(const Camera& camera, const Rectangle& viewPort) {

		int screenWidth = GetScreenWidth();
		int screenHeight = GetScreenHeight();

		float minX = (screenWidth - viewPort.width) / 2;
		float minY = (screenHeight - viewPort.height) / 2;
		float maxX = minX + viewPort.width;
		float maxY = minY + viewPort.height;

		Ray minMinRay = GetMouseRay(Vector2{ minX, minY }, camera);
		Ray minMaxRay = GetMouseRay(Vector2{ minX, maxY }, camera);
		Ray maxMaxRay = GetMouseRay(Vector2{ maxX, maxY }, camera);
		Ray maxMinRay = GetMouseRay(Vector2{ maxX, minY }, camera);

		Vector3 anchor = camera.position;
		Vector3 topNormal = Vector3Normalize(Vector3CrossProduct(maxMinRay.direction, minMinRay.direction));
		Vector3 leftNormal = Vector3Normalize(Vector3CrossProduct(minMinRay.direction, minMaxRay.direction));
		Vector3 bottomNormal = Vector3Normalize(Vector3CrossProduct(minMaxRay.direction, maxMaxRay.direction));
		Vector3 rightNormal = Vector3Normalize(Vector3CrossProduct(maxMaxRay.direction, maxMinRay.direction));

		float topAnchor = Vector3DotProduct(topNormal, anchor);
		float leftAnchor = Vector3DotProduct(leftNormal, anchor);
		float bottomAnchor = Vector3DotProduct(bottomNormal, anchor);
		float rightAnchor = Vector3DotProduct(rightNormal, anchor);

		return { camera.target, topAnchor, topNormal, leftAnchor, leftNormal, bottomAnchor, bottomNormal, rightAnchor, rightNormal };
	}

	inline std::optional<Vector3> FindFrustumVisiblePosition(const CameraFrustum& frustum, const Vector3& position, float radius) {
		const Vector3 renderPosition = Vector3Add(SpaceUtil::FindVectorGap(frustum.Target, position), frustum.Target);

		float topSupport = Vector3DotProduct(frustum.TopNormal, renderPosition) - radius;
		float leftSupport = Vector3DotProduct(frustum.LeftNormal, renderPosition) - radius;
		float bottomSupport = Vector3DotProduct(frustum.BottomNormal, renderPosition) - radius;
		float rightSupport = Vector3DotProduct(frustum.RightNormal, renderPosition) - radius;

		if (topSupport <= frustum.TopSupport
			&& leftSupport <= frustum.LeftSupport
			&& bottomSupport <= frustum.BottomSupport
			&& rightSupport <= frustum.RightSupport) {
			return renderPosition;
		}
		return {};
	}

	void DrawToCurrentTarget(const Camera& camera, const Rectangle& viewPort, const entt::registry& registry) {
		const CameraFrustum frustum = ComputeFrustum(camera, viewPort);

		for (auto entity : registry.view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {
			auto& position = registry.get<PositionComponent>(entity);
			auto& orientation = registry.get<OrientationComponent>(entity);
			if (auto renderPosition = FindFrustumVisiblePosition(frustum, position.Position, SpaceshipData::CollisionRadius)) {
				DrawSpaceShip(renderPosition.value(), orientation.Quaternion);
			}
		}

		for (auto asteroid : registry.view<AsteroidComponent>()) {
			const float radius = registry.get<AsteroidComponent>(asteroid).Radius;
			const Vector3 position = registry.get<PositionComponent>(asteroid).Position;
			if (auto renderPosition = FindFrustumVisiblePosition(frustum, position, radius)) {
				DrawSphereWires(renderPosition.value(), radius, 8, 8, RAYWHITE);
			}
		}

		for (entt::entity particle : registry.view<ParticleComponent>(entt::exclude<BulletComponent>)) {
			const Vector3 position = registry.get<PositionComponent>(particle).Position;
			if (auto renderPosition = FindFrustumVisiblePosition(frustum, position, 0.f)) {
				DrawPoint3D(renderPosition.value(), ORANGE);
			}
		}
	}

	void DrawBulletsToCurrentTarget(const Camera& camera, const Rectangle& viewPort, const entt::registry& registry, float gameTime) {
		const CameraFrustum frustum = ComputeFrustum(camera, viewPort);

		BeginBlendMode(BLEND_ALPHA);
		rlDisableDepthMask();

		for (entt::entity particle : registry.view<BulletComponent>()) {
			const Vector3 position = registry.get<PositionComponent>(particle).Position;
			if (auto renderPosition = FindFrustumVisiblePosition(frustum, position, 0.f)) {
				// Ghetto bloom
				DrawSphere(renderPosition.value(), 0.05f, WHITE);
				DrawSphere(renderPosition.value(), 0.12f, GREEN);
				DrawSphere(renderPosition.value(), 0.175f, { 0, 228, 48, 180 });
				DrawSphere(renderPosition.value(), 1.f, { 0, 228, 48, 80 });
			}
		}

		for (auto explosion : registry.view<ExplosionComponent>()) {
			const Vector3& position = registry.get<PositionComponent>(explosion).Position;
			const ExplosionComponent& explosionComponent = registry.get<ExplosionComponent>(explosion);
			const float relativeTime = std::clamp((gameTime - explosionComponent.StartTime) / ExplosionData::Time, 0.f, 1.f);
			const float radiusModule = std::cbrt(relativeTime);
			const float radius = radiusModule * explosionComponent.Radius;
			Color color = { 255, 255, 255, (1.f - relativeTime) * 255 };
			if (auto renderPosition = FindFrustumVisiblePosition(frustum, position, radius)) {
				DrawSphere(renderPosition.value(), explosionComponent.Radius * radiusModule, color);
			}
		}

		rlEnableDepthMask();
	}

	void RenderView(const Camera& camera, const Rectangle& viewPort, RenderTexture& viewTexture, RenderTexture& backgroundTexture, RenderTexture& bulletTexture, entt::registry& registry, float gameTime) {
		Camera backgroundCamera = camera;
		float targetX = camera.target.x + SpaceData::LengthX * 0.5f;
		float targetZ = camera.target.z + SpaceData::LengthZ * 0.5f;
		if (targetX >= SpaceData::LengthX) {
			targetX -= SpaceData::LengthX;
		}
		if (targetZ >= SpaceData::LengthZ) {
			targetZ -= SpaceData::LengthZ;
		}
		backgroundCamera.target = { targetX, 0.f, targetZ };
		backgroundCamera.position = Vector3Add(backgroundCamera.target, Vector3Scale(CameraOffset, 1.75f));

		BeginTextureMode(bulletTexture);
		ClearBackground(BLANK);
		BeginMode3D(backgroundCamera);
		DrawBulletsToCurrentTarget(backgroundCamera, viewPort, registry, gameTime);
		EndBlendMode();
		EndMode3D();
		EndTextureMode();

		BeginTextureMode(backgroundTexture);
		ClearBackground(BLANK);
		BeginMode3D(backgroundCamera);
		DrawToCurrentTarget(backgroundCamera, viewPort, registry);
		EndMode3D();
		EndTextureMode();

		BeginTextureMode(bulletTexture);
		BeginMode3D(camera);
		DrawBulletsToCurrentTarget(camera, viewPort, registry, gameTime);
		EndBlendMode();
		EndMode3D();
		EndTextureMode();

		Rectangle target = { 0, 0, viewPort.width, -viewPort.height };

		BeginTextureMode(viewTexture);
		ClearBackground({ 0, 41, 96, 255 });
		DrawTextureRec(backgroundTexture.texture, target, Vector2Zero(), GRAY);
		BeginMode3D(camera);
		DrawToCurrentTarget(camera, viewPort, registry);
		EndMode3D();
		BeginBlendMode(BLEND_ADDITIVE);
		DrawTextureRec(bulletTexture.texture, target, Vector2Zero(), WHITE);
		EndBlendMode();
		EndTextureMode();
	}
}

void Render::DrawScreenTexture(float gameTime) {
	for (auto playerEntity : mRegistry.view<PositionComponent, SpaceshipInputComponent>()) {
		auto& input = mRegistry.get<SpaceshipInputComponent>(playerEntity);
		auto& position = mRegistry.get<PositionComponent>(playerEntity);
		const Vector3 target = Vector3Add(position.Position, TargetOffset);
		mCameras[input.InputId].target = target;
		mCameras[input.InputId].position = Vector3Add(target, CameraOffset);
	}

	for (size_t i = 0; i < mViews; ++i) {
		RenderView(mCameras[i], mViewPorts[i], mViewPortTextures[i], mBackgroundTextures[i], mBulletTextures[i], mRegistry, gameTime);
	}

	BeginTextureMode(mScreenTexture);
	ClearBackground(BLANK);
	for (size_t i = 0; i < mViews; ++i) {
		Rectangle target = { 0, 0, mViewPorts[i].width, -mViewPorts[i].height };
		DrawTextureRec(mViewPortTextures[i].texture, target, { mViewPorts[i].x, mViewPorts[i].y }, WHITE);
	}
	if (mViews > 1) {
		DrawLine(mViewPorts[1].x, mViewPorts[1].y, mViewPorts[1].x, mViewPorts[1].height, WHITE);
	}
	DrawFPS(10, 10);
	EndTextureMode();
}

const Texture& Render::ScreenTexture() const {
	return mScreenTexture.texture;
}

