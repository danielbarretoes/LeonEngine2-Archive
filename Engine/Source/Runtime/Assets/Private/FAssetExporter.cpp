#include "Assets/FAssetExporter.hpp"
#include "Assets/FAssetPath.hpp"
#include "Assets/FHDRImporter.hpp"
#include "Assets/FTextureImporter.hpp"
#include "Assets/UStaticMesh.hpp"
#include "Core/FLog.hpp"

#include <stb_image_write.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace Leon {

    namespace {

        bool EnsureParentDir(const std::string& InPath) {
            const fs::path P(InPath);
            if (!P.has_parent_path())
                return true;
            std::error_code Ec;
            fs::create_directories(P.parent_path(), Ec);
            return !Ec;
        }

        const FTextureMipData* GetLevel0(const FNativeTextureData& InTex) {
            for (const auto& Mip : InTex.Mips) {
                if (Mip.Level == 0)
                    return &Mip;
            }
            return InTex.Mips.empty() ? nullptr : &InTex.Mips.front();
        }

        bool FlattenTextureRGBA8(const FNativeTextureData& InTex, int& OutW, int& OutH, std::vector<uint8_t>& OutRGBA) {
            const FTextureMipData* Mip = GetLevel0(InTex);
            if (!Mip || Mip->Pixels.empty())
                return false;

            OutW = static_cast<int>(Mip->Width);
            OutH = static_cast<int>(Mip->Height);
            const uint32_t SrcCh = InTex.Header.Channels ? InTex.Header.Channels : 4;
            const size_t Expected = static_cast<size_t>(OutW) * static_cast<size_t>(OutH) * SrcCh;
            if (Mip->Pixels.size() < Expected)
                return false;

            OutRGBA.resize(static_cast<size_t>(OutW) * static_cast<size_t>(OutH) * 4);
            for (int I = 0; I < OutW * OutH; ++I) {
                const size_t Si = static_cast<size_t>(I) * SrcCh;
                const size_t Di = static_cast<size_t>(I) * 4;
                const uint8_t R = Mip->Pixels[Si + 0];
                const uint8_t G = SrcCh > 1 ? Mip->Pixels[Si + 1] : R;
                const uint8_t B = SrcCh > 2 ? Mip->Pixels[Si + 2] : R;
                const uint8_t A = SrcCh > 3 ? Mip->Pixels[Si + 3] : 255;
                OutRGBA[Di + 0] = R;
                OutRGBA[Di + 1] = G;
                OutRGBA[Di + 2] = B;
                OutRGBA[Di + 3] = A;
            }
            return true;
        }

        bool FlattenHDRRGB32F(const FNativeHDRData& InHdr, int& OutW, int& OutH, std::vector<float>& OutRGB) {
            if (InHdr.Pixels.empty() || InHdr.Header.Width == 0 || InHdr.Header.Height == 0)
                return false;
            OutW = static_cast<int>(InHdr.Header.Width);
            OutH = static_cast<int>(InHdr.Header.Height);
            const uint32_t Ch = InHdr.Header.Channels ? InHdr.Header.Channels : 4;
            const size_t Expected = static_cast<size_t>(OutW) * static_cast<size_t>(OutH) * Ch;
            if (InHdr.Pixels.size() < Expected)
                return false;

            OutRGB.resize(static_cast<size_t>(OutW) * static_cast<size_t>(OutH) * 3);
            for (int I = 0; I < OutW * OutH; ++I) {
                const size_t Si = static_cast<size_t>(I) * Ch;
                const size_t Di = static_cast<size_t>(I) * 3;
                OutRGB[Di + 0] = InHdr.Pixels[Si + 0];
                OutRGB[Di + 1] = Ch > 1 ? InHdr.Pixels[Si + 1] : InHdr.Pixels[Si];
                OutRGB[Di + 2] = Ch > 2 ? InHdr.Pixels[Si + 2] : InHdr.Pixels[Si];
            }
            return true;
        }

        std::string SanitizeObjName(const std::string& InName) {
            std::string Out;
            Out.reserve(InName.size());
            for (char C : InName) {
                if (std::isalnum(static_cast<unsigned char>(C)) || C == '_' || C == '-')
                    Out.push_back(C);
                else
                    Out.push_back('_');
            }
            return Out.empty() ? "Mesh" : Out;
        }

        void WriteLE32(std::ostream& Out, uint32_t V) {
            const uint8_t B[4] = {static_cast<uint8_t>(V & 0xFF), static_cast<uint8_t>((V >> 8) & 0xFF),
                                  static_cast<uint8_t>((V >> 16) & 0xFF), static_cast<uint8_t>((V >> 24) & 0xFF)};
            Out.write(reinterpret_cast<const char*>(B), 4);
        }

        void WriteLE64(std::ostream& Out, uint64_t V) {
            WriteLE32(Out, static_cast<uint32_t>(V & 0xFFFFFFFFu));
            WriteLE32(Out, static_cast<uint32_t>((V >> 32) & 0xFFFFFFFFu));
        }

        void WriteExrString(std::ostream& Out, const char* S) {
            Out.write(S, static_cast<std::streamsize>(std::strlen(S) + 1));
        }

        void WriteExrAttribute(std::ostream& Out, const char* Name, const char* Type, const void* Data,
                               uint32_t Size) {
            WriteExrString(Out, Name);
            WriteExrString(Out, Type);
            WriteLE32(Out, Size);
            Out.write(reinterpret_cast<const char*>(Data), Size);
        }

    } // namespace

    bool FAssetExporter::ExportStaticMeshToOBJ(const UStaticMesh& InMesh, const std::string& InOutPath) {
        if (!EnsureParentDir(InOutPath)) {
            LE_CORE_ERROR("FAssetExporter: Cannot create directory for '{}'", InOutPath);
            return false;
        }

        std::ofstream Out(InOutPath);
        if (!Out.is_open()) {
            LE_CORE_ERROR("FAssetExporter: Failed to open '{}' for OBJ write", InOutPath);
            return false;
        }

        const auto& Verts = InMesh.GetVertices();
        const auto& Indices = InMesh.GetIndices();
        if (Verts.empty() || Indices.empty()) {
            LE_CORE_ERROR("FAssetExporter: Mesh '{}' has no geometry", InMesh.GetName());
            return false;
        }

        Out << "# LeonEngine2 AssetTool export\n";
        Out << "o " << SanitizeObjName(InMesh.GetName()) << "\n";

        for (const auto& V : Verts)
            Out << "v " << V.Position.x << ' ' << V.Position.y << ' ' << V.Position.z << '\n';
        for (const auto& V : Verts)
            Out << "vt " << V.TexCoord.x << ' ' << V.TexCoord.y << '\n';
        for (const auto& V : Verts)
            Out << "vn " << V.Normal.x << ' ' << V.Normal.y << ' ' << V.Normal.z << '\n';

        for (size_t I = 0; I + 2 < Indices.size(); I += 3) {
            const uint32_t A = Indices[I] + 1;
            const uint32_t B = Indices[I + 1] + 1;
            const uint32_t C = Indices[I + 2] + 1;
            Out << "f " << A << '/' << A << '/' << A << ' ' << B << '/' << B << '/' << B << ' ' << C << '/' << C << '/'
                << C << '\n';
        }

        LE_CORE_INFO("FAssetExporter: Wrote OBJ '{}' ({} verts, {} tris)", InOutPath, Verts.size(),
                     Indices.size() / 3);
        return Out.good();
    }

    bool FAssetExporter::ExportStaticMeshToFBX(const UStaticMesh& InMesh, const std::string& InOutPath) {
        if (!EnsureParentDir(InOutPath)) {
            LE_CORE_ERROR("FAssetExporter: Cannot create directory for '{}'", InOutPath);
            return false;
        }

        const auto& Verts = InMesh.GetVertices();
        const auto& Indices = InMesh.GetIndices();
        if (Verts.empty() || Indices.empty()) {
            LE_CORE_ERROR("FAssetExporter: Mesh '{}' has no geometry", InMesh.GetName());
            return false;
        }

        std::ofstream Out(InOutPath);
        if (!Out.is_open()) {
            LE_CORE_ERROR("FAssetExporter: Failed to open '{}' for FBX write", InOutPath);
            return false;
        }

        const std::string MeshName = SanitizeObjName(InMesh.GetName());
        const int32_t VertCount = static_cast<int32_t>(Verts.size());
        const int32_t IndexCount = static_cast<int32_t>(Indices.size());
        const int64_t GeoId = 100000;
        const int64_t ModelId = 100001;

        Out << "; FBX 7.4.0 project file\n";
        Out << "; Created by LeonEngine2 AssetTool export\n";
        Out << "FBXHeaderExtension:  {\n";
        Out << "    FBXHeaderVersion: 1003\n";
        Out << "    FBXVersion: 7400\n";
        Out << "}\n";
        Out << "Definitions:  {\n";
        Out << "    Version: 100\n";
        Out << "    Count: 2\n";
        Out << "    ObjectType: \"Geometry\" {\n";
        Out << "        Count: 1\n";
        Out << "    }\n";
        Out << "    ObjectType: \"Model\" {\n";
        Out << "        Count: 1\n";
        Out << "    }\n";
        Out << "}\n";
        Out << "Objects:  {\n";
        Out << "    Geometry: " << GeoId << ", \"Geometry::" << MeshName << "\", \"Mesh\" {\n";
        Out << "        Vertices: *" << (VertCount * 3) << " {\n            a: ";
        for (int32_t I = 0; I < VertCount; ++I) {
            if (I)
                Out << ',';
            Out << Verts[I].Position.x << ',' << Verts[I].Position.y << ',' << Verts[I].Position.z;
        }
        Out << "\n        }\n";
        Out << "        PolygonVertexIndex: *" << IndexCount << " {\n            a: ";
        for (int32_t I = 0; I < IndexCount; ++I) {
            if (I)
                Out << ',';
            if ((I % 3) == 2)
                Out << (~static_cast<int32_t>(Indices[I]));
            else
                Out << static_cast<int32_t>(Indices[I]);
        }
        Out << "\n        }\n";
        Out << "        LayerElementNormal: 0 {\n";
        Out << "            Version: 101\n";
        Out << "            Name: \"\"\n";
        Out << "            MappingInformationType: \"ByPolygonVertex\"\n";
        Out << "            ReferenceInformationType: \"Direct\"\n";
        Out << "            Normals: *" << (IndexCount * 3) << " {\n                a: ";
        for (int32_t I = 0; I < IndexCount; ++I) {
            if (I)
                Out << ',';
            const auto& N = Verts[Indices[I]].Normal;
            Out << N.x << ',' << N.y << ',' << N.z;
        }
        Out << "\n            }\n";
        Out << "        }\n";
        Out << "        LayerElementUV: 0 {\n";
        Out << "            Version: 101\n";
        Out << "            Name: \"UVMap\"\n";
        Out << "            MappingInformationType: \"ByPolygonVertex\"\n";
        Out << "            ReferenceInformationType: \"Direct\"\n";
        Out << "            UV: *" << (IndexCount * 2) << " {\n                a: ";
        for (int32_t I = 0; I < IndexCount; ++I) {
            if (I)
                Out << ',';
            const auto& UV = Verts[Indices[I]].TexCoord;
            Out << UV.x << ',' << UV.y;
        }
        Out << "\n            }\n";
        Out << "        }\n";
        Out << "        Layer: 0 {\n";
        Out << "            Version: 100\n";
        Out << "            LayerElement:  {\n";
        Out << "                Type: \"LayerElementNormal\"\n";
        Out << "                TypedIndex: 0\n";
        Out << "            }\n";
        Out << "            LayerElement:  {\n";
        Out << "                Type: \"LayerElementUV\"\n";
        Out << "                TypedIndex: 0\n";
        Out << "            }\n";
        Out << "        }\n";
        Out << "    }\n";
        Out << "    Model: " << ModelId << ", \"Model::" << MeshName << "\", \"Mesh\" {\n";
        Out << "        Version: 232\n";
        Out << "        Properties70:  {\n";
        Out << "            P: \"Inheritance\", \"enum\", \"\", \"\",1\n";
        Out << "        }\n";
        Out << "        Shading: Y\n";
        Out << "        Culling: \"CullingOff\"\n";
        Out << "    }\n";
        Out << "}\n";
        Out << "Connections:  {\n";
        Out << "    C: \"OO\"," << GeoId << "," << ModelId << "\n";
        Out << "    C: \"OO\"," << ModelId << ",0\n";
        Out << "}\n";

        LE_CORE_INFO("FAssetExporter: Wrote ASCII FBX '{}' ({} verts, {} tris)", InOutPath, Verts.size(),
                     Indices.size() / 3);
        return Out.good();
    }

    bool FAssetExporter::ExportTextureToPNG(const FNativeTextureData& InTex, const std::string& InOutPath) {
        int W = 0, H = 0;
        std::vector<uint8_t> RGBA;
        if (!FlattenTextureRGBA8(InTex, W, H, RGBA) || !EnsureParentDir(InOutPath)) {
            LE_CORE_ERROR("FAssetExporter: Invalid texture for PNG export");
            return false;
        }
        if (!stbi_write_png(InOutPath.c_str(), W, H, 4, RGBA.data(), W * 4)) {
            LE_CORE_ERROR("FAssetExporter: stbi_write_png failed for '{}'", InOutPath);
            return false;
        }
        LE_CORE_INFO("FAssetExporter: Wrote PNG '{}' ({}x{})", InOutPath, W, H);
        return true;
    }

    bool FAssetExporter::ExportTextureToTGA(const FNativeTextureData& InTex, const std::string& InOutPath) {
        int W = 0, H = 0;
        std::vector<uint8_t> RGBA;
        if (!FlattenTextureRGBA8(InTex, W, H, RGBA) || !EnsureParentDir(InOutPath)) {
            LE_CORE_ERROR("FAssetExporter: Invalid texture for TGA export");
            return false;
        }
        if (!stbi_write_tga(InOutPath.c_str(), W, H, 4, RGBA.data())) {
            LE_CORE_ERROR("FAssetExporter: stbi_write_tga failed for '{}'", InOutPath);
            return false;
        }
        LE_CORE_INFO("FAssetExporter: Wrote TGA '{}' ({}x{})", InOutPath, W, H);
        return true;
    }

    bool FAssetExporter::ExportTextureToJPG(const FNativeTextureData& InTex, const std::string& InOutPath,
                                            int InQuality) {
        int W = 0, H = 0;
        std::vector<uint8_t> RGBA;
        if (!FlattenTextureRGBA8(InTex, W, H, RGBA) || !EnsureParentDir(InOutPath)) {
            LE_CORE_ERROR("FAssetExporter: Invalid texture for JPG export");
            return false;
        }
        const int Q = std::clamp(InQuality, 1, 100);
        if (!stbi_write_jpg(InOutPath.c_str(), W, H, 4, RGBA.data(), Q)) {
            LE_CORE_ERROR("FAssetExporter: stbi_write_jpg failed for '{}'", InOutPath);
            return false;
        }
        LE_CORE_INFO("FAssetExporter: Wrote JPG '{}' ({}x{}, q={})", InOutPath, W, H, Q);
        return true;
    }

    bool FAssetExporter::ExportTextureToBMP(const FNativeTextureData& InTex, const std::string& InOutPath) {
        int W = 0, H = 0;
        std::vector<uint8_t> RGBA;
        if (!FlattenTextureRGBA8(InTex, W, H, RGBA) || !EnsureParentDir(InOutPath)) {
            LE_CORE_ERROR("FAssetExporter: Invalid texture for BMP export");
            return false;
        }
        if (!stbi_write_bmp(InOutPath.c_str(), W, H, 4, RGBA.data())) {
            LE_CORE_ERROR("FAssetExporter: stbi_write_bmp failed for '{}'", InOutPath);
            return false;
        }
        LE_CORE_INFO("FAssetExporter: Wrote BMP '{}' ({}x{})", InOutPath, W, H);
        return true;
    }

    bool FAssetExporter::ExportHDRToRadiance(const FNativeHDRData& InHdr, const std::string& InOutPath) {
        int W = 0, H = 0;
        std::vector<float> RGB;
        if (!FlattenHDRRGB32F(InHdr, W, H, RGB) || !EnsureParentDir(InOutPath)) {
            LE_CORE_ERROR("FAssetExporter: Invalid HDR for Radiance export");
            return false;
        }
        if (!stbi_write_hdr(InOutPath.c_str(), W, H, 3, RGB.data())) {
            LE_CORE_ERROR("FAssetExporter: stbi_write_hdr failed for '{}'", InOutPath);
            return false;
        }
        LE_CORE_INFO("FAssetExporter: Wrote Radiance HDR '{}' ({}x{})", InOutPath, W, H);
        return true;
    }

    bool FAssetExporter::ExportHDRToEXR(const FNativeHDRData& InHdr, const std::string& InOutPath) {
        // Minimal uncompressed OpenEXR (RGBA FLOAT, scanline, NO_COMPRESSION).
        if (InHdr.Pixels.empty() || InHdr.Header.Width == 0 || InHdr.Header.Height == 0) {
            LE_CORE_ERROR("FAssetExporter: Invalid HDR for EXR export");
            return false;
        }
        if (!EnsureParentDir(InOutPath))
            return false;

        const int32_t W = static_cast<int32_t>(InHdr.Header.Width);
        const int32_t H = static_cast<int32_t>(InHdr.Header.Height);
        const uint32_t Ch = InHdr.Header.Channels ? InHdr.Header.Channels : 4;
        const size_t Expected = static_cast<size_t>(W) * static_cast<size_t>(H) * Ch;
        if (InHdr.Pixels.size() < Expected) {
            LE_CORE_ERROR("FAssetExporter: HDR payload truncated");
            return false;
        }

        std::ofstream Out(InOutPath, std::ios::binary);
        if (!Out.is_open())
            return false;

        WriteLE32(Out, 20000630u); // OpenEXR magic
        WriteLE32(Out, 2u);        // version: single-part scanline

        auto Attr = [&](const char* Name, const char* Type, const void* Data, uint32_t Size) {
            WriteExrAttribute(Out, Name, Type, Data, Size);
        };

        {
            // Channel names must be stored alphabetically: A, B, G, R
            std::string Blob;
            auto AppendCh = [&](const char* Name) {
                Blob.append(Name, std::strlen(Name) + 1);
                const uint32_t PixelType = 2; // FLOAT
                for (int S = 0; S < 4; ++S)
                    Blob.push_back(static_cast<char>((PixelType >> (8 * S)) & 0xFF));
                Blob.push_back(1); // pLinear
                Blob.push_back(0);
                Blob.push_back(0);
                Blob.push_back(0);
                const uint32_t One = 1;
                for (int K = 0; K < 2; ++K) {
                    for (int S = 0; S < 4; ++S)
                        Blob.push_back(static_cast<char>((One >> (8 * S)) & 0xFF));
                }
            };
            AppendCh("A");
            AppendCh("B");
            AppendCh("G");
            AppendCh("R");
            Blob.push_back(0);
            Attr("channels", "chlist", Blob.data(), static_cast<uint32_t>(Blob.size()));
        }

        {
            const uint8_t Compression = 0; // NO_COMPRESSION
            Attr("compression", "compression", &Compression, 1);
        }
        {
            int32_t Box[4] = {0, 0, W - 1, H - 1};
            Attr("dataWindow", "box2i", Box, 16);
            Attr("displayWindow", "box2i", Box, 16);
        }
        {
            const float Aspect = 1.0f;
            Attr("pixelAspectRatio", "float", &Aspect, 4);
        }
        {
            const uint8_t LineOrder = 0; // INCREASING_Y
            Attr("lineOrder", "lineOrder", &LineOrder, 1);
        }
        {
            float Center[2] = {0.0f, 0.0f};
            Attr("screenWindowCenter", "v2f", Center, 8);
            const float Width = 1.0f;
            Attr("screenWindowWidth", "float", &Width, 4);
        }
        Out.put(0); // end of header

        const std::streampos OffsetTablePos = Out.tellp();
        std::vector<uint64_t> Offsets(static_cast<size_t>(H), 0);
        for (int32_t Y = 0; Y < H; ++Y)
            WriteLE64(Out, 0);

        const uint32_t RowBytes = static_cast<uint32_t>(W) * 4u * 4u;
        std::vector<float> Plane(static_cast<size_t>(W));
        for (int32_t Y = 0; Y < H; ++Y) {
            Offsets[static_cast<size_t>(Y)] = static_cast<uint64_t>(Out.tellp());
            WriteLE32(Out, static_cast<uint32_t>(Y));
            WriteLE32(Out, RowBytes);

            auto WritePlane = [&](int ChannelIndex) {
                for (int32_t X = 0; X < W; ++X) {
                    const size_t Si =
                        (static_cast<size_t>(Y) * static_cast<size_t>(W) + static_cast<size_t>(X)) * Ch;
                    float V = 0.0f;
                    if (ChannelIndex == 0) // A
                        V = Ch > 3 ? InHdr.Pixels[Si + 3] : 1.0f;
                    else if (ChannelIndex == 1) // B
                        V = Ch > 2 ? InHdr.Pixels[Si + 2] : InHdr.Pixels[Si];
                    else if (ChannelIndex == 2) // G
                        V = Ch > 1 ? InHdr.Pixels[Si + 1] : InHdr.Pixels[Si];
                    else // R
                        V = InHdr.Pixels[Si + 0];
                    Plane[static_cast<size_t>(X)] = V;
                }
                Out.write(reinterpret_cast<const char*>(Plane.data()),
                          static_cast<std::streamsize>(Plane.size() * sizeof(float)));
            };
            WritePlane(0);
            WritePlane(1);
            WritePlane(2);
            WritePlane(3);
        }

        const std::streampos EndPos = Out.tellp();
        Out.seekp(OffsetTablePos);
        for (int32_t Y = 0; Y < H; ++Y)
            WriteLE64(Out, Offsets[static_cast<size_t>(Y)]);
        Out.seekp(EndPos);

        LE_CORE_INFO("FAssetExporter: Wrote OpenEXR '{}' ({}x{}, RGBA32F)", InOutPath, W, H);
        return Out.good();
    }

    bool FAssetExporter::ExportNativeFile(const std::string& InNativePath, const std::string& InOutPath) {
        const std::string NativeExt = FAssetPath::GetExtension(InNativePath);
        const std::string OutExt = FAssetPath::GetExtension(InOutPath);

        if (NativeExt == "lmesh") {
            UStaticMesh Mesh;
            if (!Mesh.LoadFromFile(InNativePath))
                return false;
            if (OutExt == "fbx")
                return ExportStaticMeshToFBX(Mesh, InOutPath);
            return ExportStaticMeshToOBJ(Mesh, InOutPath);
        }
        if (NativeExt == "ltex") {
            FNativeTextureData Tex;
            if (!Tex.LoadFromFile(InNativePath))
                return false;
            if (OutExt == "jpg" || OutExt == "jpeg")
                return ExportTextureToJPG(Tex, InOutPath);
            if (OutExt == "bmp")
                return ExportTextureToBMP(Tex, InOutPath);
            if (OutExt == "tga")
                return ExportTextureToTGA(Tex, InOutPath);
            return ExportTextureToPNG(Tex, InOutPath);
        }
        if (NativeExt == "lhdr") {
            FNativeHDRData Hdr;
            if (!Hdr.LoadFromFile(InNativePath))
                return false;
            if (OutExt == "exr")
                return ExportHDRToEXR(Hdr, InOutPath);
            return ExportHDRToRadiance(Hdr, InOutPath);
        }

        LE_CORE_ERROR("FAssetExporter: Unsupported native type '.{}'", NativeExt);
        return false;
    }

} // namespace Leon
