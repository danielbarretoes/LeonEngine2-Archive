#pragma once

#include "AI/FNavTypes.hpp"

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace Leon {

    class UWorld;

    /**
     * Grid navmesh over ANavMeshBoundsVolume. Rasterizes WorldStatic obstacles
     * and answers A* queries. Game-agnostic: no teams, weapons, or combat.
     */
    class UNavigationSystem {
    public:
        void Rebuild(UWorld& InWorld);
        bool IsBuilt() const { return bBuilt && Width > 0 && Depth > 0; }

        FNavPath FindPath(const glm::vec3& InStart, const glm::vec3& InGoal) const;
        bool ProjectPoint(const glm::vec3& InPoint, glm::vec3& OutProjected) const;
        bool IsWalkable(const glm::vec3& InPoint) const;

        void DrawDebug() const;

        int32_t GetWidth() const { return Width; }
        int32_t GetDepth() const { return Depth; }
        float GetCellSize() const { return CellSize; }
        int32_t GetWalkableCount() const { return WalkableCount; }
        uint32_t GetPathsValid() const { return PathsValid; }
        uint32_t GetPathsFailed() const { return PathsFailed; }

        glm::vec3 GetBoundsMin() const { return BoundsMin; }
        glm::vec3 GetBoundsMax() const { return BoundsMax; }

        float AgentRadius = 0.4f;
        float CellSize = 0.75f;
        float ProbeHeight = 0.55f;

    private:
        int32_t Index(int32_t X, int32_t Z) const { return Z * Width + X; }
        bool InRange(int32_t X, int32_t Z) const { return X >= 0 && Z >= 0 && X < Width && Z < Depth; }
        bool CellWalkable(int32_t X, int32_t Z) const {
            return InRange(X, Z) && Walkable[static_cast<size_t>(Index(X, Z))] != 0;
        }
        glm::vec3 CellCenter(int32_t X, int32_t Z) const;
        bool WorldToCell(const glm::vec3& InPoint, int32_t& OutX, int32_t& OutZ) const;
        bool FindNearestWalkable(int32_t InX, int32_t InZ, int32_t& OutX, int32_t& OutZ) const;
        bool LineWalkable(int32_t X0, int32_t Z0, int32_t X1, int32_t Z1) const;
        void SmoothPath(std::vector<glm::ivec2>& InOutCells) const;

        bool bBuilt = false;
        int32_t Width = 0;
        int32_t Depth = 0;
        int32_t WalkableCount = 0;
        glm::vec3 BoundsMin{0.0f};
        glm::vec3 BoundsMax{0.0f};
        float FloorY = 0.0f;
        std::vector<uint8_t> Walkable;
        mutable uint32_t PathsValid = 0;
        mutable uint32_t PathsFailed = 0;
    };

} // namespace Leon
