#include "Core/FConfigFile.hpp"
#include "Core/FLog.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Leon {

    FConfigFile::FConfigFile(const std::string& InFilePath) {
        Load(InFilePath);
    }

    std::string FConfigFile::Trim(const std::string& InStr) {
        size_t first = InStr.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return "";
        size_t last = InStr.find_last_not_of(" \t\r\n");
        return InStr.substr(first, (last - first + 1));
    }

    bool FConfigFile::Load(const std::string& InFilePath) {
        FilePath = InFilePath;
        Data.clear();

        std::ifstream file(InFilePath);
        if (!file.is_open()) {
            LE_CORE_WARN("FConfigFile: Could not open file '{0}'", InFilePath);
            return false;
        }

        std::string currentSection = "General";
        std::string line;

        while (std::getline(file, line)) {
            line = Trim(line);

            // Skip empty lines and comments
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }

            // Section header: [SectionName]
            if (line.front() == '[' && line.back() == ']') {
                currentSection = Trim(line.substr(1, line.length() - 2));
                continue;
            }

            // Key-Value pair: Key = Value
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = Trim(line.substr(0, eqPos));
                std::string value = Trim(line.substr(eqPos + 1));

                // Strip quotes if present
                if (value.length() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                            (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.length() - 2);
                }

                Data[currentSection][key] = value;
            }
        }

        LE_CORE_INFO("FConfigFile: Successfully loaded '{0}' ({1} sections)", InFilePath, Data.size());
        return true;
    }

    bool FConfigFile::Save(const std::string& InFilePath) {
        std::string path = InFilePath.empty() ? FilePath : InFilePath;
        if (path.empty()) {
            LE_CORE_ERROR("FConfigFile: Save path is empty!");
            return false;
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            LE_CORE_ERROR("FConfigFile: Could not write to '{0}'", path);
            return false;
        }

        for (const auto& [section, keys] : Data) {
            file << "[" << section << "]\n";
            for (const auto& [key, value] : keys) {
                file << key << "=" << value << "\n";
            }
            file << "\n";
        }

        return true;
    }

    bool FConfigFile::HasSection(const std::string& InSection) const {
        return Data.find(InSection) != Data.end();
    }

    bool FConfigFile::HasKey(const std::string& InSection, const std::string& InKey) const {
        auto secIt = Data.find(InSection);
        if (secIt != Data.end()) {
            return secIt->second.find(InKey) != secIt->second.end();
        }
        return false;
    }

    std::string FConfigFile::GetString(const std::string& InSection, const std::string& InKey,
                                       const std::string& InDefault) const {
        auto secIt = Data.find(InSection);
        if (secIt != Data.end()) {
            auto keyIt = secIt->second.find(InKey);
            if (keyIt != secIt->second.end()) {
                return keyIt->second;
            }
        }
        return InDefault;
    }

    int FConfigFile::GetInt(const std::string& InSection, const std::string& InKey, int InDefault) const {
        std::string val = GetString(InSection, InKey, "");
        if (val.empty())
            return InDefault;
        try {
            return std::stoi(val);
        } catch (...) {
            return InDefault;
        }
    }

    float FConfigFile::GetFloat(const std::string& InSection, const std::string& InKey, float InDefault) const {
        std::string val = GetString(InSection, InKey, "");
        if (val.empty())
            return InDefault;
        try {
            return std::stof(val);
        } catch (...) {
            return InDefault;
        }
    }

    bool FConfigFile::GetBool(const std::string& InSection, const std::string& InKey, bool InDefault) const {
        std::string val = GetString(InSection, InKey, "");
        if (val.empty())
            return InDefault;

        std::string lowerVal = val;
        std::transform(lowerVal.begin(), lowerVal.end(), lowerVal.begin(), ::tolower);

        if (lowerVal == "1" || lowerVal == "true" || lowerVal == "yes" || lowerVal == "on")
            return true;
        if (lowerVal == "0" || lowerVal == "false" || lowerVal == "no" || lowerVal == "off")
            return false;

        return InDefault;
    }

    void FConfigFile::SetString(const std::string& InSection, const std::string& InKey, const std::string& InValue) {
        Data[InSection][InKey] = InValue;
    }

    void FConfigFile::SetInt(const std::string& InSection, const std::string& InKey, int InValue) {
        Data[InSection][InKey] = std::to_string(InValue);
    }

    void FConfigFile::SetFloat(const std::string& InSection, const std::string& InKey, float InValue) {
        Data[InSection][InKey] = std::to_string(InValue);
    }

    void FConfigFile::SetBool(const std::string& InSection, const std::string& InKey, bool InValue) {
        Data[InSection][InKey] = InValue ? "True" : "False";
    }

} // namespace Leon
