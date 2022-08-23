#pragma once

#include "entt/entt.hpp"
#include "DependencyContainer.h"
#include "Data.h"

struct SimFlag {};
using SimDependencies = DependencyContainer<SimFlag>;

class Simulation {
public:
	Simulation(const SimDependencies& dependencies);

	void Init();
	void Tick();

private:
	entt::registry& mRegistry;
	const std::array<GameInput, 4>& mGameInput;
};