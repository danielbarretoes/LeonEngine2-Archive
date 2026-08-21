#include "Editor/Play/FPlaySettings.hpp"
#include "Core/FLog.hpp"
#include "Core/FStringUtils.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Leon::Editor {

    void FPlaySettings::Clamp() {
        NumberOfPlayers = std::clamp(NumberOfPlayers, 1, 4);
        if (ListenPort <= 0)
            ListenPort = 7777;
    }

    bool FPlaySettings::LoadFromFile(const std::string& InPath) {
        std::ifstream Fin(InPath);
        if (!Fin.is_open())
            return false;

        std::string Line;
        while (std::getline(Fin, Line)) {
            const std::string Trimmed = FStringUtils::Trim(Line);
            if (Trimmed.empty() || Trimmed[0] == '#' || Trimmed[0] == '{' || Trimmed[0] == '}')
                continue;
            const size_t Colon = Trimmed.find(':');
            if (Colon == std::string::npos)
                continue;
            std::string Key = FStringUtils::Trim(Trimmed.substr(0, Colon));
            if (!Key.empty() && Key.front() == '"')
                Key = Key.substr(1, Key.size() - 2);
            std::string Val = FStringUtils::Trim(Trimmed.substr(Colon + 1));
            if (!Val.empty() && Val.back() == ',')
                Val.pop_back();
            Val = FStringUtils::Trim(Val);
            if (!Val.empty() && Val.front() == '"')
                Val = Val.substr(1, Val.size() >= 2 ? Val.size() - 2 : 0);

            if (Key == "NetMode")
                NetMode = static_cast<EPlayNetMode>(std::atoi(Val.c_str()));
            else if (Key == "PlayMode")
                PlayMode = static_cast<EPlayMode>(std::atoi(Val.c_str()));
            else if (Key == "NumberOfPlayers")
                NumberOfPlayers = std::atoi(Val.c_str());
            else if (Key == "ListenPort")
                ListenPort = std::atoi(Val.c_str());
            else if (Key == "ClientAddress")
                ClientAddress = Val;
            else if (Key == "AutoSaveMapBeforePlay")
                bAutoSaveMapBeforePlay = (Val == "true" || Val == "1");
        }
        Clamp();
        return true;
    }

    bool FPlaySettings::SaveToFile(const std::string& InPath) const {
        std::ofstream Fout(InPath);
        if (!Fout.is_open()) {
            LE_CORE_WARN("FPlaySettings: Failed to write '{}'", InPath);
            return false;
        }
        Fout << "{\n";
        Fout << "  \"NetMode\": " << static_cast<int>(NetMode) << ",\n";
        Fout << "  \"PlayMode\": " << static_cast<int>(PlayMode) << ",\n";
        Fout << "  \"NumberOfPlayers\": " << NumberOfPlayers << ",\n";
        Fout << "  \"ListenPort\": " << ListenPort << ",\n";
        Fout << "  \"ClientAddress\": \"" << ClientAddress << "\",\n";
        Fout << "  \"AutoSaveMapBeforePlay\": " << (bAutoSaveMapBeforePlay ? "true" : "false") << "\n";
        Fout << "}\n";
        return true;
    }

} // namespace Leon::Editor
