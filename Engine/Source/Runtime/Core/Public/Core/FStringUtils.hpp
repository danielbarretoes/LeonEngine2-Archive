#pragma once

#include <sstream>
#include <string>
#include <vector>

namespace Leon {

    /** Shared string helpers for serializers, descriptors, and config parsers. */
    struct FStringUtils {
        static inline std::string Trim(const std::string& InStr) {
            size_t first = InStr.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return "";
            size_t last = InStr.find_last_not_of(" \t\r\n");
            return InStr.substr(first, (last - first + 1));
        }

        static inline std::vector<std::string> Split(const std::string& InStr, char InDelim) {
            std::vector<std::string> parts;
            std::stringstream ss(InStr);
            std::string item;
            while (std::getline(ss, item, InDelim)) {
                parts.push_back(item);
            }
            return parts;
        }
    };

} // namespace Leon
