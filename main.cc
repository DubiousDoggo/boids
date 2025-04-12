#include <SDL2\SDL.h>

#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <random>

#include "cleanup.hh"
#include "resource.hh"

/* Define window size */
const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

// Clamp value into set range
template <typename T>
inline const T clamp(const T &a, const T &mi, const T &ma) { return std::min(std::max(a, mi), ma); }

float wrapf(float value, float min, float max)
{
    float delta = max - min;
    while (value >= max)
        value -= delta;
    while (value < min)
        value += delta;
    return value;
}
float wrapf(float value, float max) { return wrapf(value, 0, max); }

const float minSpeed = 1;
const float maxSpeed = 7;

const float alignmentRadius = 450;
const float cohesionRadius = 500;
const float avoidanceRadius = 100;

const float alignmentWeight = 1.5f;
const float cohesionWeight = 1.0f;
const float avoidanceWeight = 1.6f;

const float debugVecMultiplier = 200;
const float maxAccel = 1.5f; 

struct vec2f
{
    float x, y;

    constexpr vec2f &operator+=(const vec2f &vec)
    {
        x += vec.x;
        y += vec.y;
        return *this;
    }
    constexpr vec2f &operator-=(const vec2f &vec)
    {
        x -= vec.x;
        y -= vec.y;
        return *this;
    }
    constexpr vec2f &operator/=(float d)
    {
        x /= d;
        y /= d;
        return *this;
    }
    constexpr vec2f &operator*=(float m)
    {
        x *= m;
        y *= m;
        return *this;
    }
    constexpr operator SDL_FPoint() const { return {x, y}; }
};

constexpr vec2f operator+(vec2f vec1, const vec2f &vec2) { return vec1 += vec2; }
constexpr vec2f operator-(vec2f vec1, const vec2f &vec2) { return vec1 -= vec2; }
constexpr vec2f operator/(vec2f vec, float d) { return vec /= d; }
constexpr vec2f operator*(vec2f vec, float m) { return vec *= m; }

inline float dist_squared(const vec2f &p1, const vec2f &p2) { return (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y); }
inline float dist(const vec2f &p1, const vec2f &p2) { return sqrtf(dist_squared(p1, p2)); }
constexpr float mag(const vec2f &vec) { return sqrtf(vec.x * vec.x + vec.y * vec.y); }
constexpr vec2f normal(const vec2f &vec) { return mag(vec) != 0 ? vec / mag(vec) : vec2f{0, 0}; }
constexpr vec2f clamp_mag(const vec2f &vec, float min, float max) { return mag(vec) > max ? normal(vec) * max : mag(vec) < min ? normal(vec) * min : vec; }

struct boid { vec2f pos, vel; };

std::vector<boid> boids(20);

void MoveBoids(SDL_Renderer *renderer)
{
    std::vector<boid> new_boids(boids.size());
    for (std::size_t i = 0; i < boids.size(); i++)
    {
        const boid &b = boids[i];
        boid &n = new_boids[i];

        vec2f alignment_vec{0, 0}, cohesion_vec{0, 0}, avoidance_vec{0, 0};
        int alignment_count = 0, cohesion_count = 0, avoidance_count = 0;

        for (std::size_t k = 0; k < boids.size(); k++)
        {
            if (k == i) continue;

            boid &g = boids[k];

            if (dist_squared(b.pos, g.pos) <= alignmentRadius * alignmentRadius)
            {
                alignment_vec += normal(g.vel);
                alignment_count++;
            }

            if (dist_squared(b.pos, g.pos) <= cohesionRadius * cohesionRadius)
            {
                cohesion_vec += normal(g.pos - b.pos);
                cohesion_count++;
            }

            if (dist_squared(b.pos, g.pos) <= avoidanceRadius * avoidanceRadius)
            {
                avoidance_vec += normal(b.pos - g.pos);
                avoidance_count++;
            }
        }

        if (i == 0) {
            SDL_FPoint line[2] = { b.pos, b.pos + b.vel * debugVecMultiplier };
            SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
            SDL_RenderDrawLinesF(renderer, line, 2);
        }

        if (alignment_count > 0) {
            alignment_vec = normal(alignment_vec);
            if (i == 0) {
                SDL_FPoint line[2] = { b.pos, b.pos + alignment_vec * debugVecMultiplier };
                SDL_SetRenderDrawColor(renderer, 0, 64, 255, 255);
                SDL_RenderDrawLinesF(renderer, line, 2);
            }
            n.vel += alignment_vec * alignmentWeight;
        }
        if (cohesion_count > 0) {
            cohesion_vec = normal(cohesion_vec);
            if (i == 0) {
                SDL_FPoint line[2] = { b.pos, b.pos + cohesion_vec * debugVecMultiplier };
                SDL_SetRenderDrawColor(renderer, 0, 240, 0, 255);
                SDL_RenderDrawLinesF(renderer, line, 2);
            }
            n.vel += cohesion_vec * cohesionWeight;
        }
        if (avoidance_count > 0) {
            avoidance_vec = normal(avoidance_vec);
            if (i == 0) {
                SDL_FPoint line[2] = { b.pos, b.pos + avoidance_vec * debugVecMultiplier};
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderDrawLinesF(renderer, line, 2);
            }
            n.vel += avoidance_vec * avoidanceWeight;
        }
        
        if (i == 0) {
            SDL_FPoint line[2] = { b.pos, b.pos + clamp_mag(n.vel, 0, maxSpeed) * debugVecMultiplier};
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLinesF(renderer, line, 2);
            line[0] = line[1];
            line[1] = b.pos + clamp_mag(n.vel, minSpeed, INFINITY) * debugVecMultiplier;
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
            SDL_RenderDrawLinesF(renderer, line, 2);
        }

        n.vel = clamp_mag(n.vel, 0, maxAccel);
        n.vel = clamp_mag(b.vel + n.vel, minSpeed, maxSpeed);
        
        n.pos = b.pos + n.vel;
        n.pos.x = wrapf(n.pos.x, WINDOW_WIDTH);
        n.pos.y = wrapf(n.pos.y, WINDOW_HEIGHT);
    }
    boids = new_boids;
}


/**
 * \brief Log an SDL error with some error message to the output stream of our choice
 * format will be message error : SDL_GetError()
 * \param message The error message to write,
 * \param os The output stream to write the message to, cout by default
 */
void logSDLError(const std::string &message, std::ostream &os = std::cout)
{
    os << message << " Error : " << SDL_GetError() << std::endl;
}

void RenderMessage(SDL_Renderer *renderer, int x, int y, const std::string &text)
{

    static SDL_Texture *font = nullptr;
    if (font == nullptr) {
        if (!(font = loadTexture(renderer, "font.bmp")))
            logSDLError("loadTexture");
    }

    const int font_width = 8;
    const int font_height = 14;
    x *= font_width;
    y *= font_height;
    SDL_Rect src = {0, 0, font_width, font_height};
    SDL_Rect dst = {x, y, font_width, font_height};
    for (const char &c : text)
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

void RenderCircle(SDL_Renderer *renderer, float radius, int x, int y)
{
    const int count = 16;
    SDL_FPoint circle[count + 1];
    for (int i = 0; i < count; i++)
    {
        float angle = (float)i / count * 2 * M_PI;
        circle[i] = {x + radius * cosf(angle),
                     y + radius * sinf(angle)};
    }
    circle[count] = circle[0];
    SDL_RenderDrawLinesF(renderer, circle, count + 1);
}

void DrawBoids(SDL_Renderer *renderer)
{

    for (std::size_t i = 0; i < boids.size(); i++)
    {
        const boid &boid = boids[i];
        vec2f direction = normal(boid.vel);
        const int boid_size = 20;
        SDL_FPoint triangle[3]{{boid.pos.x + boid_size * (-direction.y - direction.x), boid.pos.y + boid_size * (direction.x - direction.y)},
                               {boid.pos.x + boid_size * direction.x, boid.pos.y + boid_size * direction.y},
                               {boid.pos.x + boid_size * (direction.y - direction.x), boid.pos.y + boid_size * (-direction.x - direction.y)}};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLinesF(renderer, triangle, 3);

        if (i != 0) continue;
        SDL_SetRenderDrawColor(renderer, 0, 64, 255, 255);
        RenderCircle(renderer, alignmentRadius, boid.pos.x, boid.pos.y);
        SDL_SetRenderDrawColor(renderer, 0, 240, 0, 255);
        RenderCircle(renderer, cohesionRadius, boid.pos.x, boid.pos.y);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        RenderCircle(renderer, avoidanceRadius, boid.pos.x, boid.pos.y);

        char s[200];
        sprintf(s, "Boid 0 pos(%4.3f,%4.3f) vel(%3.3f,%3.3f)", boid.pos.x, boid.pos.y, boid.vel.x, boid.vel.y);
        RenderMessage(renderer, 10, 10, s);
    }
}


int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        logSDLError("Init");
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("Hello, Boids!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == nullptr)
    {
        logSDLError("CreateWindow");
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (sdlRenderer == nullptr)
    {
        cleanup(window);
        logSDLError("CreateRenderer");
        SDL_Quit();
        return 1;
    }

    std::default_random_engine generator;
    std::uniform_real_distribution<float> rand_x(0, WINDOW_WIDTH);
    std::uniform_real_distribution<float> rand_y(0, WINDOW_HEIGHT);
    std::uniform_real_distribution<float> rand_vel(-maxSpeed, maxSpeed);

    for (boid &boid : boids)
    {
        boid.pos.x = rand_x(generator);
        boid.pos.y = rand_y(generator);
        boid.vel.x = rand_vel(generator);
        boid.vel.y = rand_vel(generator);
    }

    bool running = true;
    while (running)
    {
        // Draw the screen
        SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(sdlRenderer);

        DrawBoids(sdlRenderer);
        // RenderMessage(sdlRenderer, font, 0, 0, "TEST");
        // RenderCircle(sdlRenderer, 20, 0, 0);

        // process events
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
            {
                running = false;
            }
        }

        // Update boids
        MoveBoids(sdlRenderer);

        SDL_RenderPresent(sdlRenderer);
    }

    cleanup(sdlRenderer, window);
    SDL_Quit();
    return 0;
}
