#include "Core/FProjectDescriptor.hpp"
#include "Core/FLog.hpp"
#include "Core/FStringUtils.hpp"

#include <fstream>
#include <sstream>

namespace Leon {

    static std::string StripQuotes(const std::string& str) {
        std::string s = FStringUtils::Trim(str);
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }

    bool FProjectDescriptor::Load(const std::string& InFilePath) {
        std::ifstream fin(InFilePath);
        if (!fin.is_open()) {
            LE_CORE_ERROR("FProjectDescriptor: Failed to open project file '{0}'", InFilePath);
            return false;
        }

        std::stringstream ss;
        ss << fin.rdbuf();
        fin.close();

        return DeserializeJson(ss.str());
    }

    bool FProjectDescriptor::Save(const std::string& InFilePath) const {
        std::ofstream fout(InFilePath);
        if (!fout.is_open()) {
            LE_CORE_ERROR("FProjectDescriptor: Failed to open project file for writing: '{0}'", InFilePath);
            return false;
        }

        fout << SerializeJson();
        fout.close();
        return true;
    }

    bool FProjectDescriptor::DeserializeJson(const std::string& InJsonString) {
        std::stringstream ss(InJsonString);
        std::string line;

        while (std::getline(ss, line)) {
            std::string trimmed = FStringUtils::Trim(line);
            if (trimmed.empty() || trimmed[0] == '{' || trimmed[0] == '}' || trimmed[0] == '#') {
                continue;
            }

            size_t colon = trimmed.find(':');
            if (colon != std::string::npos) {
                std::string key = StripQuotes(trimmed.substr(0, colon));
                std::string val = FStringUtils::Trim(trimmed.substr(colon + 1));
                if (!val.empty() && val.back() == ',') {
                    val.pop_back();
                }
                val = StripQuotes(val);

                if (key == "FileVersion") {
                    try {
                        FileVersion = static_cast<uint32_t>(std::stoul(val));
                    } catch (...) {
                    }
                } else if (key == "EngineVersion") {
                    EngineVersion = val;
                } else if (key == "ProjectName") {
                    ProjectName = val;
                } else if (key == "DefaultMap") {
                    DefaultMap = val;
                } else if (key == "DefaultGameMode") {
                    DefaultGameMode = val;
                }
            }
        }

        return true;
    }

    std::string FProjectDescriptor::SerializeJson() const {
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"FileVersion\": " << FileVersion << ",\n";
        ss << "  \"EngineVersion\": \"" << EngineVersion << "\",\n";
        ss << "  \"ProjectName\": \"" << ProjectName << "\",\n";
        ss << "  \"DefaultMap\": \"" << DefaultMap << "\",\n";
        ss << "  \"DefaultGameMode\": \"" << DefaultGameMode << "\"\n";
        ss << "}\n";
        return ss.str();
    }

} // namespace Leon
