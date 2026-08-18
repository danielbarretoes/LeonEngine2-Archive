#include "Gameplay/FProceduralPrimitiveSpawner.hpp"
#include "Engine/UWorld.hpp"
#include "Gameplay/AActor.hpp"
#include "Gameplay/UPrimitiveComponent.hpp"

namespace Leon {

    AActor* FProceduralPrimitiveSpawner::SpawnStaticBox(UWorld* InWorld, const std::string& InName,
                                                        const glm::vec3& InLocation, const glm::vec3& InScale) {
        if (!InWorld)
            return nullptr;
        AActor* actor = InWorld->SpawnActor<AActor>(InName);
        actor->SetActorLocation(InLocation);
        actor->SetActorScale(InScale);
        auto box = actor->AddActorComponent<UBoxComponent>("Box");
        box->SetBoxExtent(glm::vec3(0.5f));
        box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
        box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        return actor;
    }

} // namespace Leon
