#include <catch2/catch_test_macros.hpp>
#include <misc/CursorRenderer.h>
#include <misc/SDL2pp.h>

TEST_CASE("Cursor overlay stays visible over letterboxed clipped content at every scale", "[rendering][cursor]") {
    sdl2::surface_ptr surface{SDL_CreateRGBSurfaceWithFormat(0, 160, 100, 32, SDL_PIXELFORMAT_ARGB8888)};
    REQUIRE(surface);
    sdl2::renderer_ptr draw{SDL_CreateSoftwareRenderer(surface.get())};
    REQUIRE(draw);
    sdl2::surface_ptr sprite{SDL_CreateRGBSurfaceWithFormat(0, 3, 3, 32, SDL_PIXELFORMAT_ARGB8888)};
    REQUIRE(sprite);
    SDL_FillRect(sprite.get(), nullptr, SDL_MapRGBA(sprite->format, 255, 255, 255, 255));
    sdl2::texture_ptr texture{SDL_CreateTextureFromSurface(draw.get(), sprite.get())};
    REQUIRE(texture);
    for(int scale = 1; scale <= 4; ++scale) {
        SDL_RenderSetLogicalSize(draw.get(), 80, 80);
        if(scale % 2 == 0) SDL_RenderSetScale(draw.get(), 1.2f, 0.8f);
        const SDL_Rect clip{10, 10, 15, 15};
        SDL_RenderSetClipRect(draw.get(), &clip);
        int logicalW, logicalH;
        float scaleX, scaleY;
        SDL_Rect viewport{};
        SDL_RenderGetLogicalSize(draw.get(), &logicalW, &logicalH);
        SDL_RenderGetScale(draw.get(), &scaleX, &scaleY);
        SDL_RenderGetViewport(draw.get(), &viewport);
        SDL_SetRenderDrawColor(draw.get(), 0, 0, 0, 255);
        SDL_RenderClear(draw.get());
        // Window points are half physical pixels (Retina). Pointer is in the
        // left letterbox, outside the menu's clip, and its center is the hotspot.
        REQUIRE(CursorRenderer::draw(draw.get(), texture.get(), {8, 10}, {80, 50}, {1, 1}, scale));
        sdl2::surface_ptr captured{SDL_CreateRGBSurfaceWithFormat(0, 160, 100, 32, SDL_PIXELFORMAT_ARGB8888)};
        // Read the full output without the restored scene transform.
        int afterW, afterH; float afterX, afterY; SDL_Rect afterViewport{}, afterClip{};
        SDL_RenderGetLogicalSize(draw.get(), &afterW, &afterH);
        SDL_RenderGetScale(draw.get(), &afterX, &afterY);
        SDL_RenderGetViewport(draw.get(), &afterViewport);
        SDL_RenderGetClipRect(draw.get(), &afterClip);
        CHECK(afterW == logicalW); CHECK(afterH == logicalH);
        CHECK(afterX == scaleX); CHECK(afterY == scaleY);
        CHECK(SDL_RectEquals(&viewport, &afterViewport));
        CHECK(SDL_RenderIsClipEnabled(draw.get()));
        CHECK(SDL_RectEquals(&clip, &afterClip));
        SDL_RenderSetLogicalSize(draw.get(), 0, 0);
        SDL_RenderSetScale(draw.get(), 1, 1);
        SDL_RenderSetViewport(draw.get(), nullptr);
        SDL_RenderSetClipRect(draw.get(), nullptr);
        REQUIRE(SDL_RenderReadPixels(draw.get(), nullptr, SDL_PIXELFORMAT_ARGB8888, captured->pixels, captured->pitch) == 0);
        int white = 0;
        for(int y = 0; y < 100; ++y) for(int x = 0; x < 160; ++x) {
            auto pixel = reinterpret_cast<Uint32*>(static_cast<Uint8*>(captured->pixels) + y * captured->pitch)[x];
            if((pixel & 0xffffff) == 0xffffff) ++white;
        }
        CHECK(white == 36 * scale * scale);
    }
}

TEST_CASE("Cursor size is consistent at standard, fractional and Retina display densities", "[rendering][cursor]") {
    // Model 100%, 125%, 150% and 200% backing scales, independent of the
    // display's physical DPI. A four-unit sprite always occupies four window
    // units; only its physical pixel coverage changes.
    for(int percentage : {100, 125, 150, 200}) {
        const int outputW = 160 * percentage / 100;
        const int outputH = 100 * percentage / 100;
        sdl2::surface_ptr surface{SDL_CreateRGBSurfaceWithFormat(0, outputW, outputH, 32, SDL_PIXELFORMAT_ARGB8888)};
        REQUIRE(surface);
        sdl2::renderer_ptr renderer{SDL_CreateSoftwareRenderer(surface.get())};
        REQUIRE(renderer);
        sdl2::surface_ptr sprite{SDL_CreateRGBSurfaceWithFormat(0, 4, 4, 32, SDL_PIXELFORMAT_ARGB8888)};
        REQUIRE(sprite);
        SDL_FillRect(sprite.get(), nullptr, SDL_MapRGBA(sprite->format, 255, 255, 255, 255));
        sdl2::texture_ptr texture{SDL_CreateTextureFromSurface(renderer.get(), sprite.get())};
        REQUIRE(texture);
        for(int scale = 1; scale <= 4; ++scale) {
            CAPTURE(percentage, scale);
            SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
            SDL_RenderClear(renderer.get());
            REQUIRE(CursorRenderer::draw(renderer.get(), texture.get(), {40, 40}, {160, 100}, {0, 0}, scale));
            REQUIRE(SDL_RenderReadPixels(renderer.get(), nullptr, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch) == 0);
            const int edge = 40 * percentage / 100;
            const int size = 4 * scale * percentage / 100;
            int mismatches = 0;
            for(int y = 0; y < outputH; ++y) for(int x = 0; x < outputW; ++x) {
                auto pixel = reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels) + y * surface->pitch)[x];
                const bool white = (pixel & 0xffffff) == 0xffffff;
                const bool expected = x >= edge && x < edge + size && y >= edge && y < edge + size;
                if(white != expected) ++mismatches;
            }
            CHECK(mismatches == 0);
        }
    }
}
