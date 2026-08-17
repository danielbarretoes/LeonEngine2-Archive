#include "Gameplay/UParticleComponent.hpp"
#include "Gameplay/AActor.hpp"
#include "Engine/UWorld.hpp"
#include "Engine/Components.hpp"

#include <algorithm>
#include <cmath>

namespace Leon {

    UParticleComponent::UParticleComponent(const std::string& InName) : UActorComponent(InName) {}

    glm::vec3 UParticleComponent::RandomInRange(const glm::vec3& InMin, const glm::vec3& InMax) {
        auto next = [&]() {
            RngState = RngState * 1664525u + 1013904223u;
            return static_cast<float>(RngState & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
        };
        return {InMin.x + (InMax.x - InMin.x) * next(), InMin.y + (InMax.y - InMin.y) * next(),
                InMin.z + (InMax.z - InMin.z) * next()};
    }

    void UParticleComponent::BeginPlay() {
        if (Owner)
            RngState = 1u + static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Owner) & 0xFFFFFFFFu);
        if (bActive)
            Activate(true);
        PushToRenderComponent();
    }

    void UParticleComponent::Activate(bool bReset) {
        bActive = true;
        if (bReset) {
            Particles.clear();
            bHasEmitted = false;
        }
        glm::vec3 origin = Owner ? Owner->GetActorLocation() : glm::vec3(0.0f);
        if (Settings.Kind == EParticleKind::Beam)
            SpawnBeam(origin, Settings.BeamEnd);
        else
            SpawnBurst(origin);
        bHasEmitted = true;
        PushToRenderComponent();
    }

    void UParticleComponent::Deactivate() {
        bActive = false;
        Particles.clear();
        PushToRenderComponent();
    }

    void UParticleComponent::SpawnBurst(const glm::vec3& InOrigin) {
        const int32_t count = std::max(1, Settings.BurstCount);
        Particles.reserve(Particles.size() + static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i) {
            FParticleInstance p;
            p.Kind = EParticleKind::SpriteBurst;
            p.Location = InOrigin;
            p.Velocity = RandomInRange(Settings.VelocityMin, Settings.VelocityMax);
            p.Color = Settings.Color;
            p.ColorEnd = Settings.ColorEnd;
            p.Lifetime = std::max(0.01f, Settings.Lifetime);
            p.Size = Settings.Size;
            p.SizeEnd = Settings.SizeEnd;
            Particles.push_back(p);
        }
    }

    void UParticleComponent::SpawnBeam(const glm::vec3& InStart, const glm::vec3& InEnd) {
        FParticleInstance p;
        p.Kind = EParticleKind::Beam;
        p.Location = InStart;
        p.BeamEnd = InEnd;
        p.Color = Settings.Color;
        p.ColorEnd = Settings.ColorEnd;
        p.Lifetime = std::max(0.01f, Settings.Lifetime);
        p.Size = Settings.BeamThickness;
        p.SizeEnd = Settings.BeamThickness * 0.35f;
        Particles.push_back(p);
    }

    void UParticleComponent::Tick(float DeltaSeconds) {
        if (!bActive)
            return;

        for (FParticleInstance& p : Particles) {
            p.Age += DeltaSeconds;
            if (p.Kind == EParticleKind::SpriteBurst) {
                p.Velocity += Settings.Gravity * DeltaSeconds;
                p.Location += p.Velocity * DeltaSeconds;
            }
        }
        Particles.erase(std::remove_if(Particles.begin(), Particles.end(),
                                       [](const FParticleInstance& p) { return p.Age >= p.Lifetime; }),
                        Particles.end());

        PushToRenderComponent();

        if (bHasEmitted && Settings.bOneShot && Particles.empty()) {
            bActive = false;
            if (bDestroyOwnerWhenDone && Owner && Owner->GetWorld())
                Owner->GetWorld()->DestroyActor(Owner);
        }
    }

    void UParticleComponent::PushToRenderComponent() {
        if (!Owner)
            return;
        if (!Owner->HasComponent<FParticleRenderComponent>())
            Owner->AddComponent<FParticleRenderComponent>();
        auto& render = Owner->GetComponent<FParticleRenderComponent>();
        render.Particles = Particles;
        render.bVisible = bActive || !Particles.empty();
    }

} // namespace Leon
