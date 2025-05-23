#pragma once

#include "CameraFrustm.h"
#include "Components.h"
#include "FrustumPlaneData.h"
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

    template <typename TAction>
    void IterateFrustumVisiblePositions(const CameraFrustum& frustum,
                                        const FrustumPlaneData& frustumPlaneData,
                                        const Vector3& position,
                                        float radius,
                                        TAction&& action)
    {
        const float minX = frustumPlaneData.MinX - radius;
        const float maxX = frustumPlaneData.MaxX + radius;
        const float minZ = frustumPlaneData.MinZ - radius;
        const float maxZ = frustumPlaneData.MaxZ + radius;

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

    FrustumPlaneData ComputeFrustumPlaneData(const CameraRays& cameraRays, float planeY)
    {
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

        return {minX, maxX, minZ, maxZ};
    }

    void BakeRespawners(const entt::registry* simFrame, const CameraRays& cameraRays, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Respawners.empty());
        assert((BakeProgressFlags & (1 << ProgressRespawners)) == 0);

        auto insertAction = [&](auto&& position, auto&& inputId, auto&& planeData) {
            IterateFrustumVisiblePositions(frustum, planeData, position, SpaceshipData::CollisionRadius,
                                           [&](const Vector3& renderPosition) {
                                               Respawners.emplace_back(renderPosition, inputId);
                                           });
        };

        const FrustumPlaneData foregroundData = ComputeFrustumPlaneData(cameraRays, 0.f);
        const FrustumPlaneData backgroundData = ComputeFrustumPlaneData(cameraRays, BackgroundOffset.y);

        for (auto respawner : simFrame->view<RespawnComponent, PositionComponent>()) {
            const auto& respawnComponent = simFrame->get<RespawnComponent>(respawner);
            if (respawnComponent.TimeLeft > 0.f) {
                continue;
            }
            const auto& position = simFrame->get<PositionComponent>(respawner).Position;
            insertAction(position, respawnComponent.InputId, foregroundData);
            insertAction(position + BackgroundOffset, respawnComponent.InputId, backgroundData);
        }
        BakeProgressFlags |= (1 << ProgressRespawners);
    }

    void BakeSpaceships(const entt::registry* simFrame, const CameraRays& cameraRays, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Spaceships.empty());
        assert((BakeProgressFlags & (1 << ProgressSpaceships)) == 0);

        auto insertAction = [&](auto&& position, auto&& orientation, auto&& inputID, auto&& planeData) {
            IterateFrustumVisiblePositions(frustum, planeData, position, SpaceshipData::CollisionRadius,
                                           [&](const Vector3& renderPosition) {
                                               Spaceships.emplace_back(renderPosition, orientation, inputID);
                                           });
        };

        const FrustumPlaneData foregroundData = ComputeFrustumPlaneData(cameraRays, 0.f);
        const FrustumPlaneData backgroundData = ComputeFrustumPlaneData(cameraRays, BackgroundOffset.y);

        for (auto entity : simFrame->view<PositionComponent, OrientationComponent, SpaceshipInputComponent>()) {

            const auto& position = simFrame->get<PositionComponent>(entity).Position;
            const auto& orientation = simFrame->get<OrientationComponent>(entity).Rotation;
            const uint32_t inputID = simFrame->get<SpaceshipInputComponent>(entity).InputId;
            insertAction(position, orientation, inputID, foregroundData);
            insertAction(position + BackgroundOffset, orientation, inputID, backgroundData);
        }
        BakeProgressFlags |= (1 << ProgressSpaceships);
    }

    void BakeExplosions(const entt::registry* simFrame, const CameraRays& cameraRays, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Explosions.empty());
        assert((BakeProgressFlags & (1 << ProgressExplosions)) == 0);

        auto insertAction = [&](auto&& position, auto&& radius, auto&& relativeRadius, auto&& planeData) {
            IterateFrustumVisiblePositions(frustum, planeData, position, radius, [&](const Vector3& renderPosition) {
                Explosions.emplace_back(renderPosition, radius, std::clamp(relativeRadius, 0.f, 1.f));
            });
        };

        const FrustumPlaneData foregroundData = ComputeFrustumPlaneData(cameraRays, 0.f);
        const FrustumPlaneData backgroundData = ComputeFrustumPlaneData(cameraRays, BackgroundOffset.y);

        for (auto explosion : simFrame->view<ExplosionComponent>()) {
            const Vector3& position = simFrame->get<PositionComponent>(explosion).Position;
            const ExplosionComponent& explosionComponent = simFrame->get<ExplosionComponent>(explosion);
            const float radius = explosionComponent.CurrentRadius;
            const float relativeRadius = radius / explosionComponent.TerminalRadius;
            insertAction(position, radius, relativeRadius, foregroundData);
            insertAction(position + BackgroundOffset, radius, relativeRadius, backgroundData);
        }
        BakeProgressFlags |= (1 << ProgressExplosions);
    }

    void BakeAsteroids(const entt::registry* simFrame, const CameraRays& cameraRays, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert((BakeProgressFlags & (1 << ProgressAsteroids)) == 0);

        auto insertAction = [&](auto&& position, auto&& radius, auto&& frustumData) {
            IterateFrustumVisiblePositions(frustum, frustumData, position, radius, [&](const Vector3& visiblePosition) {
                Asteroids.emplace_back(visiblePosition, radius);
            });
        };

        const FrustumPlaneData foregroundData = ComputeFrustumPlaneData(cameraRays, 0.f);
        const FrustumPlaneData backgroundData = ComputeFrustumPlaneData(cameraRays, BackgroundOffset.y);

        for (auto asteroid : simFrame->view<AsteroidComponent>()) {
            const float radius = simFrame->get<AsteroidComponent>(asteroid).Radius;
            const Vector3 position = simFrame->get<PositionComponent>(asteroid).Position;
            insertAction(position, radius, foregroundData);
            insertAction(position + BackgroundOffset, radius, backgroundData);
        }
        BakeProgressFlags |= (1 << ProgressAsteroids);
    }

    void BakeBullets(const entt::registry* simFrame, const CameraRays& cameraRays, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Bullets.empty());
        assert((BakeProgressFlags & (1 << ProgressBullets)) == 0);

        auto insertAction = [&](auto&& position, auto&& color, auto&& planeData) {
            IterateFrustumVisiblePositions(frustum, planeData, position, 0.f, [&](const Vector3& renderPosition) {
                Bullets.emplace_back(renderPosition, color);
            });
        };

        const FrustumPlaneData foregroundData = ComputeFrustumPlaneData(cameraRays, 0.f);
        const FrustumPlaneData backgroundData = ComputeFrustumPlaneData(cameraRays, BackgroundOffset.y);

        for (entt::entity particle : simFrame->view<BulletComponent>()) {
            const Vector3 position = simFrame->get<PositionComponent>(particle).Position;
            const Color color = simFrame->get<ParticleComponent>(particle).Color;
            insertAction(position, color, foregroundData);
            insertAction(position + BackgroundOffset, color, backgroundData);
        }
        BakeProgressFlags |= (1 << ProgressBullets);
    }

    void BakeParticles(const entt::registry* simFrame, const CameraRays& cameraRays, const CameraFrustum& frustum)
    {
        ZoneScoped;
        assert(Particles.empty());
        assert((BakeProgressFlags & (1 << ProgressParticles)) == 0);

        auto insertAction = [&](auto&& position, auto&& color, auto&& planeData) {
            IterateFrustumVisiblePositions(frustum, planeData, position, 0.f, [&](const Vector3& renderPosition) {
                Particles.emplace_back(renderPosition, color);
            });
        };

        const FrustumPlaneData foregroundData = ComputeFrustumPlaneData(cameraRays, 0.f);
        const FrustumPlaneData backgroundData = ComputeFrustumPlaneData(cameraRays, BackgroundOffset.y);

        for (entt::entity particle : simFrame->view<ParticleComponent>(entt::exclude<BulletComponent>)) {
            const Vector3 position = simFrame->get<PositionComponent>(particle).Position;
            const Color color = simFrame->get<ParticleComponent>(particle).Color;
            insertAction(position, color, foregroundData);
            insertAction(position + BackgroundOffset, color, backgroundData);
        }
        BakeProgressFlags |= (1 << ProgressParticles);
    }
};