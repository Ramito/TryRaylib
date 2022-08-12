#include "entt/entt.hpp"
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <random>

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

constexpr Vector3 Forward3 = {0.f, 0.f, 1.f};
constexpr Vector3 Back3 = {0.f, 0.f, -1.f};
constexpr Vector3 Left3 = {1.f, 0.f, 0.f};
constexpr Vector3 Up3 = {0.f, 1.f, 0.f};

struct GameInput
{
	Vector3 Direction = Forward3;
	float Thrust = 0.f;
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

namespace SimTimeData
{
constexpr uint32_t TargetFPS = 60;
constexpr float DeltaTime = 1.f / TargetFPS;
} // namespace SimTimeData

namespace SpaceshipData
{
constexpr float MinThrust = 10.f;
constexpr float Thrust = 15.f;
constexpr float LinearDrag = 1e-5;
constexpr float QuadraticDrag = 1e-3;

constexpr float Yaw = 0.75f;
constexpr float Pitch = 2.25f;
constexpr float Roll = 1.25f;
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
	FlightStickState State;
};
struct PlayerControlComponent
{
	uint32_t InputId;
	GameInput Input;
};
struct ParticleComponent
{
	uint32_t LifeTime;
};

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
		auto& position = registry.get<PositionComponent>(entity).Position;
		auto& orientation = registry.get<OrientationComponent>(entity);
		DrawSpaceShip(position, orientation.Quaternion, RED);
		if(const auto* playerInput = registry.try_get<PlayerControlComponent>(entity))
		{
			auto endpoint = Vector3Add(position, Vector3Scale(playerInput->Input.Direction, 10.f));
			DrawLine3D(position, endpoint, YELLOW);
			endpoint = Vector3Add(
				position,
				Vector3Scale(playerInput->Input.Direction, playerInput->Input.Thrust * 10.f));
			DrawLine3D(position, endpoint, WHITE);
		}
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
	Vector2 pos = {camera.position.x, camera.position.z};
	Vector2 tar = {camera.target.x, camera.target.z};
	Vector2 to = Vector2Subtract(tar, pos);
	Vector2 normalizedTo = Vector2Normalize(to);
	Vector2 normalizedOrthogonal = {-normalizedTo.y, normalizedTo.x};

	size_t idx = 0;
	for(GameInput& gameInput : gameInputs)
	{
		gameInput = GameInput{};
		if(IsGamepadAvailable(idx))
		{
			Vector2 stick;
			stick.x = GetGamepadAxisMovement(idx, GAMEPAD_AXIS_LEFT_X);
			stick.y = -GetGamepadAxisMovement(idx, GAMEPAD_AXIS_LEFT_Y);

			float stickLength = Vector2Length(stick);
			if(!FloatEquals(stickLength, 0.f))
			{
				stick = Vector2Scale(stick, 1.f / stickLength);
			}

			gameInput.Direction = {Vector2DotProduct(stick, normalizedOrthogonal),
								   0.f,
								   Vector2DotProduct(stick, normalizedTo)};

			gameInput.Thrust = std::min(stickLength, 1.f);

			gameInput.Fire = IsGamepadButtonDown(idx, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
		}
		idx++;
	}
}

static entt::registry SetupSim()
{
	entt::registry registry;
	registry.reserve(2000);

	entt::entity player = registry.create();
	registry.emplace<PlayerControlComponent>(player, 0u, GameInput{});
	registry.emplace<SpaceshipControlComponent>(player, FlightStickState{0.f, 0.f, 0.f, 0.f});
	registry.emplace<SteerComponent>(player, 0.f);
	registry.emplace<ThrustComponent>(player, 0.f);
	registry.emplace<PositionComponent>(player, 0.f, 0.f, 0.f);
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
	for(PlayerControlComponent& controlComponent :
		registry.view<PlayerControlComponent>().storage())
	{
		controlComponent.Input = gameInput[controlComponent.InputId];
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

class ManeuverFinder
{
public:
	constexpr static uint32_t TreeIterations = 125;
	constexpr static uint32_t DepthIterations = 60;

	static inline void ExpandChildren(float deltaTime,
									  entt::entity source,
									  entt::registry& sourceRegistry,
									  entt::registry& targetRegistry,
									  const GameInput& input,
									  std::vector<FlightStickState>& scratchpad)
	{
		FlightStickState originalState;
		if(const SpaceshipControlComponent* control =
			   sourceRegistry.try_get<SpaceshipControlComponent>(source))
		{
			originalState = control->State;
		}
		else
		{
			originalState = sourceRegistry.get<FlightStickState>(source);
		}

		NodeComponent* sourceNodeComponent = sourceRegistry.try_get<NodeComponent>(source);

		ExpandStickStates(originalState, scratchpad);

		for(const FlightStickState& state : scratchpad)
		{
			entt::entity internalEntity = targetRegistry.create();

			if(sourceNodeComponent == nullptr)
			{
				targetRegistry.emplace<RootNodeComponent>(internalEntity);
			}
			else
			{
				sourceNodeComponent->ChildCount++;
				targetRegistry.emplace<ChildLinkComponent>(internalEntity,
														   sourceNodeComponent->FirstChild);
				sourceNodeComponent->FirstChild = internalEntity;
			}

			targetRegistry.emplace<FlightStickState>(internalEntity, state);

			PositionComponent& positionComponent = targetRegistry.emplace<PositionComponent>(
				internalEntity, sourceRegistry.get<PositionComponent>(source));

			VelocityComponent& velocityComponent = targetRegistry.emplace<VelocityComponent>(
				internalEntity, sourceRegistry.get<VelocityComponent>(source));

			OrientationComponent& orientationComponent =
				targetRegistry.emplace<OrientationComponent>(
					internalEntity, sourceRegistry.get<OrientationComponent>(source));

			SimulateSpaceship(
				deltaTime, positionComponent, velocityComponent, orientationComponent, state);

			entt::entity parent = entt::null;
			uint32_t timeSteps = 1u;
			if(sourceNodeComponent != nullptr)
			{
				parent = source;
				timeSteps += sourceNodeComponent->TimeSteps;
			}

			float nodeValue = Value(deltaTime * timeSteps,
									velocityComponent.Velocity,
									positionComponent.Position.y,
									Vector3RotateByQuaternion(Up3, orientationComponent.Quaternion),
									input);

			targetRegistry.emplace<NodeComponent>(
				internalEntity, nodeValue, 1u, timeSteps, 0u, entt::null, parent);
		}
	}

	FlightStickState FindNextControl(float deltaTime,
									 entt::entity source,
									 entt::registry& sourceRegistry,
									 const GameInput& input)
	{
		internalRegistry.clear();

		ExpandChildren(
			deltaTime, source, sourceRegistry, internalRegistry, input, stickStateScratchpad);

		Iterations = 1;

		while(Iterations < TreeIterations)
		{
			IterateTree(deltaTime, input);
			++Iterations;
		}

		UpdateBestRootNode<false>();

		entt::entity bestNode = *internalRegistry.view<BestRootNodeComponent>().begin();

		return internalRegistry.get<FlightStickState>(bestNode);
	}

	template <bool TUtility>
	void UpdateBestRootNode()
	{
		internalRegistry.clear<BestRootNodeComponent>();

		auto rootNodes = internalRegistry.view<NodeComponent, RootNodeComponent>();

		float bestUtility = std::numeric_limits<float>::lowest();
		entt::entity bestNode;

		for(entt::entity node : rootNodes)
		{
			const NodeComponent& nodeComponent = internalRegistry.get<NodeComponent>(node);

			float utility;
			if constexpr(TUtility)
			{
				utility = Utility(nodeComponent.Value, nodeComponent.Iterations, Iterations);
			}
			else
			{
				utility = nodeComponent.Value / nodeComponent.Iterations;
			}

			if(utility > bestUtility)
			{
				bestUtility = utility;
				bestNode = node;
			}
		}

		internalRegistry.emplace<BestRootNodeComponent>(bestNode);
	}

	entt::entity GetBestLeafNode(entt::entity parent)
	{
		const NodeComponent& parentNodeComponent = internalRegistry.get<NodeComponent>(parent);
		const uint32_t childCount = parentNodeComponent.ChildCount;
		entt::entity iteratingChild = parentNodeComponent.FirstChild;

		entt::entity bestChild;
		float bestUtility = std::numeric_limits<float>::lowest();

		for(uint32_t it = 0; it < childCount; ++it)
		{
			const NodeComponent& childNodeComponent =
				internalRegistry.get<NodeComponent>(iteratingChild);
			float utility = Utility(childNodeComponent.Value,
									childNodeComponent.Iterations,
									parentNodeComponent.Iterations);
			if(utility > bestUtility)
			{
				bestUtility = utility;
				bestChild = iteratingChild;
			}

			iteratingChild = internalRegistry.get<ChildLinkComponent>(iteratingChild).NextChild;
		}

		return bestChild;
	}

	std::default_random_engine randomGenerator; // WIP WIP WIP
	void IterateTree(float deltaTime, const GameInput& input)
	{
		UpdateBestRootNode<true>();
		entt::entity bestNode = *internalRegistry.view<BestRootNodeComponent>().begin();

		const NodeComponent* bestNodeComponent = &internalRegistry.get<NodeComponent>(bestNode);
		while(bestNodeComponent->ChildCount > 0)
		{
			const uint32_t childCount = bestNodeComponent->ChildCount;
			entt::entity iteratingChild = bestNodeComponent->FirstChild;

			float bestUtility = std::numeric_limits<float>::lowest();

			for(uint32_t childIt = 0; childIt < childCount; ++childIt)
			{
				const NodeComponent& childNodeComponent =
					internalRegistry.get<NodeComponent>(iteratingChild);
				float utility = Utility(childNodeComponent.Value,
										childNodeComponent.Iterations,
										bestNodeComponent->Iterations);
				if(utility > bestUtility)
				{
					bestUtility = utility;
					bestNode = iteratingChild;
				}

				iteratingChild = internalRegistry.get<ChildLinkComponent>(iteratingChild).NextChild;
			}
			bestNodeComponent = &internalRegistry.get<NodeComponent>(bestNode);
		}

		ExpandChildren(
			deltaTime, bestNode, internalRegistry, internalRegistry, input, stickStateScratchpad);

		entt::entity bestLeaf = GetBestLeafNode(bestNode);

		uint32_t timeSteps = internalRegistry.get<NodeComponent>(bestLeaf).TimeSteps;

		entt::entity simulated = internalRegistry.create();

		FlightStickState& stickState = internalRegistry.emplace<FlightStickState>(
			simulated, internalRegistry.get<FlightStickState>(bestLeaf));

		PositionComponent& positionComponent = internalRegistry.emplace<PositionComponent>(
			simulated, internalRegistry.get<PositionComponent>(bestLeaf));

		VelocityComponent& velocityComponent = internalRegistry.emplace<VelocityComponent>(
			simulated, internalRegistry.get<VelocityComponent>(bestLeaf));

		OrientationComponent& orientationComponent = internalRegistry.emplace<OrientationComponent>(
			simulated, internalRegistry.get<OrientationComponent>(bestLeaf));

		float simulatedValue = std::numeric_limits<float>::lowest();
		uint32_t iterations = 0;
		while(iterations++ < DepthIterations)
		{
			ExpandStickStates(internalRegistry.get<FlightStickState>(simulated),
							  stickStateScratchpad);
			std::uniform_int_distribution<int> distribution(0, stickStateScratchpad.size() - 1);
			int randomIndex = distribution(randomGenerator);

			stickState = stickStateScratchpad[randomIndex];

			SimulateSpaceship(
				deltaTime, positionComponent, velocityComponent, orientationComponent, stickState);

			timeSteps += 1;
			simulatedValue =
				std::max(simulatedValue,
						 Value(deltaTime * timeSteps,
							   velocityComponent.Velocity,
							   positionComponent.Position.y,
							   Vector3RotateByQuaternion(Up3, orientationComponent.Quaternion),
							   input));
		}

		internalRegistry.destroy(simulated);

		// Back propagate
		entt::entity parent = bestLeaf;

		while(internalRegistry.valid(parent))
		{
			NodeComponent* parentNodeComponent = &internalRegistry.get<NodeComponent>(parent);
			parentNodeComponent->Iterations += 1;
			parentNodeComponent->Value += simulatedValue;
			parent = parentNodeComponent->Parent;
		}
	}

	static float Utility(float nodeValue, uint32_t nodeIterations, uint32_t parentIterations)
	{
		float utility = nodeValue / nodeIterations;
		utility += 2.f * sqrtf(log(static_cast<float>(parentIterations)) / nodeIterations);
		return utility;
	}

	static float Value(float totalTime,
					   const Vector3& velocity,
					   float yPosition,
					   const Vector3& up,
					   const GameInput& input)
	{
		float directionDistance =
			-22.0f * Vector3DistanceSqr(Vector3Normalize(velocity), input.Direction);
		float verticalDistance = -0.25f * abs(yPosition);
		float verticalAlignment = -0.2f * (1.f - Vector3DotProduct(Up3, up));
		return std::min({directionDistance, verticalDistance, verticalAlignment});
	}

	static void ExpandStickStates(const FlightStickState& fromState,
								  std::vector<FlightStickState>& expanded)
	{
		expanded.clear();

		constexpr float ControlSpeed = 0.2f;

		float prevRoll = std::numeric_limits<float>::quiet_NaN();
		float prevPitch = std::numeric_limits<float>::quiet_NaN();
		float prevYaw = std::numeric_limits<float>::quiet_NaN();
		float prevThrust = std::numeric_limits<float>::quiet_NaN();

		for(int i = -1; i <= 1; ++i)
		{
			float roll = std::clamp(fromState.Roll + ControlSpeed * i, -1.f, 1.f);
			if(roll == prevRoll)
			{
				continue;
			}
			prevRoll = roll;

			for(int j = -1; j <= 1; ++j)
			{
				float pitch = std::clamp(fromState.Pitch + ControlSpeed * j, -1.f, 1.f);
				if(pitch == prevPitch)
				{
					continue;
				}
				prevPitch = pitch;

				for(int k = -1; k <= 1; ++k)
				{
					float yaw = std::clamp(fromState.Yaw + ControlSpeed * k, -1.f, 1.f);
					if(yaw == prevYaw)
					{
						continue;
					}
					prevYaw = yaw;

					for(int l = -1; l <= 1; ++l)
					{
						float thrust = std::clamp(fromState.Thrust + ControlSpeed * l, -1.f, 1.f);
						if(thrust == prevThrust)
						{
							continue;
						}
						prevThrust = thrust;

						expanded.push_back({roll, pitch, yaw, thrust});
					}
				}
			}
		}
	}

private:
	struct RootNodeComponent
	{ };
	struct BestRootNodeComponent
	{ };
	struct NodeComponent
	{
		float Value = 0.f;
		uint32_t Iterations = 0;
		uint32_t TimeSteps = 0;
		uint32_t ChildCount = 0;
		entt::entity FirstChild;
		entt::entity Parent;
	};
	struct ChildLinkComponent
	{
		entt::entity NextChild;
	};

	std::vector<FlightStickState> stickStateScratchpad;
	entt::registry internalRegistry;
	uint32_t Iterations = 0;
};

static void Simulate(entt::registry& registry, ManeuverFinder& finder)
{
	constexpr float deltaTime = SimTimeData::DeltaTime;

	auto playerView = registry.view<PositionComponent,
									VelocityComponent,
									OrientationComponent,
									SteerComponent,
									PlayerControlComponent,
									ThrustComponent>();
	auto playerProcess = [deltaTime, &registry, &finder](
							 entt::entity entity,
							 PositionComponent& positionComponent,
							 VelocityComponent& velocityComponent,
							 OrientationComponent& orientationComponent,
							 SteerComponent& steerComponent,
							 const PlayerControlComponent& controlComponent,
							 ThrustComponent& thrustComponent) {
		const FlightStickState& state =
			finder.FindNextControl(deltaTime, entity, registry, controlComponent.Input);

		SimulateSpaceship(
			deltaTime, positionComponent, velocityComponent, orientationComponent, state);

		thrustComponent.Thrust = SpaceshipData::MinThrust + state.Thrust * SpaceshipData::Thrust;
		;
	};
	playerView.each(playerProcess);

	auto shootView = registry.view<PositionComponent,
								   VelocityComponent,
								   OrientationComponent,
								   PlayerControlComponent,
								   GunComponent>();
	auto shootProcess = [&registry](const PositionComponent& positionComponent,
									const VelocityComponent& velocityComponent,
									const OrientationComponent& orientationComponent,
									const PlayerControlComponent& inputComponent,
									GunComponent& gunComponent) {
		gunComponent.TimeBeforeNextShot =
			std::max(gunComponent.TimeBeforeNextShot - deltaTime, 0.f);
		if(!inputComponent.Input.Fire)
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
	ManeuverFinder finder;
	while(!WindowShouldClose())
	{
		UpdateInput(camera, gameInput);
		ProcessInput(registry, gameInput);
		while(gameStartTime + SimTimeData::DeltaTime * simTicks < GetTime())
		{
			Simulate(registry, finder);
			simTicks += 1;
		}
		Draw(camera, registry);
	}
	CloseWindow();
}