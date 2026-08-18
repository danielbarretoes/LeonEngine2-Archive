#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Leon {

    /**
     * @brief Tiny binary helpers for snapshot / input blobs. Game-agnostic.
     */
    struct FNetBlob {
        static void WriteU8(std::vector<uint8_t>& Out, uint8_t InValue) { Out.push_back(InValue); }

        static void WriteU16(std::vector<uint8_t>& Out, uint16_t InValue) {
            Out.push_back(static_cast<uint8_t>(InValue));
            Out.push_back(static_cast<uint8_t>(InValue >> 8));
        }

        static void WriteU32(std::vector<uint8_t>& Out, uint32_t InValue) {
            Out.push_back(static_cast<uint8_t>(InValue));
            Out.push_back(static_cast<uint8_t>(InValue >> 8));
            Out.push_back(static_cast<uint8_t>(InValue >> 16));
            Out.push_back(static_cast<uint8_t>(InValue >> 24));
        }

        static void WriteI32(std::vector<uint8_t>& Out, int32_t InValue) {
            WriteU32(Out, static_cast<uint32_t>(InValue));
        }

        static void WriteF32(std::vector<uint8_t>& Out, float InValue) {
            uint32_t bits = 0;
            std::memcpy(&bits, &InValue, sizeof(bits));
            WriteU32(Out, bits);
        }

        static void WriteU64(std::vector<uint8_t>& Out, uint64_t InValue) {
            WriteU32(Out, static_cast<uint32_t>(InValue));
            WriteU32(Out, static_cast<uint32_t>(InValue >> 32));
        }

        static void WriteString(std::vector<uint8_t>& Out, const std::string& InStr) {
            WriteU32(Out, static_cast<uint32_t>(InStr.size()));
            Out.insert(Out.end(), InStr.begin(), InStr.end());
        }

        static void WriteBlob(std::vector<uint8_t>& Out, const std::vector<uint8_t>& InBlob) {
            WriteU32(Out, static_cast<uint32_t>(InBlob.size()));
            Out.insert(Out.end(), InBlob.begin(), InBlob.end());
        }

        static bool ReadU8(const std::vector<uint8_t>& In, size_t& Offset, uint8_t& OutValue) {
            if (Offset >= In.size())
                return false;
            OutValue = In[Offset++];
            return true;
        }

        static bool ReadU16(const std::vector<uint8_t>& In, size_t& Offset, uint16_t& OutValue) {
            if (Offset + 2 > In.size())
                return false;
            OutValue = static_cast<uint16_t>(In[Offset]) | (static_cast<uint16_t>(In[Offset + 1]) << 8);
            Offset += 2;
            return true;
        }

        static bool ReadU32(const std::vector<uint8_t>& In, size_t& Offset, uint32_t& OutValue) {
            if (Offset + 4 > In.size())
                return false;
            OutValue = static_cast<uint32_t>(In[Offset]) | (static_cast<uint32_t>(In[Offset + 1]) << 8) |
                       (static_cast<uint32_t>(In[Offset + 2]) << 16) | (static_cast<uint32_t>(In[Offset + 3]) << 24);
            Offset += 4;
            return true;
        }

        static bool ReadI32(const std::vector<uint8_t>& In, size_t& Offset, int32_t& OutValue) {
            uint32_t raw = 0;
            if (!ReadU32(In, Offset, raw))
                return false;
            OutValue = static_cast<int32_t>(raw);
            return true;
        }

        static bool ReadF32(const std::vector<uint8_t>& In, size_t& Offset, float& OutValue) {
            uint32_t bits = 0;
            if (!ReadU32(In, Offset, bits))
                return false;
            std::memcpy(&OutValue, &bits, sizeof(OutValue));
            return true;
        }

        static bool ReadU64(const std::vector<uint8_t>& In, size_t& Offset, uint64_t& OutValue) {
            uint32_t lo = 0, hi = 0;
            if (!ReadU32(In, Offset, lo) || !ReadU32(In, Offset, hi))
                return false;
            OutValue = static_cast<uint64_t>(lo) | (static_cast<uint64_t>(hi) << 32);
            return true;
        }

        static bool ReadString(const std::vector<uint8_t>& In, size_t& Offset, std::string& OutStr) {
            uint32_t len = 0;
            if (!ReadU32(In, Offset, len) || Offset + len > In.size())
                return false;
            OutStr.assign(reinterpret_cast<const char*>(In.data() + Offset), len);
            Offset += len;
            return true;
        }

        static bool ReadBlob(const std::vector<uint8_t>& In, size_t& Offset, std::vector<uint8_t>& OutBlob) {
            uint32_t len = 0;
            if (!ReadU32(In, Offset, len) || Offset + len > In.size())
                return false;
            OutBlob.assign(In.begin() + static_cast<std::ptrdiff_t>(Offset),
                           In.begin() + static_cast<std::ptrdiff_t>(Offset + len));
            Offset += len;
            return true;
        }
    };

} // namespace Leon
