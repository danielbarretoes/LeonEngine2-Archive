#pragma once

#include <string>

namespace Leon {

    class FAssetPath {
    public:
        /** Normalize path to use forward slashes and remove redundant slashes or relative segments */
        static std::string Normalize(const std::string& InPath);

        /** Extract virtual path relative to a project Content or Assets root */
        static std::string MakeVirtualPath(const std::string& InRoot, const std::string& InFullPath);

        /** Get file extension without dot in lowercase (e.g. "lmesh", "ltex") */
        static std::string GetExtension(const std::string& InPath);

        /** Get filename with extension (e.g. "Corals.lmesh") */
        static std::string GetFileName(const std::string& InPath);

        /** Get filename without extension (e.g. "Corals") */
        static std::string GetFileNameWithoutExtension(const std::string& InPath);

        /** Get parent directory path */
        static std::string GetDirectory(const std::string& InPath);

        /** Combine two path segments using standard forward slash */
        static std::string Combine(const std::string& InA, const std::string& InB);

        /** Check if path contains absolute drive or root markers */
        static bool IsAbsolutePath(const std::string& InPath);
    };

} // namespace Leon
