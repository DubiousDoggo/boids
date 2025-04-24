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

bool DEBUG_ENABLE = false;

const float minSpeed = 2;
const float maxSpeed = 15;
const float maxAccel = 0.7f;

const float alignmentRadius = 75;
const float cohesionRadius = 75;
const float avoidanceRadius = 20;
const float obstacleRadius = 100;

const float alignmentWeight = 0.05f;
const float cohesionWeight = 0.005f;
const float avoidanceWeight = 0.05f;
const float obstacleWeight = 0.05f;

const float debugVecMultiplier = 200;

#define COLOR_BOID      255, 255, 255  //
#define COLOR_VELOCITY  255,   0, 255  // PURPLE
#define COLOR_ALIGNMENT   0,  64, 255  // BLUE
#define COLOR_COHESION    0, 240,   0  // GREEN
#define COLOR_AVOIDANCE 255,   0,   0  // RED
#define COLOR_OBSTACLE  255, 255,  64  // YELLOW

struct vec2f
{
    float x, y;

    constexpr vec2f& operator+=(const vec2f& vec)
    {
        x += vec.x;
        y += vec.y;
        return *this;
    }
    constexpr vec2f& operator-=(const vec2f& vec)
    {
        x -= vec.x;
        y -= vec.y;
        return *this;
    }
    constexpr vec2f& operator/=(float d)
    {
        x /= d;
        y /= d;
        return *this;
    }
    constexpr vec2f& operator*=(float m)
    {
        x *= m;
        y *= m;
        return *this;
    }
    constexpr operator SDL_FPoint() const { return { x, y }; }
};

constexpr vec2f operator+(vec2f vec1, const vec2f& vec2) { return vec1 += vec2; }
constexpr vec2f operator-(vec2f vec1, const vec2f& vec2) { return vec1 -= vec2; }
constexpr vec2f operator/(vec2f vec, float d) { return vec /= d; }
constexpr vec2f operator*(vec2f vec, float m) { return vec *= m; }

inline float dist_squared(const vec2f& p1, const vec2f& p2) { return (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y); }
inline float dist(const vec2f& p1, const vec2f& p2) { return sqrtf(dist_squared(p1, p2)); }
constexpr float mag(const vec2f& vec) { return sqrtf(vec.x * vec.x + vec.y * vec.y); }
constexpr vec2f normal(const vec2f& vec) { return mag(vec) != 0 ? vec / mag(vec) : vec2f{ 0, 0 }; }
constexpr vec2f clamp_mag(const vec2f& vec, float min, float max) { return mag(vec) > max ? normal(vec) * max : mag(vec) < min ? normal(vec) * min : vec; }

/** Compute the shortest vector from pos to the interior of rect */
constexpr vec2f vec_to(vec2f pos, SDL_FRect rect)
{
    vec2f result = { 0, 0 };
    if (pos.x < rect.x) { result.x = rect.x - pos.x; }
    if (pos.y < rect.y) { result.y = rect.y - pos.y; }
    if (pos.x > rect.x + rect.w) { result.x = rect.x + rect.w - pos.x; }
    if (pos.y > rect.y + rect.h) { result.y = rect.y + rect.h - pos.y; }
    return result;
}

template <typename T>
constexpr T clamp(const T& a, const T& mi, const T& ma) { return std::min(std::max(a, mi), ma); }

float wrapf(float value, float min, float max)
{
    float delta = max - min;
    while (value >= max) value -= delta;
    while (value < min) value += delta;
    return value;
}
float wrapf(float value, float max) { return wrapf(value, 0, max); }

struct boid { vec2f pos, vel; };

std::vector<boid> boids(100);

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

void RenderMessage(SDL_Renderer* renderer, int x, int y, const std::string& text)
{
    static SDL_Texture* font = nullptr;
    if (font == nullptr) {
        if ((font = loadTexture(renderer, "font.bmp")) == nullptr) {
            logSDLError("loadTexture");
        }
    }

    const int font_width = 8;
    const int font_height = 14;
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

void RenderCircle(SDL_Renderer* renderer, float radius, float x, float y)
{
    const int count = 16;
    SDL_FPoint circle[count + 1];
    for (int i = 0; i < count; i++)
    {
        float angle = (float)i / count * 2 * M_PI;
        circle[i] = { x + radius * cosf(angle),
                      y + radius * sinf(angle) };
    }
    circle[count] = circle[0];
    SDL_RenderDrawLinesF(renderer, circle, count + 1);
}

void RenderBoids(SDL_Renderer* renderer)
{

    for (std::size_t i = 0; i < boids.size(); i++)
    {
        const boid& boid = boids[i];
        vec2f direction = normal(boid.vel);
        const int boid_size = 10;
        SDL_FPoint triangle[3]{ {boid.pos.x + boid_size * (-direction.y - direction.x), boid.pos.y + boid_size * (direction.x - direction.y)},
                                {boid.pos.x + boid_size * direction.x , boid.pos.y + boid_size * direction.y },
                                {boid.pos.x + boid_size * (direction.y - direction.x), boid.pos.y + boid_size * (-direction.x - direction.y)} };
        SDL_SetRenderDrawColor(renderer, COLOR_BOID, 255);
        SDL_RenderDrawLinesF(renderer, triangle, 3);

        if (DEBUG_ENABLE && i == 0)
        {
            SDL_SetRenderDrawColor(renderer, COLOR_ALIGNMENT, 255);
            RenderCircle(renderer, alignmentRadius, boid.pos.x, boid.pos.y);
            SDL_SetRenderDrawColor(renderer, COLOR_COHESION, 255);
            RenderCircle(renderer, cohesionRadius, boid.pos.x, boid.pos.y);
            SDL_SetRenderDrawColor(renderer, COLOR_AVOIDANCE, 255);
            RenderCircle(renderer, avoidanceRadius, boid.pos.x, boid.pos.y);

            char s[200];
            sprintf(s, "Boid 0 pos(%4.3f,%4.3f) vel(%3.3f,%3.3f)", boid.pos.x, boid.pos.y, boid.vel.x, boid.vel.y);
            RenderMessage(renderer, 10, 10, s);
        }
    }
}

void MoveBoids(SDL_Renderer* renderer)
{
    std::vector<boid> new_boids(boids.size());
    for (std::size_t i = 0; i < boids.size(); i++)
    {
        const boid& b = boids[i];
        boid& n = new_boids[i];

        vec2f alignment_vec{ 0, 0 }, cohesion_vec{ 0, 0 }, avoidance_vec{ 0, 0 }, obstacle_vec{ 0, 0 };
        int alignment_count = 0, cohesion_count = 0, avoidance_count = 0, obstacle_count = 0;

        for (std::size_t k = 0; k < boids.size(); k++)
        {
            if (k == i) continue;

            boid& g = boids[k];

            if (dist_squared(b.pos, g.pos) <= alignmentRadius * alignmentRadius)
            {
                alignment_vec += g.vel;
                alignment_count++;
            }

            if (dist_squared(b.pos, g.pos) <= cohesionRadius * cohesionRadius)
            {
                cohesion_vec += g.pos - b.pos;
                cohesion_count++;
            }

            if (dist_squared(b.pos, g.pos) <= avoidanceRadius * avoidanceRadius)
            {
                avoidance_vec += b.pos - g.pos;
                avoidance_count++;
            }
        }

        SDL_FRect bounds = { obstacleRadius, obstacleRadius, WINDOW_WIDTH - 2 * obstacleRadius, WINDOW_HEIGHT - 2 * obstacleRadius };
        obstacle_vec = vec_to(b.pos, bounds);
        n.vel += obstacle_vec * obstacleWeight;
        if (DEBUG_ENABLE) {
            SDL_SetRenderDrawColor(renderer, COLOR_OBSTACLE, 255);
            SDL_RenderDrawRectF(renderer, &bounds);
            RenderVec(renderer, b.pos, obstacle_vec);
        }

        if (i == 0) {
            SDL_SetRenderDrawColor(renderer, COLOR_VELOCITY, 255);
            RenderVec(renderer, b.pos, b.vel * debugVecMultiplier);
        }

        if (alignment_count > 0) {
            alignment_vec /= alignment_count;
            n.vel += alignment_vec *= alignmentWeight;
            if (DEBUG_ENABLE && i == 0) {
                SDL_SetRenderDrawColor(renderer, COLOR_ALIGNMENT, 255);
                RenderVec(renderer, b.pos, alignment_vec * debugVecMultiplier);
            }
        }
        if (cohesion_count > 0) {
            cohesion_vec /= cohesion_count;
            cohesion_vec *= cohesionWeight;
            n.vel += cohesion_vec;
            if (DEBUG_ENABLE && i == 0) {
                SDL_SetRenderDrawColor(renderer, COLOR_COHESION, 255);
                RenderVec(renderer, b.pos, cohesion_vec * debugVecMultiplier);
            }
        }
        if (avoidance_count > 0) {
            avoidance_vec *= avoidanceWeight;
            n.vel += avoidance_vec;
            if (DEBUG_ENABLE && i == 0) {
                SDL_SetRenderDrawColor(renderer, COLOR_AVOIDANCE, 255);
                RenderVec(renderer, b.pos, avoidance_vec * debugVecMultiplier);
            }
        }

        if (DEBUG_ENABLE && i == 0) {
            SDL_SetRenderDrawColor(renderer, COLOR_VELOCITY, 255);
            RenderVec(renderer, b.pos, n.vel * debugVecMultiplier);
        }

        n.vel = clamp_mag(n.vel, 0, maxAccel);
        n.vel = clamp_mag(b.vel + n.vel, 0, maxSpeed);

        n.pos = b.pos + n.vel;

    }
    boids = new_boids;
}

int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        logSDLError("Init");
        return EXIT_FAILURE;
    }

    SDL_Window* window = SDL_CreateWindow("Hello, Boids!", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        logSDLError("CreateWindow");
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (sdlRenderer == nullptr) {
        cleanup(window);
        logSDLError("CreateRenderer");
        SDL_Quit();
        return EXIT_FAILURE;
    }

    std::default_random_engine generator;
    std::uniform_real_distribution<float> rand_x(0, WINDOW_WIDTH);
    std::uniform_real_distribution<float> rand_y(0, WINDOW_HEIGHT);
    std::uniform_real_distribution<float> rand_vel(-maxSpeed, maxSpeed);

    for (boid& boid : boids) {
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

        RenderBoids(sdlRenderer);


        // Render parameters
        RenderMessage(sdlRenderer, 5, 5, "Press ~ to toggle debug options");

        // Update boids
        MoveBoids(sdlRenderer);

        SDL_RenderPresent(sdlRenderer);

        // process events
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (ev.key.keysym.sym) {
                case SDLK_UP:
                case SDLK_DOWN:
                case SDLK_LEFT:
                case SDLK_RIGHT:

                    break;
                case SDLK_BACKQUOTE:
                    DEBUG_ENABLE = !DEBUG_ENABLE;
                    break;
                }
                break;
            }
        }
    }

    cleanup(sdlRenderer, window);
    SDL_Quit();
    return 0;
}
