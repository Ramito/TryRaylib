#include "Render.h"

#include "Components.h"
#include "Data.h"
#include <raymath.h>
#include <rlgl.h>

constexpr Vector3 CameraOffset = { -10.f, 25.f, -10.f };

Render::Render(uint32_t viewID, RenderDependencies& dependencies) : mViewID(viewID)
, mRegistry(dependencies.GetDependency<entt::registry>())
, mMainCamera(dependencies.GetDependency<GameCameras>()[viewID])
{
	mMainCamera.target = { 0.f, 0.f, 0.f };
	mMainCamera.position = Vector3Negate(CameraOffset);
	mMainCamera.projection = CAMERA_PERSPECTIVE;
	mMainCamera.up = { 0.f, 1.f, 0.f };
	mMainCamera.fovy = 70.f;
	SetCameraMode(mMainCamera, CAMERA_CUSTOM);

	int width = GetScreenWidth();
	int height = GetScreenHeight();

	mBackgroundTexture = LoadRenderTexture(width, height);

	mBulletTexture = LoadRenderTexture(width, height);
}

Render::~Render() {
	UnloadRenderTexture(mBackgroundTexture);
	UnloadRenderTexture(mBulletTexture);
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
	constexpr std::array<Vector3, 9> SpaceOffsets = {
			Vector3{0.f,0.f,0.f},
			Vector3{0.f,0.f,SpaceData::LengthZ},
			Vector3{0.f,0.f,-SpaceData::LengthZ},
			Vector3{SpaceData::LengthX,0.f,0.f},
			Vector3{SpaceData::LengthX,0.f,SpaceData::LengthZ},
			Vector3{SpaceData::LengthX,0.f,-SpaceData::LengthZ},
			Vector3{-SpaceData::LengthX,0.f,0.f},
			Vector3{-SpaceData::LengthX,0.f,SpaceData::LengthZ},
			Vector3{-SpaceData::LengthX,0.f,-SpaceData::LengthZ}
	};

	struct CameraFrustum {
		float TopSupport;
		Vector3 TopNormal;
		float LeftSupport;
		Vector3 LeftNormal;
		float BottomSupport;
		Vector3 BottomNormal;
		float RightSupport;
		Vector3 RightNormal;
	};

	CameraFrustum ComputeFrustum(const Camera camera) {
		const float screenW = GetScreenWidth();
		const float screenH = GetScreenHeight();

		Ray minMinRay = GetMouseRay(Vector2{ 0.f, 0.f }, camera);
		Ray minMaxRay = GetMouseRay(Vector2{ 0.f, screenH }, camera);
		Ray maxMaxRay = GetMouseRay(Vector2{ screenW, screenH }, camera);
		Ray maxMinRay = GetMouseRay(Vector2{ screenW, 0.f }, camera);

		Vector3 anchor = camera.position;
		Vector3 topNormal = Vector3Normalize(Vector3CrossProduct(maxMinRay.direction, minMinRay.direction));
		Vector3 leftNormal = Vector3Normalize(Vector3CrossProduct(minMinRay.direction, minMaxRay.direction));
		Vector3 bottomNormal = Vector3Normalize(Vector3CrossProduct(minMaxRay.direction, maxMaxRay.direction));
		Vector3 rightNormal = Vector3Normalize(Vector3CrossProduct(maxMaxRay.direction, maxMinRay.direction));

		float topAnchor = Vector3DotProduct(topNormal, anchor);
		float leftAnchor = Vector3DotProduct(leftNormal, anchor);
		float bottomAnchor = Vector3DotProduct(bottomNormal, anchor);
		float rightAnchor = Vector3DotProduct(rightNormal, anchor);

		return { topAnchor, topNormal, leftAnchor, leftNormal, bottomAnchor, bottomNormal, rightAnchor, rightNormal };
	}

	inline bool PositionRadiusInsideFrustum(const CameraFrustum& frustum, const Vector3& position, float radius) {
		float topSupport = Vector3DotProduct(frustum.TopNormal, position) - radius;
		float leftSupport = Vector3DotProduct(frustum.LeftNormal, position) - radius;
		float bottomSupport = Vector3DotProduct(frustum.BottomNormal, position) - radius;
		float rightSupport = Vector3DotProduct(frustum.RightNormal, position) - radius;

		return topSupport <= frustum.TopSupport
			&& leftSupport <= frustum.LeftSupport
			&& bottomSupport <= frustum.BottomSupport
			&& rightSupport <= frustum.RightSupport;
	}

	void DrawToCurrentTarget(const Camera& camera, const entt::registry& registry) {
		const CameraFrustum frustum = ComputeFrustum(camera);

		for (auto entity : registry.view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {
			auto& position = registry.get<PositionComponent>(entity);
			auto& orientation = registry.get<OrientationComponent>(entity);
			if (PositionRadiusInsideFrustum(frustum, position.Position, 0.f)) {
				// TODO: Spaceship bound radius!
				DrawSpaceShip(position.Position, orientation.Quaternion);
			}
		}

		for (auto asteroid : registry.view<AsteroidComponent>()) {
			for (const Vector3& offset : SpaceOffsets) {
				const Vector3 position = Vector3Add(registry.get<PositionComponent>(asteroid).Position, offset);
				const float radius = registry.get<AsteroidComponent>(asteroid).Radius;

				if (PositionRadiusInsideFrustum(frustum, position, radius)) {
					DrawSphereWires(position, radius, 8, 8, YELLOW);
					break;
				}
			}
		}

		for (entt::entity particle : registry.view<ParticleComponent>(entt::exclude<BulletComponent>)) {
			for (const Vector3& offset : SpaceOffsets) {
				const Vector3 position = Vector3Add(registry.get<PositionComponent>(particle).Position, offset);
				if (PositionRadiusInsideFrustum(frustum, position, 0.f)) {
					DrawPoint3D(position, ORANGE);
				}
			}
		}
	}

	void DrawBulletsToCurrentTarget(const Camera& camera, const entt::registry& registry, float gameTime) {
		const CameraFrustum frustum = ComputeFrustum(camera);

		BeginBlendMode(BLEND_ALPHA);
		rlDisableDepthMask();

		for (entt::entity particle : registry.view<BulletComponent>()) {
			for (const Vector3& offset : SpaceOffsets) {
				const Vector3 position = Vector3Add(registry.get<PositionComponent>(particle).Position, offset);
				if (PositionRadiusInsideFrustum(frustum, position, 0.f)) {
					// Ghetto bloom
					DrawSphere(position, 0.05f, WHITE);
					DrawSphere(position, 0.12f, GREEN);
					DrawSphere(position, 0.175f, { 0, 228, 48, 180 });
					DrawSphere(position, 1.f, { 0, 228, 48, 80 });
				}
			}
		}

		for (auto explosion : registry.view<ExplosionComponent>()) {
			const Vector3& position = registry.get<PositionComponent>(explosion).Position;
			const ExplosionComponent& explosionComponent = registry.get<ExplosionComponent>(explosion);
			const float relativeTime = std::clamp((gameTime - explosionComponent.StartTime) / ExplosionData::Time, 0.f, 1.f);
			const float radiusModule = std::cbrt(relativeTime);
			const float radius = radiusModule * explosionComponent.Radius;
			Color color = { 255, 255, 255, (1.f - relativeTime) * 255 };
			for (const Vector3& offset : SpaceOffsets) {
				if (PositionRadiusInsideFrustum(frustum, position, radius)) {
					DrawSphere(position, explosionComponent.Radius * radiusModule, color);
					break;
				}
			}
		}

		rlEnableDepthMask();
	}
}

void Render::Draw(float gameTime) {
	for (auto playerEntity : mRegistry.view<PositionComponent, SpaceshipInputComponent>()) {
		auto& input = mRegistry.get<SpaceshipInputComponent>(playerEntity);
		if (input.InputId == mViewID) {
			auto& position = mRegistry.get<PositionComponent>(playerEntity);
			mMainCamera.target = position.Position;
			mMainCamera.position = Vector3Add(mMainCamera.target, CameraOffset);
			UpdateCamera(&mMainCamera);
			break;
		}
	}

	Camera backgroundCamera = mMainCamera;
	float targetX = mMainCamera.target.x + SpaceData::LengthX * 0.5f;
	float targetZ = mMainCamera.target.z + SpaceData::LengthZ * 0.5f;
	if (targetX >= SpaceData::LengthX) {
		targetX -= SpaceData::LengthX;
	}
	if (targetZ >= SpaceData::LengthZ) {
		targetZ -= SpaceData::LengthZ;
	}
	backgroundCamera.target = { targetX, 0.f, targetZ };
	backgroundCamera.position = Vector3Add(backgroundCamera.target, Vector3Scale(CameraOffset, 2.25f));

	int width = GetScreenWidth();
	int height = GetScreenHeight();
	Rectangle targetRect = { 0, 0, (float)width, (float)-height };

	Rectangle sourceRect = { 0, 0, (float)width, (float)height };	// TODO: Name makes no sense?

	BeginTextureMode(mBulletTexture);
	ClearBackground(BLANK);
	BeginMode3D(backgroundCamera);
	DrawBulletsToCurrentTarget(backgroundCamera, mRegistry, gameTime);
	EndBlendMode();
	EndMode3D();
	EndTextureMode();

	BeginTextureMode(mBackgroundTexture);
	ClearBackground(BLANK);
	BeginMode3D(backgroundCamera);
	DrawToCurrentTarget(backgroundCamera, mRegistry);
	EndMode3D();
	EndTextureMode();

	BeginTextureMode(mBulletTexture);
	BeginMode3D(mMainCamera);
	DrawBulletsToCurrentTarget(mMainCamera, mRegistry, gameTime);
	EndBlendMode();
	EndMode3D();
	EndTextureMode();

	BeginDrawing();
	ClearBackground(DARKGRAY);
	DrawTextureRec(mBackgroundTexture.texture, targetRect, Vector2Zero(), DARKBROWN);
	BeginMode3D(mMainCamera);
	DrawGrid(500, 5.0f);
	DrawToCurrentTarget(mMainCamera, mRegistry);
	EndMode3D();
	BeginBlendMode(BLEND_ADDITIVE);
	DrawTextureRec(mBulletTexture.texture, targetRect, Vector2Zero(), WHITE);
	EndBlendMode();

	DrawFPS(10, 10);
	EndDrawing();
}

