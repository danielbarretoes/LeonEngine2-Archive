#include "FLeonTournamentArenaBuilder.hpp"
#include "Gameplay/FProceduralPrimitiveSpawner.hpp"

namespace Leon {

    namespace {
        const char* ArenaMaterialPath(ELeonTournamentArenaSurface InSurface) {
            switch (InSurface) {
            case ELeonTournamentArenaSurface::Floor:
                return "/Game/Materials/M_LabFloor.lmat";
            case ELeonTournamentArenaSurface::Wall:
                return "/Game/Materials/M_LabWall.lmat";
            case ELeonTournamentArenaSurface::Metal:
                return "/Game/Materials/M_ArenaMetal.lmat";
            case ELeonTournamentArenaSurface::Accent:
                return "/Game/Materials/M_ArenaAccent.lmat";
            case ELeonTournamentArenaSurface::Ceiling:
                return "/Game/Materials/M_LabCeiling.lmat";
            case ELeonTournamentArenaSurface::Mirror:
                return "/Game/Materials/M_ArenaMirror.lmat";
            case ELeonTournamentArenaSurface::Prop:
            default:
                return "/Game/Materials/M_LabProp.lmat";
            }
        }
    } // namespace

    AActor* FLeonTournamentArenaBuilder::SpawnBox(UWorld* InWorld, const std::string& InName,
                                                  const glm::vec3& InLocation, const glm::vec3& InScale,
                                                  ELeonTournamentArenaSurface InSurface, const glm::vec3& InTint,
                                                  float InUvTile) {
        const bool planar = InSurface == ELeonTournamentArenaSurface::Floor ||
                            InSurface == ELeonTournamentArenaSurface::Mirror;
        const bool visibleInReflection = InSurface != ELeonTournamentArenaSurface::Floor;
        return FProceduralPrimitiveSpawner::SpawnMeshBox(InWorld, InName, InLocation, InScale, InTint,
                                                         ArenaMaterialPath(InSurface), InUvTile, planar,
                                                         visibleInReflection);
    }

    AActor* FLeonTournamentArenaBuilder::SpawnSimpleBox(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InLocation, const glm::vec3& InScale,
                                                        const glm::vec3& InColor) {
        return FProceduralPrimitiveSpawner::SpawnMeshBox(InWorld, InName, InLocation, InScale, InColor);
    }

    AActor* FLeonTournamentArenaBuilder::SpawnPointLight(UWorld* InWorld, const std::string& InName,
                                                         const glm::vec3& InPos, const glm::vec3& InColor,
                                                         float InIntensity, float InRadius, ELightMobility InMobility) {
        return FProceduralPrimitiveSpawner::SpawnPointLight(InWorld, InName, InPos, InColor, InIntensity, InRadius,
                                                            InMobility);
    }

    AActor* FLeonTournamentArenaBuilder::SpawnSpotLight(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InPos, const glm::vec3& InDir,
                                                        const glm::vec3& InColor, float InIntensity, float InRadius,
                                                        float InInnerDeg, float InOuterDeg, ELightMobility InMobility) {
        return FProceduralPrimitiveSpawner::SpawnSpotLight(InWorld, InName, InPos, InDir, InColor, InIntensity, InRadius,
                                                           InInnerDeg, InOuterDeg, InMobility);
    }

} // namespace Leon
