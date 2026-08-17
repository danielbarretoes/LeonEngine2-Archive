#include "Audio/UAudioComponent.hpp"
#include "Audio/FAudioDevice.hpp"
#include "Audio/USoundWave.hpp"
#include "Gameplay/AActor.hpp"

namespace Leon {

    UAudioComponent::UAudioComponent(const std::string& InName) : UActorComponent(InName) {}

    UAudioComponent::~UAudioComponent() {
        Stop();
    }

    void UAudioComponent::SetSound(const TRef<USoundWave>& InSound) {
        Sound = InSound;
    }

    void UAudioComponent::Play() {
        Stop();
        if (!Sound || !Sound->IsValid())
            return;

        glm::vec3 location(0.0f);
        if (Owner)
            location = Owner->GetActorLocation();

        if (bSpatialized)
            ActiveVoiceId =
                FAudioDevice::Get().PlayWave(Sound, VolumeMultiplier, true, location, AttenuationRadius);
        else
            ActiveVoiceId = FAudioDevice::Get().PlayWave(Sound, VolumeMultiplier, false, location, 0.0f);
    }

    void UAudioComponent::Stop() {
        if (ActiveVoiceId != 0) {
            FAudioDevice::Get().StopVoice(ActiveVoiceId);
            ActiveVoiceId = 0;
        }
    }

    void UAudioComponent::Tick(float /*DeltaSeconds*/) {
        // Voice completion is tracked by FAudioDevice::Tick; clear local id when stopped.
        if (ActiveVoiceId != 0 && !FAudioDevice::Get().IsNullDevice()) {
            // Polling via StopVoice no-op if still playing is handled in device map cleanup;
            // local id is cleared on EndPlay/Stop only for fire-and-forget simplicity.
        }
    }

    void UAudioComponent::EndPlay() {
        Stop();
        UActorComponent::EndPlay();
    }

} // namespace Leon
