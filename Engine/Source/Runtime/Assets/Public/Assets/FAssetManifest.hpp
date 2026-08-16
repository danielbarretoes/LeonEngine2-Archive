#pragma once

#include "Core/Base.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leon {

    struct FManifestEntry {
        std::string SourcePath;
        std::string SourceHash;
        uint64_t SourceSize = 0;
        uint64_t ImportTimestamp = 0;
        std::vector<std::string> GeneratedAssets;
        std::vector<std::string> Dependencies;
    };

    class FAssetManifest {
    public:
        static std::string ComputeFileHash(const std::string& InFilePath);

        bool LoadFromFile(const std::string& InManifestPath);
        bool SaveToFile(const std::string& InManifestPath) const;

        bool NeedsReimport(const std::string& InSourcePath) const;

        void RegisterImport(const std::string& InSourcePath, const std::vector<std::string>& InGeneratedAssets,
                            const std::vector<std::string>& InDependencies);

        void RemoveEntry(const std::string& InSourcePath);

        const std::unordered_map<std::string, FManifestEntry>& GetEntries() const { return Entries; }
        std::unordered_map<std::string, FManifestEntry>& GetEntries() { return Entries; }

        void Clear() { Entries.clear(); }

    private:
        std::unordered_map<std::string, FManifestEntry> Entries;
    };

} // namespace Leon
