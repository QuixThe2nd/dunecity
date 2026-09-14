#ifndef CURSORRENDERER_H
#define CURSORRENDERER_H

#include <SDL.h>

namespace CursorRenderer {
// Draw in window coordinates after compositing. Restore the scene's letterbox,
// logical-size, scale and clip state so the pointer never changes game geometry.
inline bool draw(SDL_Renderer* renderer, SDL_Texture* texture, SDL_Point mouse,
                 SDL_Point windowSize, SDL_Point hotspot, int scale) {
    if(!renderer || !texture || windowSize.x <= 0 || windowSize.y <= 0 || scale < 1) return false;
    int outputW = 0, outputH = 0, width = 0, height = 0;
    if(SDL_GetRendererOutputSize(renderer, &outputW, &outputH) != 0
       || SDL_QueryTexture(texture, nullptr, nullptr, &width, &height) != 0) return false;
    int logicalW = 0, logicalH = 0;
    float scaleX = 1, scaleY = 1;
    SDL_Rect viewport{}, clip{};
    SDL_RenderGetLogicalSize(renderer, &logicalW, &logicalH);
    SDL_RenderGetScale(renderer, &scaleX, &scaleY);
    SDL_RenderGetViewport(renderer, &viewport);
    const bool clipped = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &clip);
    SDL_RenderSetLogicalSize(renderer, 0, 0);
    SDL_RenderSetScale(renderer, 1, 1);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetClipRect(renderer, nullptr);
    const float pixelX = static_cast<float>(outputW) / windowSize.x;
    const float pixelY = static_cast<float>(outputH) / windowSize.y;
    SDL_FRect destination{(mouse.x - hotspot.x * scale) * pixelX,
                          (mouse.y - hotspot.y * scale) * pixelY,
                          width * scale * pixelX, height * scale * pixelY};
    const bool drawn = SDL_RenderCopyF(renderer, texture, nullptr, &destination) == 0;
    SDL_RenderSetLogicalSize(renderer, logicalW, logicalH);
    float restoredX = 1, restoredY = 1;
    SDL_RenderGetScale(renderer, &restoredX, &restoredY);
    if(restoredX != scaleX || restoredY != scaleY) {
        SDL_RenderSetScale(renderer, scaleX, scaleY);
        SDL_RenderGetScale(renderer, &restoredX, &restoredY);
        // sdl2-compat multiplies an explicit scale by logical presentation
        // scaling; native SDL2 replaces it. Avoid multiplying the scene scale
        // on every frame, including when a caller supplied a custom scale.
        if(restoredX != scaleX || restoredY != scaleY)
            SDL_RenderSetScale(renderer, scaleX * scaleX / restoredX, scaleY * scaleY / restoredY);
    }
    SDL_RenderSetViewport(renderer, &viewport);
    SDL_RenderSetClipRect(renderer, clipped ? &clip : nullptr);
    return drawn;
}
}
#endif
