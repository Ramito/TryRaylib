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

	void Init(uint32_t players);
	void Tick();

	float GameTime;

private:
	void Simulate();
	void MakeExplosion(const Vector3& position, const Vector3& velocity, float radius);
	void DestroySpaceship(entt::entity spaceship, const Vector3 position, const Vector3 velocity);

	struct CollisionPayload {
		entt::entity Entity;
		float Radius;
	};

	uint32_t mFrame = 0;
	entt::registry& mRegistry;
	const std::array<GameInput, 4>& mGameInput;
	SpatialPartition<CollisionPayload> mSpatialPartition;
	std::default_random_engine mRandomGenerator;
};