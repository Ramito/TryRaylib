#pragma once

#include "entt/entt.hpp"
#include "DependencyContainer.h"
#include "Data.h"
#include "SpatialPartition.h"
#include <random>

struct SimFlag {};
using SimDependencies = DependencyContainer<SimFlag>;

class Simulation {
public:
	Simulation(const SimDependencies& dependencies);

	void Init();
	void Tick();

	float GameTime;

private:
	void Simulate();

	uint32_t mFrame = 0;

	entt::registry& mRegistry;
	const std::array<GameInput, 4>& mGameInput;
	SpatialPartition<entt::entity> mSpatialPartition;

	std::default_random_engine mRandomGenerator;
};