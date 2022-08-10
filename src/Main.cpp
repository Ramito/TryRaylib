#include "entt/entt.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"

static void SetupWindow()
{
	SetTargetFPS(120);
	InitWindow(2400, 1500, "Game");
}

constexpr Vector3 CameraOffset = {-10.f, 25.f, -10.f};

static Camera SetupCamera()
{
	Camera camera;
	camera.position = Vector3Negate(CameraOffset);
	camera.target = {0.f, 0.f, 0.f};
	camera.projection = CAMERA_PERSPECTIVE;
	camera.up = {0.f, 1.f, 0.f};
	camera.fovy = 72.f;
	SetCameraMode(camera, CAMERA_CUSTOM);
	return camera;
}

struct GameInput
{
	float Roll;
	float Pitch;
	float Yaw;
	float Thrust;
	bool Fire = false;
};

struct FlightStickState
{
	float Roll;
	float Pitch;
	float Yaw;
	float Thrust;

	auto operator<=>(const FlightStickState&) const = default;
};

constexpr Vector3 Forward3 = {0.f, 0.f, 1.f};
constexpr Vector3 Back3 = {0.f, 0.f, -1.f};
constexpr Vector3 Left3 = {1.f, 0.f, 0.f};
constexpr Vector3 Up3 = {0.f, 1.f, 0.f};

namespace SimTimeData
{
constexpr uint32_t TargetFPS = 30;
constexpr float DeltaTime = 1.f / TargetFPS;
} // namespace SimTimeData

namespace SpaceshipData
{
constexpr float MinThrust = 10.f;
constexpr float Thrust = 15.f;
constexpr float LinearDrag = 1e-5;
constexpr float QuadraticDrag = 1e-3;

constexpr float Yaw = 0.4f;
constexpr float Pitch = 2.5f;
constexpr float Roll = 1.5f;

constexpr float NegativePitch = 1.25f;
constexpr float NegativeRoll = 2.75f;

constexpr float SteerB = 0.25f;
constexpr float SteerM = 1.5f;
} // namespace SpaceshipData

namespace WeaponData
{
constexpr float RateOfFire = 0.125f;
constexpr float BulletSpeed = 75.f;
constexpr float BulletLifetime = 1.f;
constexpr std::array<Vector3, 2> ShootBones = {Vector3{0.65f, 0.f, 0.f}, Vector3{-0.65f, 0.f, 0.f}};
} // namespace WeaponData

namespace ParticleData
{
constexpr float LinearDrag = 0.f;
constexpr float QuadraticDrag = 1e-2;
} // namespace ParticleData

struct SteerComponent
{
	float Steer;
};
struct GunComponent
{
	float TimeBeforeNextShot;
	uint32_t NextShotBone;
};
struct ThrustComponent
{
	float Thrust;
};
struct PositionComponent
{
	Vector3 Position;
};
struct OrientationComponent
{
	Quaternion Quaternion;
};
struct VelocityComponent
{
	Vector3 Velocity;
};
struct SpaceshipControlComponent
{
	uint32_t InputId;
	FlightStickState State;
	bool FireTrigger = false;
};
struct ParticleComponent
{
	uint32_t LifeTime;
};
struct TerminalSimulationNodeComponent
{ };
struct SimulationNodeComponent
{ };

static void DrawSpaceShip(const Vector3& position, const Quaternion orientation, Color color)
{
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
	Vector3 vertex3 =
		Vector3Subtract(Vector3Add(Vector3Scale(relVertex, Width), position), upShift);

	DrawLine3D(vertex1, vertex2, color);
	DrawLine3D(vertex2, vertex3, color);
	DrawLine3D(vertex3, vertex1, color);

	vertex2 = Vector3Add(vertex2, downShift);
	vertex3 = Vector3Subtract(vertex3, downShift);

	DrawLine3D(vertex1, vertex2, color);
	DrawLine3D(vertex2, vertex3, color);
	DrawLine3D(vertex3, vertex1, color);

	Vector3 midBack = Vector3Lerp(vertex2, vertex3, 0.5f);
	DrawLine3D(vertex1, midBack, color);
}

static void Draw(Camera& camera, entt::registry& registry)
{
	for(auto playerEntity : registry.view<PositionComponent>())
	{
		auto& position = registry.get<PositionComponent>(playerEntity);
		camera.target = position.Position;
		camera.position = Vector3Add(camera.target, CameraOffset);
	}
	UpdateCamera(&camera);
	BeginDrawing();
	ClearBackground(DARKGRAY);
	BeginMode3D(camera);
	DrawGrid(500, 5.0f);

	for(auto entity : registry.view<PositionComponent,
									OrientationComponent,
									SpaceshipControlComponent,
									SteerComponent>())
	{
		auto& position = registry.get<PositionComponent>(entity);
		auto& orientation = registry.get<OrientationComponent>(entity);
		DrawSpaceShip(position.Position, orientation.Quaternion, RED);
	}

	for(auto entity :
		registry.view<PositionComponent, OrientationComponent, TerminalSimulationNodeComponent>())
	{
		auto& position = registry.get<PositionComponent>(entity);
		auto& orientation = registry.get<OrientationComponent>(entity);
		DrawSpaceShip(position.Position, orientation.Quaternion, YELLOW);
	}

	for(entt::entity particle : registry.view<ParticleComponent>())
	{
		const Vector3& position = registry.get<PositionComponent>(particle).Position;
		if(!registry.any_of<OrientationComponent>(particle))
		{
			DrawPoint3D(position, ORANGE);
		}
		else
		{
			DrawSphere(position, 0.2f, GREEN);
		}
	}

	EndMode3D();
	EndDrawing();
}

void UpdateInput(const Camera& camera, std::array<GameInput, 4>& gameInputs)
{
	size_t idx = 0;
	for(GameInput& gameInput : gameInputs)
	{
		if(IsGamepadAvailable(idx))
		{
			gameInput.Roll = GetGamepadAxisMovement(idx, GAMEPAD_AXIS_LEFT_X);
			gameInput.Pitch = -GetGamepadAxisMovement(idx, GAMEPAD_AXIS_LEFT_Y);
			gameInput.Yaw = GetGamepadAxisMovement(idx, GAMEPAD_AXIS_RIGHT_X);
			gameInput.Thrust = 0.f;
			if(IsGamepadButtonDown(idx, GAMEPAD_BUTTON_RIGHT_TRIGGER_2))
			{
				gameInput.Thrust += 1.f;
			}
			if(IsGamepadButtonDown(idx, GAMEPAD_BUTTON_LEFT_TRIGGER_2))
			{
				gameInput.Thrust -= 1.f;
			}
		}
	}
}

static entt::registry SetupSim()
{
	entt::registry registry;
	registry.reserve(256000);

	entt::entity player = registry.create();
	registry.emplace<SpaceshipControlComponent>(player, 0u, FlightStickState{0.f, 0.f, 0.f, 0.f});
	registry.emplace<SteerComponent>(player, 0.f);
	registry.emplace<ThrustComponent>(player, 0.f);
	registry.emplace<PositionComponent>(player, 0.f, 2.f, 0.f);
	registry.emplace<VelocityComponent>(player, 0.f, 0.f, 0.f);
	registry.emplace<OrientationComponent>(player, QuaternionIdentity());
	registry.emplace<GunComponent>(player, 0.f, 0u);

	return registry;
}

static Vector3 HorizontalOrthogonal(const Vector3& vector)
{
	return {-vector.z, vector.y, vector.x};
}

static void ProcessInput(entt::registry& registry, std::array<GameInput, 4> gameInput)
{
	for(SpaceshipControlComponent& controlComponent :
		registry.view<SpaceshipControlComponent>().storage())
	{
		auto input = gameInput[controlComponent.InputId];
		controlComponent.State.Roll = input.Roll;
		controlComponent.State.Pitch = input.Pitch;
		controlComponent.State.Yaw = input.Yaw;
		controlComponent.State.Thrust = 0.5f + 0.5f * input.Thrust;
		controlComponent.FireTrigger = input.Fire;
	}
}

static void SimulateSpaceship(float deltaTime,
							  PositionComponent& positionComponent,
							  VelocityComponent& velocityComponent,
							  OrientationComponent& orientationComponent,
							  const FlightStickState& control)
{
	Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);
	Vector3 left = Vector3RotateByQuaternion(Left3, orientationComponent.Quaternion);
	Vector3 up = Vector3RotateByQuaternion(Up3, orientationComponent.Quaternion);

	Vector3 rollV3 = Vector3Scale(forward, control.Roll * SpaceshipData::Roll);
	Vector3 yawV3 = Vector3Scale(up, -control.Yaw * SpaceshipData::Yaw);
	Vector3 pitchV3 = Vector3Scale(left, control.Pitch * SpaceshipData::Pitch);

	Vector3 angularSpeedV3 = Vector3Add(Vector3Add(rollV3, yawV3), pitchV3);
	// Scale to apply as quaternion rotation
	angularSpeedV3 = Vector3Scale(angularSpeedV3, 0.5f * deltaTime);

	Quaternion rotationalSpeed = {angularSpeedV3.x, angularSpeedV3.y, angularSpeedV3.z, 0.f};
	Quaternion currentQuaternion = orientationComponent.Quaternion;

	currentQuaternion =
		QuaternionAdd(currentQuaternion, QuaternionMultiply(rotationalSpeed, currentQuaternion));
	orientationComponent.Quaternion = QuaternionNormalize(currentQuaternion);

	Vector3 newForward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);

	float thrust = SpaceshipData::MinThrust + control.Thrust * SpaceshipData::Thrust;

	Vector3 acceleration = Vector3Scale(newForward, deltaTime * thrust);
	Vector3 velocity = Vector3Add(velocityComponent.Velocity, acceleration);

	float speed = Vector3Length(velocity);
	if(!FloatEquals(speed, 0.f))
	{
		float drag =
			speed * SpaceshipData::LinearDrag + speed * speed * SpaceshipData::QuadraticDrag;
		velocity = Vector3Add(velocity, Vector3Scale(velocity, -drag / speed));
	}

	velocityComponent.Velocity = velocity;
	Vector3 displacement = Vector3Scale(velocity, deltaTime);

	positionComponent.Position = Vector3Add(positionComponent.Position, displacement);
}

template <typename TComponent>
static TComponent&
CloneComponent(entt::entity target, entt::entity source, entt::registry& registry)
{
	return registry.emplace<TComponent>(target, registry.get<TComponent>(source));
}

std::vector<FlightStickState> states;

static void CreateStepClone(float deltaTime,
							entt::entity base,
							entt::registry& registry,
							const FlightStickState state)
{
	registry.remove<TerminalSimulationNodeComponent>(base);

	entt::entity child = registry.create();
	registry.emplace<SimulationNodeComponent>(child);
	registry.emplace<TerminalSimulationNodeComponent>(child);

	PositionComponent& positionComponent = CloneComponent<PositionComponent>(child, base, registry);
	VelocityComponent& velocityComponent = CloneComponent<VelocityComponent>(child, base, registry);
	OrientationComponent& orientationComponent =
		CloneComponent<OrientationComponent>(child, base, registry);
	SpaceshipControlComponent& controlComponent =
		CloneComponent<SpaceshipControlComponent>(child, base, registry);

	controlComponent.State = state;

	SimulateSpaceship(deltaTime, positionComponent, velocityComponent, orientationComponent, state);
	SimulateSpaceship(deltaTime, positionComponent, velocityComponent, orientationComponent, state);
	SimulateSpaceship(deltaTime, positionComponent, velocityComponent, orientationComponent, state);
}

static void ExpandAllSimNodes(float deltaTime, entt::entity base, entt::registry& registry)
{
	states.clear();

	const FlightStickState originalState = registry.get<SpaceshipControlComponent>(base).State;

	constexpr float ControlSpeed = 0.5f;

	for(int i = -1; i <= 1; ++i)
	{
		FlightStickState iteratingState = originalState;
		iteratingState.Roll = std::clamp(iteratingState.Roll + ControlSpeed * i, -1.f, 1.f);
		for(int j = -1; j <= 1; ++j)
		{
			iteratingState.Pitch = std::clamp(iteratingState.Pitch + ControlSpeed * j, -1.f, 1.f);
			for(int k = -1; k <= 1; ++k)
			{
				iteratingState.Yaw = std::clamp(iteratingState.Yaw + ControlSpeed * k, -1.f, 1.f);
				for(int l = -1; l <= 1; ++l)
				{
					iteratingState.Thrust =
						std::clamp(iteratingState.Thrust + ControlSpeed * l, 0.f, 1.f);

					if(std::find(states.begin(), states.end(), iteratingState) != states.end())
					{
						continue;
					}
					states.push_back(iteratingState);
				}
			}
		}
	}

	for(const auto& state : states)
	{
		CreateStepClone(deltaTime, base, registry, state);
	}
}

static void ExpandRandomSimNode(float deltaTime, entt::entity base, entt::registry& registry)
{
	const FlightStickState originalState = registry.get<SpaceshipControlComponent>(base).State;

	constexpr float ControlSpeed = 0.5f;

	int i = GetRandomValue(-1, 1);
	int j = GetRandomValue(-1, 1);
	int k = GetRandomValue(-1, 1);
	int l = GetRandomValue(-1, 1);

	FlightStickState state;
	state.Roll = std::clamp(originalState.Roll + ControlSpeed * i, -1.f, 1.f);
	state.Pitch = std::clamp(originalState.Pitch + ControlSpeed * j, -1.f, 1.f);
	state.Yaw = std::clamp(originalState.Yaw + ControlSpeed * k, -1.f, 1.f);
	state.Thrust = std::clamp(originalState.Thrust + ControlSpeed * l, 0.f, 1.f);

	CreateStepClone(deltaTime, base, registry, state);
}

static void Simulate(entt::registry& registry)
{
	constexpr float deltaTime = SimTimeData::DeltaTime;

	auto simulated = registry.view<SimulationNodeComponent>();
	registry.destroy(simulated.begin(), simulated.end());

	auto playerView = registry.view<PositionComponent,
									VelocityComponent,
									OrientationComponent,
									SteerComponent,
									SpaceshipControlComponent,
									ThrustComponent>();
	auto playerProcess = [deltaTime, &registry](entt::entity entity,
												PositionComponent& positionComponent,
												VelocityComponent& velocityComponent,
												OrientationComponent& orientationComponent,
												SteerComponent& steerComponent,
												const SpaceshipControlComponent& inputComponent,
												ThrustComponent& thrustComponent) {
		const FlightStickState& state = inputComponent.State;

		SimulateSpaceship(
			deltaTime, positionComponent, velocityComponent, orientationComponent, state);

		ExpandRandomSimNode(deltaTime, entity, registry);

		thrustComponent.Thrust = SpaceshipData::MinThrust + state.Thrust * SpaceshipData::Thrust;
		;
	};
	playerView.each(playerProcess);

	size_t count = 3;
	while(count-- != 0)
	{
		std::vector<entt::entity> nodes;
		nodes.reserve(registry.storage<TerminalSimulationNodeComponent>().size());
		for(entt::entity node : registry.view<TerminalSimulationNodeComponent>())
		{
			nodes.emplace_back(node);
		}
		for(entt::entity node : nodes)
		{
			ExpandRandomSimNode(deltaTime, node, registry);
		}
	}

	auto shootView = registry.view<PositionComponent,
								   VelocityComponent,
								   OrientationComponent,
								   SpaceshipControlComponent,
								   GunComponent>();
	auto shootProcess = [&registry](const PositionComponent& positionComponent,
									const VelocityComponent& velocityComponent,
									const OrientationComponent& orientationComponent,
									const SpaceshipControlComponent& inputComponent,
									GunComponent& gunComponent) {
		gunComponent.TimeBeforeNextShot =
			std::max(gunComponent.TimeBeforeNextShot - deltaTime, 0.f);
		if(!inputComponent.FireTrigger)
		{
			return;
		}
		if(gunComponent.TimeBeforeNextShot > 0.f)
		{
			return;
		}
		Vector3 forward = Vector3RotateByQuaternion(Forward3, orientationComponent.Quaternion);
		Vector3 offset = Vector3RotateByQuaternion(
			WeaponData::ShootBones[gunComponent.NextShotBone], orientationComponent.Quaternion);
		Vector3 shotPosition = Vector3Add(positionComponent.Position, offset);
		Vector3 shotVelocity =
			Vector3Add(velocityComponent.Velocity, Vector3Scale(forward, WeaponData::BulletSpeed));
		entt::entity bullet = registry.create();
		registry.emplace<PositionComponent>(bullet, shotPosition);
		registry.emplace<OrientationComponent>(bullet, orientationComponent.Quaternion);
		registry.emplace<VelocityComponent>(bullet, shotVelocity);
		registry.emplace<ParticleComponent>(
			bullet, static_cast<uint32_t>(WeaponData::BulletLifetime / SimTimeData::DeltaTime));
		gunComponent.NextShotBone += 1;
		gunComponent.NextShotBone %= WeaponData::ShootBones.size();
		gunComponent.TimeBeforeNextShot = WeaponData::RateOfFire;
	};
	shootView.each(shootProcess);

	auto thrustView =
		registry
			.view<ThrustComponent, PositionComponent, VelocityComponent, OrientationComponent>();
	auto thrustParticleProcess = [&](ThrustComponent& thrustComponent,
									 const PositionComponent& positionComponent,
									 const VelocityComponent& velocityComponent,
									 const OrientationComponent& orientationComponent) {
		constexpr float ThrustModule = 25.f;
		constexpr float RandomModule = 5.5f;
		constexpr float Offset = 0.4f;
		constexpr uint32_t MinParticles = 0;
		constexpr uint32_t MaxParticles = 3;
		float relativeThrust = thrustComponent.Thrust / SpaceshipData::Thrust;
		uint32_t particles = MinParticles + relativeThrust * relativeThrust * MaxParticles;

		Vector3 baseVelocity = velocityComponent.Velocity;
		Vector3 back = Vector3RotateByQuaternion(Back3, orientationComponent.Quaternion);
		baseVelocity = Vector3Add(
			baseVelocity, Vector3Scale(back, thrustComponent.Thrust * ThrustModule * deltaTime));

		while(particles-- > 0)
		{
			entt::entity particleEntity = registry.create();
			registry.emplace<PositionComponent>(
				particleEntity, Vector3Add(positionComponent.Position, Vector3Scale(back, Offset)));
			float randX =
				1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
			float randY =
				1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
			float randZ =
				1.f - 2.f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
			Vector3 randomVelocity = Vector3Scale({randX, randY, randZ}, RandomModule);
			registry.emplace<VelocityComponent>(particleEntity,
												Vector3Add(baseVelocity, randomVelocity));
			uint32_t lifetime = (std::rand() + std::rand()) / (2 * RAND_MAX / 1250);
			registry.emplace<ParticleComponent>(particleEntity, lifetime);
		}
	};
	thrustView.each(thrustParticleProcess);

	auto particleView = registry.view<ParticleComponent, PositionComponent, VelocityComponent>();
	auto particleProcess = [deltaTime, &registry](entt::entity particle,
												  ParticleComponent& particleComponent,
												  PositionComponent& positionComponent,
												  VelocityComponent& velocityComponent) {
		if(particleComponent.LifeTime == 0)
		{
			registry.destroy(particle);
			return;
		}
		particleComponent.LifeTime--;
		positionComponent.Position = Vector3Add(
			positionComponent.Position, Vector3Scale(velocityComponent.Velocity, deltaTime));

		if(registry.any_of<OrientationComponent>(particle))
		{
			// It's a bullet!
			return;
		}

		float speed = Vector3Length(velocityComponent.Velocity);
		if(!FloatEquals(speed, 0.f))
		{
			float drag =
				speed * ParticleData::LinearDrag + speed * speed * ParticleData::QuadraticDrag;
			velocityComponent.Velocity =
				Vector3Add(velocityComponent.Velocity,
						   Vector3Scale(velocityComponent.Velocity, -drag / speed));
		}
	};
	particleView.each(particleProcess);
}

void main()
{
	SetupWindow();
	Camera camera = SetupCamera();
	entt::registry registry = SetupSim();
	double gameStartTime = GetTime();
	uint32_t simTicks = 0;
	std::array<GameInput, 4> gameInput;
	while(!WindowShouldClose())
	{
		UpdateInput(camera, gameInput);
		ProcessInput(registry, gameInput);
		while(gameStartTime + SimTimeData::DeltaTime * simTicks < GetTime())
		{
			Simulate(registry);
			simTicks += 1;
		}
		Draw(camera, registry);
	}
	CloseWindow();
}