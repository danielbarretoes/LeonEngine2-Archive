#include "Assets/UAnimSequence.hpp"
#include "Core/FLog.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>

namespace Leon {

    namespace {
        void WriteString(std::ostream& Out, const std::string& InStr) {
            uint32_t len = static_cast<uint32_t>(InStr.size());
            Out.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0)
                Out.write(InStr.data(), len);
        }

        bool ReadString(std::istream& In, std::string& OutStr) {
            uint32_t len = 0;
            In.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!In || len > 1024 * 1024)
                return false;
            OutStr.assign(len, '\0');
            if (len > 0)
                In.read(OutStr.data(), len);
            return static_cast<bool>(In);
        }

        template <typename TKey> void WriteKeys(std::ostream& Out, const std::vector<TKey>& InKeys) {
            uint32_t n = static_cast<uint32_t>(InKeys.size());
            Out.write(reinterpret_cast<const char*>(&n), sizeof(n));
            if (n > 0)
                Out.write(reinterpret_cast<const char*>(InKeys.data()), n * sizeof(TKey));
        }

        template <typename TKey> bool ReadKeys(std::istream& In, std::vector<TKey>& OutKeys) {
            uint32_t n = 0;
            In.read(reinterpret_cast<char*>(&n), sizeof(n));
            if (!In || n > 1'000'000)
                return false;
            OutKeys.resize(n);
            if (n > 0)
                In.read(reinterpret_cast<char*>(OutKeys.data()), n * sizeof(TKey));
            return static_cast<bool>(In);
        }
    } // namespace

    UAnimSequence::UAnimSequence(const std::string& InName) : Name(InName), UUID(FUUID::FromPath(InName)) {}

    TRef<UAnimSequence> UAnimSequence::Create(const std::string& InName) {
        return MakeRef<UAnimSequence>(InName);
    }

    void UAnimSequence::LinkSkeleton(const TRef<USkeleton>& InSkeleton) {
        TrackToBone.assign(Tracks.size(), -1);
        if (!InSkeleton)
            return;
        for (size_t i = 0; i < Tracks.size(); ++i)
            TrackToBone[i] = InSkeleton->FindBoneIndex(Tracks[i].BoneName);
    }

    float UAnimSequence::WrapTime(float InTime, bool bLoop) const {
        if (Duration <= 1e-6f)
            return 0.0f;
        if (bLoop) {
            float t = std::fmod(InTime, Duration);
            if (t < 0.0f)
                t += Duration;
            return t;
        }
        return glm::clamp(InTime, 0.0f, Duration);
    }

    bool UAnimSequence::SaveToFile(const std::string& InFilePath) const {
        std::ofstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("UAnimSequence: Failed to open \"{0}\" for writing", InFilePath);
            return false;
        }

        uint32_t magic = LANIM_MAGIC;
        uint32_t version = LANIM_VERSION;
        uint32_t trackCount = static_cast<uint32_t>(Tracks.size());
        uint8_t looping = bLooping ? 1 : 0;
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&UUID.High), sizeof(UUID.High));
        file.write(reinterpret_cast<const char*>(&UUID.Low), sizeof(UUID.Low));
        file.write(reinterpret_cast<const char*>(&SkeletonUUID.High), sizeof(SkeletonUUID.High));
        file.write(reinterpret_cast<const char*>(&SkeletonUUID.Low), sizeof(SkeletonUUID.Low));
        WriteString(file, Name);
        WriteString(file, SkeletonPath);
        file.write(reinterpret_cast<const char*>(&Duration), sizeof(Duration));
        file.write(reinterpret_cast<const char*>(&SampleRate), sizeof(SampleRate));
        file.write(reinterpret_cast<const char*>(&looping), sizeof(looping));
        file.write(reinterpret_cast<const char*>(&trackCount), sizeof(trackCount));
        for (const auto& track : Tracks) {
            WriteString(file, track.BoneName);
            WriteKeys(file, track.TranslationKeys);
            WriteKeys(file, track.RotationKeys);
            WriteKeys(file, track.ScaleKeys);
        }
        return file.good();
    }

    bool UAnimSequence::LoadFromFile(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open()) {
            LE_CORE_ERROR("UAnimSequence: Failed to open \"{0}\" for reading", InFilePath);
            return false;
        }

        uint32_t magic = 0, version = 0, trackCount = 0;
        uint8_t looping = 1;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (magic != LANIM_MAGIC || version != LANIM_VERSION) {
            LE_CORE_ERROR("UAnimSequence: Invalid magic/version in \"{0}\"", InFilePath);
            return false;
        }
        file.read(reinterpret_cast<char*>(&UUID.High), sizeof(UUID.High));
        file.read(reinterpret_cast<char*>(&UUID.Low), sizeof(UUID.Low));
        file.read(reinterpret_cast<char*>(&SkeletonUUID.High), sizeof(SkeletonUUID.High));
        file.read(reinterpret_cast<char*>(&SkeletonUUID.Low), sizeof(SkeletonUUID.Low));
        if (!ReadString(file, Name) || !ReadString(file, SkeletonPath))
            return false;
        file.read(reinterpret_cast<char*>(&Duration), sizeof(Duration));
        file.read(reinterpret_cast<char*>(&SampleRate), sizeof(SampleRate));
        file.read(reinterpret_cast<char*>(&looping), sizeof(looping));
        file.read(reinterpret_cast<char*>(&trackCount), sizeof(trackCount));
        bLooping = looping != 0;
        if (!file || trackCount > 4096)
            return false;
        Tracks.resize(trackCount);
        for (auto& track : Tracks) {
            if (!ReadString(file, track.BoneName))
                return false;
            if (!ReadKeys(file, track.TranslationKeys) || !ReadKeys(file, track.RotationKeys) ||
                !ReadKeys(file, track.ScaleKeys))
                return false;
        }
        AssetPath = InFilePath;
        return file.good();
    }

} // namespace Leon
