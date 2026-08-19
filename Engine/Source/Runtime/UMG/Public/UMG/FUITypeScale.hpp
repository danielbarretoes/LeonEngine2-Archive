#pragma once

namespace Leon {

    /**
     * @brief HTML-inspired UI type scale (multipliers on the 48px Inter atlas).
     *
     * Use these tokens instead of ad-hoc SetFontScale values.
     * FUILayout::kFs* aliases map the older names onto this scale.
     */
    struct FUITypeScale {
        static constexpr float H1 = 0.52f;     // page / hero title
        static constexpr float H2 = 0.42f;     // section title
        static constexpr float H3 = 0.32f;     // subsection / label
        static constexpr float P = 0.26f;      // body
        static constexpr float Small = 0.22f;  // captions / hints
        static constexpr float Button = 0.28f; // button labels
        static constexpr float Score = 0.48f;  // HUD scores
        static constexpr float Timer = 0.52f;  // HUD timer
        static constexpr float Vital = 0.56f;  // health / ammo
        static constexpr float Banner = 0.70f; // kill / death banners
    };

} // namespace Leon
