#pragma once

#include "Core/Base.hpp"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace Leon {

    /**
     * @brief Unreal Engine-inspired INI configuration file reader and writer.
     * Supports standard sections [SectionName] and Key=Value properties.
     */
    class FConfigFile {
    public:
        FConfigFile() = default;
        explicit FConfigFile(const std::string& InFilePath);

        bool Load(const std::string& InFilePath);
        bool Save(const std::string& InFilePath = "");

        bool HasSection(const std::string& InSection) const;
        bool HasKey(const std::string& InSection, const std::string& InKey) const;

        std::string GetString(const std::string& InSection, const std::string& InKey,
                              const std::string& InDefault = "") const;
        int GetInt(const std::string& InSection, const std::string& InKey, int InDefault = 0) const;
        float GetFloat(const std::string& InSection, const std::string& InKey, float InDefault = 0.0f) const;
        bool GetBool(const std::string& InSection, const std::string& InKey, bool InDefault = false) const;

        void SetString(const std::string& InSection, const std::string& InKey, const std::string& InValue);
        void SetInt(const std::string& InSection, const std::string& InKey, int InValue);
        void SetFloat(const std::string& InSection, const std::string& InKey, float InValue);
        void SetBool(const std::string& InSection, const std::string& InKey, bool InValue);

        const std::string& GetFilePath() const { return FilePath; }

    private:
        static std::string Trim(const std::string& InStr);

    private:
        std::string FilePath;
        // Map from Section -> (Key -> Value)
        std::map<std::string, std::map<std::string, std::string>> Data;
    };

} // namespace Leon
