#pragma once

#include "DependencyContainer.h"

#include "entt/entt.hpp"
#include <raylib.h>

using GameCameras = std::array<Camera, 4>;
using ViewPorts = std::array<Rectangle, 4>;

struct RenderFlag {};
using RenderDependencies = DependencyContainer<RenderFlag>;

class Render {
public:
	Render(uint32_t views, RenderDependencies& dependencies);
	~Render();
	void DrawScreenTexture(float gameTime);
	const Texture& ScreenTexture() const;
private:
	uint32_t mViews;
	entt::registry& mRegistry;
	std::array<Camera, 4>& mCameras;
	std::array<Rectangle, 4>& mViewPorts;
	std::array<RenderTexture, 4> mBackgroundTextures;
	std::array<RenderTexture, 4> mBulletTextures;
	std::array<RenderTexture, 4> mViewPortTextures;
	RenderTexture mScreenTexture;
};
