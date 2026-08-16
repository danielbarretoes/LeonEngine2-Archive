#include "Assets/FAssetPath.hpp"
#include <algorithm>

namespace Leon {

    std::string FAssetPath::Normalize(const std::string& InPath) {
        if (InPath.empty())
            return "";

        std::string result = InPath;
        std::replace(result.begin(), result.end(), '\\', '/');

        // Remove double slashes
        std::string clean;
        bool lastWasSlash = false;
        for (char c : result) {
            if (c == '/') {
                if (!lastWasSlash) {
                    clean += c;
                    lastWasSlash = true;
                }
            } else {
                clean += c;
                lastWasSlash = false;
            }
        }

        // Trim leading / if relative
        if (clean.length() > 1 && clean[0] == '/' && clean[1] != '/') {
            if (clean.find(':') == std::string::npos) {
                // Relative path with leading slash
                clean = clean.substr(1);
            }
        }

        // Trim trailing slash
        if (clean.length() > 1 && clean.back() == '/') {
            clean.pop_back();
        }

        return clean;
    }

    std::string FAssetPath::MakeVirtualPath(const std::string& InRoot, const std::string& InFullPath) {
        std::string normRoot = Normalize(InRoot);
        std::string normPath = Normalize(InFullPath);

        if (normRoot.empty())
            return normPath;

        if (normPath.find(normRoot) == 0) {
            std::string rel = normPath.substr(normRoot.length());
            if (!rel.empty() && rel.front() == '/')
                rel = rel.substr(1);
            return rel;
        }

        return normPath;
    }

    std::string FAssetPath::GetExtension(const std::string& InPath) {
        std::string norm = Normalize(InPath);
        size_t dotPos = norm.find_last_of('.');
        size_t slashPos = norm.find_last_of('/');
        if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
            std::string ext = norm.substr(dotPos + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext;
        }
        return "";
    }

    std::string FAssetPath::GetFileName(const std::string& InPath) {
        std::string norm = Normalize(InPath);
        size_t slashPos = norm.find_last_of('/');
        if (slashPos != std::string::npos) {
            return norm.substr(slashPos + 1);
        }
        return norm;
    }

    std::string FAssetPath::GetFileNameWithoutExtension(const std::string& InPath) {
        std::string fileName = GetFileName(InPath);
        size_t dotPos = fileName.find_last_of('.');
        if (dotPos != std::string::npos) {
            return fileName.substr(0, dotPos);
        }
        return fileName;
    }

    std::string FAssetPath::GetDirectory(const std::string& InPath) {
        std::string norm = Normalize(InPath);
        size_t slashPos = norm.find_last_of('/');
        if (slashPos != std::string::npos) {
            return norm.substr(0, slashPos);
        }
        return "";
    }

    std::string FAssetPath::Combine(const std::string& InA, const std::string& InB) {
        if (InA.empty())
            return Normalize(InB);
        if (InB.empty())
            return Normalize(InA);

        std::string normA = Normalize(InA);
        std::string normB = Normalize(InB);

        if (normB.front() == '/')
            normB = normB.substr(1);

        return normA + "/" + normB;
    }

    bool FAssetPath::IsAbsolutePath(const std::string& InPath) {
        if (InPath.empty())
            return false;
        // Windows drive letter check (e.g. C:/ or C:\)
        if (InPath.length() >= 2 && InPath[1] == ':')
            return true;
        // Unix absolute path
        if (InPath.front() == '/' || InPath.front() == '\\')
            return true;
        return false;
    }

} // namespace Leon
