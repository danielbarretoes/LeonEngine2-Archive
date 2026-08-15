#include "asset/AssetManifest.hpp"
#include "asset/AssetPath.hpp"
#include "core/Log.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Leon {

    std::string FAssetManifest::ComputeFileHash(const std::string& InFilePath) {
        std::ifstream file(InFilePath, std::ios::binary);
        if (!file.is_open())
            return "";

        uint64_t hash = 14695981039346656037ull;
        char buffer[8192];
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            std::streamsize bytesRead = file.gcount();
            for (std::streamsize i = 0; i < bytesRead; ++i) {
                hash ^= static_cast<uint8_t>(buffer[i]);
                hash *= 1099511628211ull;
            }
        }

        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << hash;
        return ss.str();
    }

    bool FAssetManifest::NeedsReimport(const std::string& InSourcePath) const {
        std::string normPath = FAssetPath::Normalize(InSourcePath);
        auto it = m_Entries.find(normPath);
        if (it == m_Entries.end()) {
            return true;
        }

        std::string currentHash = ComputeFileHash(InSourcePath);
        if (currentHash.empty() || currentHash != it->second.SourceHash) {
            return true;
        }

        return false;
    }

    void FAssetManifest::RegisterImport(const std::string& InSourcePath,
                                        const std::vector<std::string>& InGeneratedAssets,
                                        const std::vector<std::string>& InDependencies) {
        std::string normPath = FAssetPath::Normalize(InSourcePath);
        FManifestEntry entry;
        entry.SourcePath = normPath;
        entry.SourceHash = ComputeFileHash(InSourcePath);

        std::ifstream file(InSourcePath, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            entry.SourceSize = static_cast<uint64_t>(file.tellg());
        }

        entry.ImportTimestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
                .count());

        entry.GeneratedAssets = InGeneratedAssets;
        entry.Dependencies = InDependencies;

        m_Entries[normPath] = entry;
    }

    void FAssetManifest::RemoveEntry(const std::string& InSourcePath) {
        std::string normPath = FAssetPath::Normalize(InSourcePath);
        m_Entries.erase(normPath);
    }

    bool FAssetManifest::SaveToFile(const std::string& InManifestPath) const {
        std::ofstream file(InManifestPath);
        if (!file.is_open()) {
            LE_CORE_ERROR("FAssetManifest: Failed to open \"{0}\" for writing", InManifestPath);
            return false;
        }

        file << "{\n";
        file << "  \"Version\": 1,\n";
        file << "  \"Entries\": {\n";

        size_t entryIdx = 0;
        for (const auto& [srcPath, entry] : m_Entries) {
            file << "    \"" << srcPath << "\": {\n";
            file << "      \"SourceHash\": \"" << entry.SourceHash << "\",\n";
            file << "      \"SourceSize\": " << entry.SourceSize << ",\n";
            file << "      \"ImportTimestamp\": " << entry.ImportTimestamp << ",\n";

            // GeneratedAssets
            file << "      \"GeneratedAssets\": [\n";
            for (size_t i = 0; i < entry.GeneratedAssets.size(); ++i) {
                file << "        \"" << entry.GeneratedAssets[i] << "\""
                     << (i + 1 < entry.GeneratedAssets.size() ? "," : "") << "\n";
            }
            file << "      ],\n";

            // Dependencies
            file << "      \"Dependencies\": [\n";
            for (size_t i = 0; i < entry.Dependencies.size(); ++i) {
                file << "        \"" << entry.Dependencies[i] << "\"" << (i + 1 < entry.Dependencies.size() ? "," : "")
                     << "\n";
            }
            file << "      ]\n";

            file << "    }" << (++entryIdx < m_Entries.size() ? "," : "") << "\n";
        }

        file << "  }\n";
        file << "}\n";

        return file.good();
    }

    bool FAssetManifest::LoadFromFile(const std::string& InManifestPath) {
        std::ifstream file(InManifestPath);
        if (!file.is_open()) {
            return false;
        }

        m_Entries.clear();

        std::string line;
        std::string currentSource;
        FManifestEntry currentEntry;
        bool inEntry = false;
        bool inGenerated = false;
        bool inDependencies = false;

        while (std::getline(file, line)) {
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                continue;
            std::string trimmed = line.substr(first);

            if (trimmed.find("\"SourceHash\":") != std::string::npos) {
                size_t q1 = trimmed.find('"', trimmed.find(':'));
                size_t q2 = trimmed.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    currentEntry.SourceHash = trimmed.substr(q1 + 1, q2 - q1 - 1);
                }
            } else if (trimmed.find("\"SourceSize\":") != std::string::npos) {
                size_t colon = trimmed.find(':');
                size_t comma = trimmed.find(',', colon);
                std::string val = trimmed.substr(colon + 1, comma - colon - 1);
                currentEntry.SourceSize = std::strtoull(val.c_str(), nullptr, 10);
            } else if (trimmed.find("\"GeneratedAssets\":") != std::string::npos) {
                inGenerated = true;
                inDependencies = false;
            } else if (trimmed.find("\"Dependencies\":") != std::string::npos) {
                inDependencies = true;
                inGenerated = false;
            } else if (trimmed.find(']') != std::string::npos) {
                inGenerated = false;
                inDependencies = false;
            } else if (inGenerated && trimmed.front() == '"') {
                size_t q2 = trimmed.find('"', 1);
                if (q2 != std::string::npos) {
                    currentEntry.GeneratedAssets.push_back(trimmed.substr(1, q2 - 1));
                }
            } else if (inDependencies && trimmed.front() == '"') {
                size_t q2 = trimmed.find('"', 1);
                if (q2 != std::string::npos) {
                    currentEntry.Dependencies.push_back(trimmed.substr(1, q2 - 1));
                }
            } else if (trimmed.front() == '"' && trimmed.find("\": {") != std::string::npos) {
                size_t q2 = trimmed.find('"', 1);
                std::string key = trimmed.substr(1, q2 - 1);
                if (key != "Entries") {
                    if (inEntry && !currentSource.empty()) {
                        m_Entries[currentSource] = currentEntry;
                    }
                    currentSource = key;
                    currentEntry = FManifestEntry();
                    currentEntry.SourcePath = currentSource;
                    inEntry = true;
                }
            }
        }

        if (inEntry && !currentSource.empty()) {
            m_Entries[currentSource] = currentEntry;
        }

        return true;
    }

} // namespace Leon
