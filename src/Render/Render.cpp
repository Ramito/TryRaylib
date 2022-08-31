#include "Render.h"

#include "Components.h"
#include "Data.h"
#include <raymath.h>

constexpr Vector3 CameraOffset = { -10.f, 25.f, -10.f };

Render::Render(uint32_t viewID, RenderDependencies& dependencies) : mViewID(viewID)
, mRegistry(dependencies.GetDependency<entt::registry>())
, mMainCamera(dependencies.GetDependency<GameCameras>()[viewID])
{
}

void Render::Init() {
	mMainCamera.target = { 0.f, 0.f, 0.f };
	mMainCamera.position = Vector3Negate(CameraOffset);
	mMainCamera.projection = CAMERA_PERSPECTIVE;
	mMainCamera.up = { 0.f, 1.f, 0.f };
	mMainCamera.fovy = 70.f;
	SetCameraMode(mMainCamera, CAMERA_CUSTOM);

	mBackgroundTexture = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
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

	void DrawToCurrentTarget(const Camera& camera, const entt::registry& registry) {
		for (auto entity : registry.view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {
			auto& position = registry.get<PositionComponent>(entity);
			auto& orientation = registry.get<OrientationComponent>(entity);
			DrawSpaceShip(position.Position, orientation.Quaternion);
		}

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

		for (auto asteroid : registry.view<AsteroidComponent>()) {
			for (const Vector3& offset : SpaceOffsets) {
				const Vector3 position = Vector3Add(registry.get<PositionComponent>(asteroid).Position, offset);
				const float radius = registry.get<AsteroidComponent>(asteroid).Radius;

				float topSupport = Vector3DotProduct(topNormal, position) - radius;
				float leftSupport = Vector3DotProduct(leftNormal, position) - radius;
				float bottomSupport = Vector3DotProduct(bottomNormal, position) - radius;
				float rightSupport = Vector3DotProduct(rightNormal, position) - radius;

				if (topSupport <= topAnchor && leftSupport <= leftAnchor && bottomSupport <= bottomAnchor && rightSupport <= rightAnchor) {
					DrawSphereWires(position, radius, 8, 8, YELLOW);
					break;
				}
			}
		}

		for (entt::entity particle : registry.view<ParticleComponent>()) {
			for (const Vector3& offset : SpaceOffsets) {
				const Vector3 position = Vector3Add(registry.get<PositionComponent>(particle).Position, offset);

				float topSupport = Vector3DotProduct(topNormal, position);
				float leftSupport = Vector3DotProduct(leftNormal, position);
				float bottomSupport = Vector3DotProduct(bottomNormal, position);
				float rightSupport = Vector3DotProduct(rightNormal, position);

				if (topSupport <= topAnchor && leftSupport <= leftAnchor && bottomSupport <= bottomAnchor && rightSupport <= rightAnchor) {
					if (!registry.any_of<OrientationComponent>(particle))
					{
						DrawPoint3D(position, ORANGE);
					}
					else {
						DrawSphere(position, 0.2f, GREEN);
					}
					break;
				}
			}
		}
	}
}

void Render::Draw() {
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

	Rectangle backRect = { 0, 0, (float)mBackgroundTexture.texture.width, (float)-mBackgroundTexture.texture.height };

	BeginTextureMode(mBackgroundTexture);
	ClearBackground(BLANK);
	BeginMode3D(backgroundCamera);
	DrawToCurrentTarget(backgroundCamera, mRegistry);
	EndMode3D();
	EndTextureMode();

	BeginDrawing();
	ClearBackground(DARKGRAY);
	DrawTextureRec(mBackgroundTexture.texture, backRect, Vector2Zero(), DARKBROWN);
	BeginMode3D(mMainCamera);
	DrawGrid(500, 5.0f);
	DrawToCurrentTarget(mMainCamera, mRegistry);
	EndMode3D();

	DrawFPS(10, 10);
	EndDrawing();
}

