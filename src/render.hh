#ifndef RENDER_HH
#define RENDER_HH

#include <string>
#include <SDL2\SDL.h>

#include "resource.hh"
#include "vec.hh"

/**
 * \brief Log an SDL error with some error message to the output stream of our choice
 * format will be message error : SDL_GetError()
 * \param message The error message to write,
 * \param os The output stream to write the message to, cout by default
 */
void logSDLError(const std::string& message, std::ostream& os = std::cout)
{
    os << message << " Error : " << SDL_GetError() << std::endl;
}

void RenderVec(SDL_Renderer* renderer, const vec2f& pos, const vec2f& vec)
{
    SDL_FPoint line[]{ pos, pos + vec };
    SDL_RenderDrawLinesF(renderer, line, 2);
}

const int font_width = 8;
const int font_height = 14;
void RenderMessage(SDL_Renderer* renderer, int x, int y, const std::string& text)
{
    #if __EMSCRIPTEN__
        return; // FIXME: "src parameter invalid"
    #endif
    static SDL_Texture* font = nullptr;
    if (font == nullptr) {
        if ((font = loadTexture(renderer, "font.bmp")) == nullptr) {
            logSDLError("loadTexture");
        }
    }

    x *= font_width;
    y *= font_height;
    SDL_Rect src = { 0, 0, font_width, font_height };
    SDL_Rect dst = { x, y, font_width, font_height };
    for (const char& c : text)
    {
        if (c == '\n')
        {
            dst.x = x;
            dst.y += font_height;
        }
        else
        {
            src.x = (c - ' ') * font_width;
            SDL_RenderCopy(renderer, font, &src, &dst);
            dst.x += font_width;
        }
    }
}

void RenderCircle(SDL_Renderer* renderer, float radius, const vec2f& pos)
{
    const int count = 32;
    SDL_FPoint circle[count + 1];
    for (int i = 0; i < count; i++)
    {
        float angle = (float)i / count * 2 * M_PI;
        circle[i] = { pos.x + radius * std::cos(angle),
                      pos.y + radius * std::sin(angle) };
    }
    circle[count] = circle[0];
    SDL_RenderDrawLinesF(renderer, circle, count + 1);
}

#endif