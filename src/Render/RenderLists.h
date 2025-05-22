#pragma once

#include "CameraFrustm.h"
#include "Components.h"
#include <SpaceUtil.h>
#include <Tracy.hpp>
#include <atomic>
#include <entt/entt.hpp>
#include <optional>
#include <raylib.h>
#include <raymath.h>
#include <vector>

using CameraRays = std::array<Ray, 4>;

class RenderLists
{
public:
    static constexpr int32_t ProgressRespawners = 0;
    static constexpr int32_t ProgressSpaceships = 1;
    static constexpr int32_t ProgressExplosions = 2;
    static constexpr int32_t ProgressBullets = 3;
    static constexpr int32_t ProgressAsteroids = 4;
    static constexpr int32_t ProgressParticles = 5;
    static constexpr uint32_t AllProgressFlags = (1 << 6) - 1;

    std::vector<std::tuple<Vector3, uint32_t>> Respawners;
    std::vector<std::tuple<Vector3, Quaternion, uint32_t>> Spaceships;
    std::vector<std::tuple<Vector3, float, float>> Explosions;
    std::vector<std::tuple<Vector3, Color>> Bullets;
    std::vector<std::tuple<Vector3, float>> Asteroids;
    std::vector<std::tuple<Vector3, Color>> Particles;

    std::atomic<uint32_t> BakeProgressFlags = 0;

    static constexpr Vector3 BackgroundOffset = {SpaceData::LengthX * 0.5f, -100.f, SpaceData::LengthZ * 0.5f};

    std::optional<Vector3>
    FindFrustumVisiblePosition(const CameraFrustum& frustum, const Vector3& position, float radius)
    {
        const Vector3 renderPosition = SpaceUtil::FindVectorGap(frustum.Target, position) + frustum.Target;

        float topSupport = Vector3DotProduct(frustum.TopNormal, renderPosition) - radius;
        float leftSupport = Vector3DotProduct(frustum.LeftNormal, renderPosition) - radius;
        float bottomSupport = Vector3DotProduct(frustum.BottomNormal, renderPosition) - radius;
        float rightSupport = Vector3DotProduct(frustum.RightNormal, renderPosition) - radius;

        if (topSupport <= frustum.TopSupport && leftSupport <= frustum.LeftSupport &&
            bottomSupport <= frustum.BottomSupport && rightSupport <= frustum.RightSupport) {
            return renderPosition;
        }
        return {};
    }

    template <typename TAction>
    void IterateFrustumVisiblePositions(const CameraRays& cameraRays,
                                        const CameraFrustum& frustum,
                                        const Vector3& position,
                                        float radius,
                                        TAction&& action)
    {
        const float planeY = position.y;

        float minX = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::min();
        float minZ = std::numeric_limits<float>::max();
        float maxZ = std::numeric_limits<float>::min();

        for (int i = 0; i < cameraRays.size(); ++i) {
            const Vector3 rayPos = cameraRays[i].position;
            const Vector3 rayDir = cameraRays[i].direction;
            const float distance = (planeY - rayPos.y) / rayDir.y;
            const Vector3 planePoint = Vector3Add(rayPos, Vector3Scale(rayDir, distance));
            minX = std::min(minX, planePoint.x);
            maxX = std::max(maxX, planePoint.x);
            minZ = std::min(minZ, planePoint.z);
            maxZ = std::max(maxZ, planePoint.z);
        }

        minX -= radius;
        maxX += radius;
        minZ -= radius;
        maxZ += radius;

        const int fromNX = ceil((minX - position.x) / SpaceData::LengthX);
        const int toNX = floor((maxX - position.x) / SpaceData::LengthX);
        const int fromNZ = ceil((minZ - position.z) / SpaceData::LengthZ);
        const int toNZ = floor((maxZ - position.z) / SpaceData::LengthZ);

        constexpr Vector3 horizontalTranslate = {SpaceData::LengthX, 0.f, 0.f};
        constexpr Vector3 verticalTranslate = {0.f, 0.f, SpaceData::LengthZ};
        for (int iX = fromNX; iX <= toNX; ++iX) {
            for (int iZ = fromNZ; iZ <= toNZ; ++iZ) {
                const Vector3 testPosition =
                Vector3Add(position, Vector3Add(Vector3Scale(horizontalTranslate, iX),
                                                Vector3Scale(verticalTranslate, iZ)));

                float topSupport = Vector3DotProduct(frustum.TopNormal, testPosition) - radius;
                float leftSupport = Vector3DotProduct(frustum.LeftNormal, testPosition) - radius;
                float bottomSupport = Vector3DotProduct(frustum.BottomNormal, testPosition) - radius;
                float rightSupport = Vector3DotProduct(frustum.RightNormal, testPosition) - radius;

                if (topSupport <= frustum.TopSupport && leftSupport <= frustum.LeftSupport &&
                    bottomSupport <= frustum.BottomSupport && rightSupport <= frustum.RightSupport) {
                    action(testPosition);
                }
            }
        }
    }

    void Clear()
    {
        BakeProgressFlags = 0;

        Respawners.clear();
        Spaceships.clear();
        Explosions.clear();
        Bullets.clear();
        Asteroids.clear();
        Particles.clear();
    }

    void BakeRespawners(const entt::registry* simFrame, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Respawners.empty());
        assert((BakeProgressFlags & (1 << ProgressRespawners)) == 0);

        auto insertAction = [&](auto&& position, auto&& inputId) {
            if (auto renderPosition =
                FindFrustumVisiblePosition(frustum, position, SpaceshipData::CollisionRadius)) {
                Respawners.emplace_back(renderPosition.value(), inputId);
            }
        };
        for (auto respawner : simFrame->view<RespawnComponent, PositionComponent>()) {
            const auto& respawnComponent = simFrame->get<RespawnComponent>(respawner);
            if (respawnComponent.TimeLeft > 0.f) {
                continue;
            }
            const auto& position = simFrame->get<PositionComponent>(respawner).Position;
            insertAction(position, respawnComponent.InputId);
            insertAction(position + BackgroundOffset, respawnComponent.InputId);
        }
        BakeProgressFlags |= (1 << ProgressRespawners);
    }

    void BakeSpaceships(const entt::registry* simFrame, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Spaceships.empty());
        assert((BakeProgressFlags & (1 << ProgressSpaceships)) == 0);

        auto insertAction = [&](auto&& position, auto&& orientation, auto&& inputID) {
            if (auto renderPosition =
                FindFrustumVisiblePosition(frustum, position, SpaceshipData::CollisionRadius)) {
                Spaceships.emplace_back(renderPosition.value(), orientation, inputID);
            }
        };
        for (auto entity : simFrame->view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {

            const auto& position = simFrame->get<PositionComponent>(entity).Position;
            const auto& orientation = simFrame->get<OrientationComponent>(entity).Rotation;
            const uint32_t inputID = simFrame->get<SpaceshipInputComponent>(entity).InputId;
            insertAction(position, orientation, inputID);
            insertAction(position + BackgroundOffset, orientation, inputID);
        }
        BakeProgressFlags |= (1 << ProgressSpaceships);
    }

    void BakeExplosions(const entt::registry* simFrame, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Explosions.empty());
        assert((BakeProgressFlags & (1 << ProgressExplosions)) == 0);
        auto insertAction = [&](auto&& position, auto&& radius, auto&& relativeRadius) {
            if (auto renderPosition = FindFrustumVisiblePosition(frustum, position, radius)) {
                Explosions.emplace_back(renderPosition.value(), radius, std::clamp(relativeRadius, 0.f, 1.f));
            }
        };
        for (auto explosion : simFrame->view<ExplosionComponent>()) {
            const Vector3& position = simFrame->get<PositionComponent>(explosion).Position;
            const ExplosionComponent& explosionComponent = simFrame->get<ExplosionComponent>(explosion);
            const float radius = explosionComponent.CurrentRadius;
            const float relativeRadius = radius / explosionComponent.TerminalRadius;
            insertAction(position, radius, relativeRadius);
            insertAction(position + BackgroundOffset, radius, relativeRadius);
        }
        BakeProgressFlags |= (1 << ProgressExplosions);
    }

    void BakeAsteroids(const entt::registry* simFrame, const CameraRays& cameraRays, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert((BakeProgressFlags & (1 << ProgressAsteroids)) == 0);
        auto insertAction = [&](auto&& position, auto&& radius) {
            IterateFrustumVisiblePositions(cameraRays, frustum, position, radius, [&](const Vector3& visiblePosition) {
                Asteroids.emplace_back(visiblePosition, radius);
            });
        };

        for (auto asteroid : simFrame->view<AsteroidComponent>()) {
            const float radius = simFrame->get<AsteroidComponent>(asteroid).Radius;
            const Vector3 position = simFrame->get<PositionComponent>(asteroid).Position;
            insertAction(position, radius);
            insertAction(position + BackgroundOffset, radius);
        }
        BakeProgressFlags |= (1 << ProgressAsteroids);
    }

    void BakeBullets(const entt::registry* simFrame, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Bullets.empty());
        assert((BakeProgressFlags & (1 << ProgressBullets)) == 0);
        auto insertAction = [&](auto&& position, auto&& color) {
            if (auto renderPosition = FindFrustumVisiblePosition(frustum, position, 0.f)) {
                Bullets.emplace_back(renderPosition.value(), color);
            }
        };
        for (entt::entity particle : simFrame->view<BulletComponent>()) {
            const Vector3 position = simFrame->get<PositionComponent>(particle).Position;
            const Color color = simFrame->get<ParticleComponent>(particle).Color;
            insertAction(position, color);
            insertAction(position + BackgroundOffset, color);
        }
        BakeProgressFlags |= (1 << ProgressBullets);
    }

    void BakeParticles(const entt::registry* simFrame, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Particles.empty());
        assert((BakeProgressFlags & (1 << ProgressParticles)) == 0);
        auto insertAction = [&](auto&& position, auto&& color) {
            if (auto renderPosition = FindFrustumVisiblePosition(frustum, position, 0.f)) {
                Particles.emplace_back(renderPosition.value(), color);
            }
        };
        for (entt::entity particle : simFrame->view<ParticleComponent>(entt::exclude<BulletComponent>)) {
            const Vector3 position = simFrame->get<PositionComponent>(particle).Position;
            const Color color = simFrame->get<ParticleComponent>(particle).Color;
            insertAction(position, color);
            insertAction(position + BackgroundOffset, color);
        }
        BakeProgressFlags |= (1 << ProgressParticles);
    }
};