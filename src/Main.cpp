#include "raylib.h"

static void SetupWindow() {
	SetTargetFPS(60);
	InitWindow(1200, 600, "Game");
}

static Camera SetupCamera() {
	Camera camera;
	camera.position = { 10.f,10.f,10.f };
	camera.target = { 0.f, 0.f, 0.f };
	camera.projection = CAMERA_PERSPECTIVE;
	camera.up = { 0.f, 1.f, 0.f };
	camera.fovy = 60.f;	// NEAR PLANE IF ORTHOGRAPHIC
	SetCameraMode(camera, CAMERA_FIRST_PERSON);
	return camera;
}

static void Draw(const Camera& camera) {
	BeginDrawing();
	ClearBackground(GRAY);
	BeginMode3D(camera);
	DrawGrid(100, 1.0f);
	EndMode3D();
	EndDrawing();
}

void main() {
	SetupWindow();
	Camera camera = SetupCamera();
	while (!WindowShouldClose()) {
		UpdateCamera(&camera);
		Draw(camera);
	}
	CloseWindow();
}