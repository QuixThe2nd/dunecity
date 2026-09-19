#ifndef DUNECITY_MENU_PALETTE_H
#define DUNECITY_MENU_PALETTE_H

#include <Colors.h>

inline constexpr int validatedMenuPalette(int value) {
    return value == 1 ? 1 : 0;
}

namespace MenuTheme {
inline constexpr Uint32 background = COLOR_RGB(20,24,32);
inline constexpr Uint32 control = COLOR_RGB(39,45,57);
inline constexpr Uint32 pressed = COLOR_RGB(63,70,83);
inline constexpr Uint32 border = COLOR_RGB(94,104,120);
inline constexpr Uint32 accent = COLOR_RGB(245,199,103);
inline constexpr Uint32 text = COLOR_RGB(250,248,240);
inline constexpr Uint32 muted = COLOR_RGB(197,204,217);
// Preserve meaningful label colors while making dark team colors readable.
inline Uint32 readableText(Uint32 color) {
    auto c=RGBA2SDL(color);
    if(299*c.r+587*c.g+114*c.b < 150000)
        return COLOR_RGB((c.r+255)/2,(c.g+255)/2,(c.b+255)/2);
    return color;
}
}

struct MenuPalette {
    Uint32 foreground;
    Uint32 shadow;
};

inline constexpr MenuPalette menuPalette(int value) {
    return validatedMenuPalette(value) == 1
        ? MenuPalette{COLOR_WHITE, COLOR_TRANSPARENT}
        : MenuPalette{MenuTheme::text, COLOR_TRANSPARENT};
}

#endif
