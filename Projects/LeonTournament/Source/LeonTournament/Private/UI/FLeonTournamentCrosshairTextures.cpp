#include "FLeonTournamentCrosshairTextures.hpp"
#include "UMG/UImage.hpp"
#include "Assets/UAssetManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Leon {

    namespace {
        constexpr int kCrosshairDim = 64;

        struct FPixelWriter {
            std::vector<unsigned char> rgba;

            explicit FPixelWriter(int InDim) : rgba(static_cast<size_t>(InDim) * InDim * 4, 0) {}

            void Set(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
                if (x < 0 || y < 0 || x >= kCrosshairDim || y >= kCrosshairDim)
                    return;
                const size_t i = (static_cast<size_t>(y) * kCrosshairDim + static_cast<size_t>(x)) * 4;
                rgba[i + 0] = r;
                rgba[i + 1] = g;
                rgba[i + 2] = b;
                rgba[i + 3] = a;
            }

            void FillRect(int x0, int y0, int x1, int y1, unsigned char r, unsigned char g, unsigned char b,
                          unsigned char a = 255) {
                for (int y = y0; y <= y1; ++y)
                    for (int x = x0; x <= x1; ++x)
                        Set(x, y, r, g, b, a);
            }

            void LineH(int y, int x0, int x1, int thick, unsigned char r, unsigned char g, unsigned char b,
                       unsigned char a = 255) {
                FillRect(x0, y - thick / 2, x1, y + thick / 2, r, g, b, a);
            }

            void LineV(int x, int y0, int y1, int thick, unsigned char r, unsigned char g, unsigned char b,
                       unsigned char a = 255) {
                FillRect(x - thick / 2, y0, x + thick / 2, y1, r, g, b, a);
            }

            void CircleOutline(int cx, int cy, int radius, int thick, unsigned char r, unsigned char g,
                               unsigned char b, unsigned char a = 255) {
                for (int y = 0; y < kCrosshairDim; ++y) {
                    for (int x = 0; x < kCrosshairDim; ++x) {
                        const float dx = static_cast<float>(x - cx);
                        const float dy = static_cast<float>(y - cy);
                        const float dist = std::sqrt(dx * dx + dy * dy);
                        if (std::abs(dist - static_cast<float>(radius)) <= static_cast<float>(thick) * 0.5f + 0.5f)
                            Set(x, y, r, g, b, a);
                    }
                }
            }

            TRef<FTexture2D> Bake(const std::string& InDebugName) const {
                auto tex = FTexture2D::Create(kCrosshairDim, kCrosshairDim);
                if (tex)
                    tex->SetData(const_cast<unsigned char*>(rgba.data()),
                                 static_cast<unsigned int>(rgba.size()));
                (void)InDebugName;
                return tex;
            }
        };

        void DrawRifleCross(FPixelWriter& w) {
            const int c = kCrosshairDim / 2;
            const int gap = 5;
            const int len = 14;
            const int thick = 2;
            w.LineV(c, c - gap - len, c - gap, thick, 240, 245, 255);
            w.LineV(c, c + gap, c + gap + len, thick, 240, 245, 255);
            w.LineH(c - gap - len, c, c - gap, thick, 240, 245, 255);
            w.LineH(c + gap, c, c + gap + len, thick, 240, 245, 255);
            w.FillRect(c - 1, c - 1, c + 1, c + 1, 255, 220, 120);
        }

        void DrawShotgunCircle(FPixelWriter& w) {
            const int c = kCrosshairDim / 2;
            w.CircleOutline(c, c, 16, 2, 255, 210, 90);
            for (int i = 0; i < 8; ++i) {
                const float ang = static_cast<float>(i) * 0.78539816f;
                const int x = c + static_cast<int>(std::cos(ang) * 16.0f);
                const int y = c + static_cast<int>(std::sin(ang) * 16.0f);
                w.FillRect(x - 1, y - 1, x + 1, y + 1, 255, 210, 90);
            }
            w.FillRect(c - 1, c - 1, c + 1, c + 1, 255, 240, 200);
        }

        void DrawRocketBrackets(FPixelWriter& w) {
            const int inset = 18;
            const int arm = 10;
            const int thick = 2;
            w.LineH(inset, inset, inset + arm, thick, 220, 80, 80);
            w.LineV(inset, inset, inset + arm, thick, 220, 80, 80);
            const int r = kCrosshairDim - inset - 1;
            w.LineH(r - arm, inset, r, thick, 220, 80, 80);
            w.LineV(r, inset, inset + arm, thick, 220, 80, 80);
            w.LineH(inset, r, inset + arm, thick, 220, 80, 80);
            w.LineV(inset, r - arm, r, thick, 220, 80, 80);
            w.LineH(r - arm, r, r, thick, 220, 80, 80);
            w.LineV(r, r - arm, r, thick, 220, 80, 80);
            w.FillRect(kCrosshairDim / 2 - 1, kCrosshairDim / 2 - 1, kCrosshairDim / 2 + 1, kCrosshairDim / 2 + 1,
                       255, 120, 120);
        }

        void DrawLaserScope(FPixelWriter& w) {
            const int c = kCrosshairDim / 2;
            w.CircleOutline(c, c, 20, 1, 40, 220, 255, 180);
            w.LineV(c, c - 18, c - 4, 1, 120, 240, 255);
            w.LineV(c, c + 4, c + 18, 1, 120, 240, 255);
            w.LineH(c - 18, c, c - 4, 1, 120, 240, 255);
            w.LineH(c + 4, c, c + 18, 1, 120, 240, 255);
            w.FillRect(c - 1, c - 1, c + 1, c + 1, 180, 250, 255);
        }

        void DrawGrenadeReticle(FPixelWriter& w) {
            const int c = kCrosshairDim / 2;
            w.CircleOutline(c, c, 14, 2, 90, 210, 80);
            w.LineV(c, c - 10, c + 10, 2, 90, 210, 80);
            w.LineH(c - 10, c, c + 10, 2, 90, 210, 80);
        }

        void DrawFlameCone(FPixelWriter& w) {
            const int c = kCrosshairDim / 2;
            w.FillRect(c - 1, c - 1, c + 1, c + 1, 255, 120, 40);
            for (int i = -2; i <= 2; ++i) {
                const int x0 = c + i * 6 - 2;
                const int x1 = c + i * 6 + 2;
                const int y0 = c + 6 + std::abs(i) * 2;
                w.FillRect(x0, y0, x1, y0 + 3, 255, 90 + static_cast<unsigned char>(i * 10), 30);
            }
            w.LineH(c - 14, c + 18, c + 14, 2, 255, 160, 40, 200);
        }

        TRef<FTexture2D> BuildCrosshairTexture(ELeonTournamentWeaponId InId) {
            FPixelWriter writer(kCrosshairDim);
            switch (InId) {
            case ELeonTournamentWeaponId::Shotgun:
                DrawShotgunCircle(writer);
                break;
            case ELeonTournamentWeaponId::Rocket:
                DrawRocketBrackets(writer);
                break;
            case ELeonTournamentWeaponId::Laser:
                DrawLaserScope(writer);
                break;
            case ELeonTournamentWeaponId::Grenade:
                DrawGrenadeReticle(writer);
                break;
            case ELeonTournamentWeaponId::Flamethrower:
                DrawFlameCone(writer);
                break;
            case ELeonTournamentWeaponId::Rifle:
            default:
                DrawRifleCross(writer);
                break;
            }
            return writer.Bake(LeonTournamentWeaponName(InId));
        }
    } // namespace

    TRef<FTexture2D> LeonTournamentGetCrosshairTexture(ELeonTournamentWeaponId InId) {
        static std::array<TRef<FTexture2D>, static_cast<size_t>(ELeonTournamentWeaponId::Count)> cache;
        const size_t idx = static_cast<size_t>(InId);
        if (idx >= cache.size())
            return cache[0];
        if (!cache[idx])
            cache[idx] = BuildCrosshairTexture(InId);
        return cache[idx];
    }

    void LeonTournamentApplyCrosshairBrush(UImage& InImage, ELeonTournamentWeaponId InId,
                                           const FLeonTournamentWeaponConfig& InConfig) {
        if (!InConfig.CrosshairTexturePath.empty()) {
            InImage.SetBrushFromPath(InConfig.CrosshairTexturePath);
            if (InImage.HasBrushTexture())
                return;
        }
        InImage.SetBrushFromTexture(LeonTournamentGetCrosshairTexture(InId));
        InImage.SetBrushUV({0.0f, 0.0f}, {1.0f, 1.0f});
    }

} // namespace Leon
