#pragma once

#include "DependencyContainer.h"

#include "entt/entt.hpp"
#include <raylib.h>

using GameCameras = std::array<Camera, 4>;

struct RenderFlag {};
using RenderDependencies = DependencyContainer<RenderFlag>;

class Render {
public:
	Render(uint32_t viewID, RenderDependencies& dependencies);
	void Init();
	void Draw();
private:
	uint32_t ViewID;
	entt::registry& mRegistry;
	Camera& mMainCamera;
};