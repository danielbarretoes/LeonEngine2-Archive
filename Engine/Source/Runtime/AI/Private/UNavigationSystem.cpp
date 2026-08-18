#include "AI/UNavigationSystem.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/ANavMeshBoundsVolume.hpp"
#include "Physics/FHitResult.hpp"
#include "Engine/ECollisionChannel.hpp"
#include "Renderer/FDebugRenderer.hpp"
#include "Core/FLog.hpp"
#include "Core/FFrameProfiler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace Leon {

    glm::vec3 UNavigationSystem::CellCenter(int32_t X, int32_t Z) const {
        return {BoundsMin.x + (static_cast<float>(X) + 0.5f) * CellSize, FloorY,
                BoundsMin.z + (static_cast<float>(Z) + 0.5f) * CellSize};
    }

    bool UNavigationSystem::WorldToCell(const glm::vec3& InPoint, int32_t& OutX, int32_t& OutZ) const {
        if (!bBuilt)
            return false;
        OutX = static_cast<int32_t>(std::floor((InPoint.x - BoundsMin.x) / CellSize));
        OutZ = static_cast<int32_t>(std::floor((InPoint.z - BoundsMin.z) / CellSize));
        return InRange(OutX, OutZ);
    }

    bool UNavigationSystem::FindNearestWalkable(int32_t InX, int32_t InZ, int32_t& OutX, int32_t& OutZ) const {
        if (CellWalkable(InX, InZ)) {
            OutX = InX;
            OutZ = InZ;
            return true;
        }
        const int32_t maxR = std::max(Width, Depth);
        for (int32_t r = 1; r <= maxR; ++r) {
            for (int32_t dz = -r; dz <= r; ++dz) {
                for (int32_t dx = -r; dx <= r; ++dx) {
                    if (std::max(std::abs(dx), std::abs(dz)) != r)
                        continue;
                    const int32_t x = InX + dx;
                    const int32_t z = InZ + dz;
                    if (CellWalkable(x, z)) {
                        OutX = x;
                        OutZ = z;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool UNavigationSystem::LineWalkable(int32_t X0, int32_t Z0, int32_t X1, int32_t Z1) const {
        int32_t x = X0;
        int32_t z = Z0;
        const int32_t dx = std::abs(X1 - X0);
        const int32_t dz = std::abs(Z1 - Z0);
        const int32_t sx = X0 < X1 ? 1 : -1;
        const int32_t sz = Z0 < Z1 ? 1 : -1;
        int32_t err = dx - dz;
        while (true) {
            if (!CellWalkable(x, z))
                return false;
            if (x == X1 && z == Z1)
                return true;
            const int32_t e2 = 2 * err;
            if (e2 > -dz) {
                err -= dz;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                z += sz;
            }
        }
    }

    void UNavigationSystem::SmoothPath(std::vector<glm::ivec2>& InOutCells) const {
        if (InOutCells.size() < 3)
            return;
        std::vector<glm::ivec2> out;
        out.push_back(InOutCells.front());
        size_t i = 0;
        while (i + 1 < InOutCells.size()) {
            size_t best = i + 1;
            for (size_t j = InOutCells.size() - 1; j > i + 1; --j) {
                if (LineWalkable(InOutCells[i].x, InOutCells[i].y, InOutCells[j].x, InOutCells[j].y)) {
                    best = j;
                    break;
                }
            }
            out.push_back(InOutCells[best]);
            i = best;
        }
        InOutCells.swap(out);
    }

    void UNavigationSystem::Rebuild(UWorld& InWorld) {
        bBuilt = false;
        Width = Depth = WalkableCount = 0;
        Walkable.clear();
        PathsValid = PathsFailed = 0;

        glm::vec3 bmin(std::numeric_limits<float>::max());
        glm::vec3 bmax(-std::numeric_limits<float>::max());
        bool bAny = false;
        for (const auto& actor : InWorld.GetAllActors()) {
            auto* vol = dynamic_cast<ANavMeshBoundsVolume*>(actor.get());
            if (!vol || vol->IsPendingKill())
                continue;
            glm::vec3 vmin, vmax;
            vol->GetBounds(vmin, vmax);
            bmin = glm::min(bmin, vmin);
            bmax = glm::max(bmax, vmax);
            bAny = true;
        }
        if (!bAny)
            return;

        BoundsMin = bmin;
        BoundsMax = bmax;
        FloorY = bmin.y;
        const float spanX = std::max(bmax.x - bmin.x, CellSize);
        const float spanZ = std::max(bmax.z - bmin.z, CellSize);
        Width = std::max(1, static_cast<int32_t>(std::ceil(spanX / CellSize)));
        Depth = std::max(1, static_cast<int32_t>(std::ceil(spanZ / CellSize)));
        Walkable.assign(static_cast<size_t>(Width * Depth), 0);

        const glm::vec3 probeHalf(AgentRadius, ProbeHeight * 0.5f, AgentRadius);
        for (int32_t z = 0; z < Depth; ++z) {
            for (int32_t x = 0; x < Width; ++x) {
                const glm::vec3 c = CellCenter(x, z);
                const glm::vec3 probe(c.x, FloorY + ProbeHeight, c.z);
                if (!InWorld.OverlapAnyTestByChannel(probe, probeHalf, ECollisionChannel::WorldStatic, nullptr)) {
                    Walkable[static_cast<size_t>(Index(x, z))] = 1;
                    ++WalkableCount;
                }
            }
        }
        bBuilt = WalkableCount > 0;
        LE_CORE_INFO("NavMesh rebuilt: {0}x{1} cells, {2} walkable, bounds ({3:.1f},{4:.1f})-({5:.1f},{6:.1f})", Width,
                     Depth, WalkableCount, BoundsMin.x, BoundsMin.z, BoundsMax.x, BoundsMax.z);
    }

    bool UNavigationSystem::ProjectPoint(const glm::vec3& InPoint, glm::vec3& OutProjected) const {
        int32_t x = 0, z = 0;
        if (!WorldToCell(InPoint, x, z)) {
            x = std::clamp(static_cast<int32_t>(std::floor((InPoint.x - BoundsMin.x) / CellSize)), 0, Width - 1);
            z = std::clamp(static_cast<int32_t>(std::floor((InPoint.z - BoundsMin.z) / CellSize)), 0, Depth - 1);
        }
        int32_t nx = 0, nz = 0;
        if (!FindNearestWalkable(x, z, nx, nz))
            return false;
        OutProjected = CellCenter(nx, nz);
        OutProjected.y = InPoint.y;
        return true;
    }

    bool UNavigationSystem::IsWalkable(const glm::vec3& InPoint) const {
        int32_t x = 0, z = 0;
        return WorldToCell(InPoint, x, z) && CellWalkable(x, z);
    }

    FNavPath UNavigationSystem::FindPath(const glm::vec3& InStart, const glm::vec3& InGoal) const {
        FFrameProfiler::FScope nav(&FFrameProfiler::Working().NavigationMs);
        ++FFrameProfiler::Working().PathRequests;
        FNavPath path;
        if (!bBuilt) {
            ++PathsFailed;
            return path;
        }

        glm::vec3 startP, goalP;
        if (!ProjectPoint(InStart, startP) || !ProjectPoint(InGoal, goalP)) {
            ++PathsFailed;
            return path;
        }

        int32_t sx = 0, sz = 0, gx = 0, gz = 0;
        WorldToCell(startP, sx, sz);
        WorldToCell(goalP, gx, gz);
        FindNearestWalkable(sx, sz, sx, sz);
        FindNearestWalkable(gx, gz, gx, gz);

        if (sx == gx && sz == gz) {
            path.Points = {InStart, InGoal};
            path.Status = ENavPathStatus::Valid;
            path.Length = glm::length(glm::vec3(InGoal.x - InStart.x, 0.0f, InGoal.z - InStart.z));
            ++PathsValid;
            return path;
        }

        const int32_t n = Width * Depth;
        std::vector<float> gScore(static_cast<size_t>(n), 1e30f);
        std::vector<int32_t> came(static_cast<size_t>(n), -1);
        auto heur = [&](int32_t x, int32_t z) {
            const float dx = static_cast<float>(x - gx);
            const float dz = static_cast<float>(z - gz);
            return std::sqrt(dx * dx + dz * dz);
        };

        using FNode = std::pair<float, int32_t>;
        std::priority_queue<FNode, std::vector<FNode>, std::greater<FNode>> open;
        const int32_t startI = Index(sx, sz);
        gScore[static_cast<size_t>(startI)] = 0.0f;
        open.push({heur(sx, sz), startI});

        const int32_t neigh[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        bool bFound = false;
        const int32_t goalI = Index(gx, gz);
        int32_t visited = 0;
        const int32_t visitCap = n + 8;

        while (!open.empty() && visited < visitCap) {
            const int32_t cur = open.top().second;
            open.pop();
            ++visited;
            if (cur == goalI) {
                bFound = true;
                break;
            }
            const int32_t cx = cur % Width;
            const int32_t cz = cur / Width;
            for (const auto& d : neigh) {
                const int32_t nx = cx + d[0];
                const int32_t nz = cz + d[1];
                if (!CellWalkable(nx, nz))
                    continue;
                if (d[0] != 0 && d[1] != 0 && (!CellWalkable(cx + d[0], cz) || !CellWalkable(cx, cz + d[1])))
                    continue;
                const int32_t ni = Index(nx, nz);
                const float step = (d[0] != 0 && d[1] != 0) ? 1.41421356f : 1.0f;
                const float tentative = gScore[static_cast<size_t>(cur)] + step;
                if (tentative >= gScore[static_cast<size_t>(ni)])
                    continue;
                came[static_cast<size_t>(ni)] = cur;
                gScore[static_cast<size_t>(ni)] = tentative;
                open.push({tentative + heur(nx, nz), ni});
            }
        }

        std::vector<glm::ivec2> cells;
        if (bFound) {
            int32_t cur = goalI;
            while (cur >= 0) {
                cells.push_back({cur % Width, cur / Width});
                cur = came[static_cast<size_t>(cur)];
            }
            std::reverse(cells.begin(), cells.end());
            path.Status = ENavPathStatus::Valid;
        } else {
            int32_t best = startI;
            float bestG = 1e30f;
            for (int32_t i = 0; i < n; ++i) {
                if (gScore[static_cast<size_t>(i)] >= 1e29f)
                    continue;
                const int32_t x = i % Width;
                const int32_t z = i / Width;
                const float h = heur(x, z);
                if (h < bestG) {
                    bestG = h;
                    best = i;
                }
            }
            int32_t cur = best;
            while (cur >= 0) {
                cells.push_back({cur % Width, cur / Width});
                cur = came[static_cast<size_t>(cur)];
            }
            std::reverse(cells.begin(), cells.end());
            path.Status = cells.size() >= 2 ? ENavPathStatus::Partial : ENavPathStatus::Invalid;
        }

        if (cells.size() < 2) {
            path.Status = ENavPathStatus::Invalid;
            ++PathsFailed;
            return path;
        }

        SmoothPath(cells);
        path.Points.push_back(InStart);
        for (const auto& c : cells)
            path.Points.push_back(CellCenter(c.x, c.y));
        path.Points.back() = glm::vec3(InGoal.x, path.Points.back().y, InGoal.z);
        path.Length = 0.0f;
        for (size_t i = 1; i < path.Points.size(); ++i) {
            glm::vec3 d = path.Points[i] - path.Points[i - 1];
            d.y = 0.0f;
            path.Length += glm::length(d);
        }
        if (path.Status == ENavPathStatus::Valid)
            ++PathsValid;
        else
            ++PathsFailed;
        return path;
    }

    void UNavigationSystem::DrawDebug() const {
        if (!bBuilt)
            return;
        const glm::vec3 center = (BoundsMin + BoundsMax) * 0.5f;
        const glm::vec3 extent = (BoundsMax - BoundsMin) * 0.5f;
        FDebugRenderer::DrawDebugBox(center, extent, glm::vec4(0.15f, 0.7f, 0.35f, 0.35f));

        const glm::vec4 blocked(0.85f, 0.2f, 0.15f, 0.55f);
        int32_t drawn = 0;
        for (int32_t z = 0; z < Depth && drawn < 800; ++z) {
            for (int32_t x = 0; x < Width && drawn < 800; ++x) {
                if (CellWalkable(x, z))
                    continue;
                bool bEdge = false;
                const int32_t n[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& d : n) {
                    if (CellWalkable(x + d[0], z + d[1])) {
                        bEdge = true;
                        break;
                    }
                }
                if (!bEdge)
                    continue;
                const glm::vec3 c = CellCenter(x, z);
                FDebugRenderer::DrawDebugBox(c + glm::vec3(0.0f, 0.08f, 0.0f),
                                             glm::vec3(CellSize * 0.4f, 0.04f, CellSize * 0.4f), blocked);
                ++drawn;
            }
        }
    }

} // namespace Leon
