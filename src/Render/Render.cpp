#include "Render.h"

#include "Components.h"
#include "Data.h"
#include <raymath.h>

constexpr Vector3 CameraOffset = { -10.f, 25.f, -10.f };

Render::Render(uint32_t viewID, RenderDependencies& dependencies) : ViewID(viewID)
, mRegistry(dependencies.GetDependency<entt::registry>())
, mMainCamera(dependencies.GetDependency<GameCameras>()[viewID])
{
}

void Render::Init() {
	mMainCamera.position = Vector3Negate(CameraOffset);
	mMainCamera.target = { 0.f, 0.f, 0.f };
	mMainCamera.projection = CAMERA_PERSPECTIVE;
	mMainCamera.up = { 0.f, 1.f, 0.f };
	mMainCamera.fovy = 70.f;
	SetCameraMode(mMainCamera, CAMERA_CUSTOM);
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

void Render::Draw() {

	for (auto playerEntity : mRegistry.view<PositionComponent>()) {
		auto& position = mRegistry.get<PositionComponent>(playerEntity);
		mMainCamera.target = position.Position;
		mMainCamera.position = Vector3Add(mMainCamera.target, CameraOffset);
	}
	UpdateCamera(&mMainCamera);
	BeginDrawing();
	ClearBackground(DARKGRAY);
	BeginMode3D(mMainCamera);
	DrawGrid(500, 5.0f);

	for (auto entity : mRegistry.view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {
		auto& position = mRegistry.get<PositionComponent>(entity);
		auto& orientation = mRegistry.get<OrientationComponent>(entity);
		DrawSpaceShip(position.Position, orientation.Quaternion);
	}

	const float screenW = GetScreenWidth();
	const float screenH = GetScreenHeight();

	Ray minMinRay = GetMouseRay(Vector2{ 0.f, 0.f }, mMainCamera);
	Ray minMaxRay = GetMouseRay(Vector2{ 0.f, screenH }, mMainCamera);
	Ray maxMaxRay = GetMouseRay(Vector2{ screenW, screenH }, mMainCamera);
	Ray maxMinRay = GetMouseRay(Vector2{ screenW, 0.f }, mMainCamera);

	Vector3 anchor = mMainCamera.position;
	Vector3 topNormal = Vector3Normalize(Vector3CrossProduct(maxMinRay.direction, minMinRay.direction));
	Vector3 leftNormal = Vector3Normalize(Vector3CrossProduct(minMinRay.direction, minMaxRay.direction));
	Vector3 bottomNormal = Vector3Normalize(Vector3CrossProduct(minMaxRay.direction, maxMaxRay.direction));
	Vector3 rightNormal = Vector3Normalize(Vector3CrossProduct(maxMaxRay.direction, maxMinRay.direction));

	float topAnchor = Vector3DotProduct(topNormal, anchor);
	float leftAnchor = Vector3DotProduct(leftNormal, anchor);
	float bottomAnchor = Vector3DotProduct(bottomNormal, anchor);
	float rightAnchor = Vector3DotProduct(rightNormal, anchor);

	static constexpr std::array<Vector3, 9> offsets = {
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

	for (auto asteroid : mRegistry.view<AsteroidComponent>()) {
		for (const Vector3& offset : offsets) {
			const Vector3 position = Vector3Add(mRegistry.get<PositionComponent>(asteroid).Position, offset);
			const float radius = mRegistry.get<AsteroidComponent>(asteroid).Radius;

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

	for (entt::entity particle : mRegistry.view<ParticleComponent>()) {
		for (const Vector3& offset : offsets) {
			const Vector3 position = Vector3Add(mRegistry.get<PositionComponent>(particle).Position, offset);

			float topSupport = Vector3DotProduct(topNormal, position);
			float leftSupport = Vector3DotProduct(leftNormal, position);
			float bottomSupport = Vector3DotProduct(bottomNormal, position);
			float rightSupport = Vector3DotProduct(rightNormal, position);

			if (topSupport <= topAnchor && leftSupport <= leftAnchor && bottomSupport <= bottomAnchor && rightSupport <= rightAnchor) {
				if (!mRegistry.any_of<OrientationComponent>(particle))
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

	EndMode3D();
	DrawFPS(10, 10);
	EndDrawing();
}

