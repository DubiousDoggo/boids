
#if __EMSCRIPTEN__
	#include <emscripten/emscripten.h>
#endif

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

float minSpeed = 2;
float maxSpeed = 12;
float maxAccel = 5.7f;
float baseAccel = 0.6f;
float dragCoeff = 0.004f;

float tailwindAngle = M_PI / 4;

float alignmentRadius = 96; 
float cohesionRadius = 100;
float avoidanceRadius = 25;
float tailwindRadius = 150; 
float obstacleRadius = 200;

float edgeObstacle = 20;

float alignmentWeight = 0.05f;
float cohesionWeight = 0.005f;
float avoidanceWeight = 0.05f;
float tailwindWeight = 0.02f;
float obstacleWeight = 0.01f;

float leaderChance = 0.01;

const float debugVecMultiplier = 200;

#define COLOR_CURSOR    255, 255, 255  // WHITE
#define COLOR_VELOCITY  255,   0, 255  // PURPLE
#define COLOR_ALIGNMENT  32,  64, 255  // BLUE
#define COLOR_COHESION    0, 240,   0  // GREEN
#define COLOR_AVOIDANCE 255,   0,   0  // RED
#define COLOR_TAILWIND  255, 255,  64  // YELLOW
#define COLOR_ACCEL       0, 255, 255  // CYAN
#define COLOR_OBSTACLE  255, 128,   0  // ORANGE
#define COLOR_DRAG      128, 128, 128  // GREY

#define COLOR_BOID   255, 255, 255 // WHITE
#define COLOR_LEADER   0, 160, 200 // DEEP BLUE
#define COLOR_LEFT     0, 240,   0 // GREEN
#define COLOR_RIGHT  255,   0,   0 // RED

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
    constexpr vec2f operator-() const
    {
        vec2f vec{0, 0};
        return vec -= *this;
    }

    constexpr operator SDL_FPoint() const { return { x, y }; }
};

constexpr vec2f operator+(vec2f vec1, const vec2f& vec2) { return vec1 += vec2; }
constexpr vec2f operator-(vec2f vec1, const vec2f& vec2) { return vec1 -= vec2; }
constexpr vec2f operator/(vec2f vec, float d) { return vec /= d; }
constexpr vec2f operator*(vec2f vec, float m) { return vec *= m; }

inline float dist_squared(const vec2f& p1, const vec2f& p2) { return (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y); }
inline float dist(const vec2f& p1, const vec2f& p2) { return sqrtf(dist_squared(p1, p2)); }
constexpr float dot(const vec2f& v1, const vec2f& v2) { return v1.x * v2.x + v1.y * v2.y; }
/** @return the magnitude of the cross product of v1 and v2 */
constexpr float cross_mag(const vec2f& v1, const vec2f& v2) { return v1.x * v2.y - v1.y * v2.x; } 
/** @return the projection of v1 onto v2 */
constexpr vec2f proj(const vec2f& v1, const vec2f& v2) { return v2 * dot(v1, v2) / dot(v2, v2); }
constexpr float mag(const vec2f& vec) { return sqrtf(vec.x * vec.x + vec.y * vec.y); }
constexpr vec2f normal(const vec2f& vec) { return mag(vec) != 0 ? vec / mag(vec) : vec2f{ 0, 0 }; }
constexpr vec2f clamp_mag(const vec2f& vec, float min, float max) { return mag(vec) > max ? normal(vec) * max : mag(vec) < min ? normal(vec) * min : vec; }
inline vec2f rotate(const vec2f& vec, float angle) { return { vec.x * cosf(angle) - vec.y * sinf(angle), vec.x * sinf(angle) + vec.y * cosf(angle) }; }

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
    while (value <  min) value += delta;
    return value;
}
float wrapf(float value, float max) { return wrapf(value, 0, max); }

#define FLAG_LEADER (1)
#define FLAG_HANDED (1 << 1)

struct boid { 
    vec2f pos, vel; 
    unsigned flags;
};

std::vector<boid> boids(100);
std::vector<SDL_FRect> obstacles;

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

#include <memory>
#include <string>
#include <stdexcept>
template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    auto buf = std::make_unique<char[]>( size );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
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
        return; // FIXME: src parameter invalid
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
        circle[i] = { pos.x + radius * cosf(angle),
                      pos.y + radius * sinf(angle) };
    }
    circle[count] = circle[0];
    SDL_RenderDrawLinesF(renderer, circle, count + 1);
}

/** calculate the acceleration of the boid at index, based on neighboring boids */
vec2f calc_accel(const std::vector<boid>& boids, std::size_t index, SDL_Renderer* debug_render = nullptr)
{
    const boid& b = boids[index];

    vec2f alignment_vec{ 0, 0 }, cohesion_vec{ 0, 0 }, avoidance_vec{ 0, 0 }, tailwind_vec{ 0, 0 };
    int alignment_count = 0, cohesion_count = 0, avoidance_count = 0, tailwind_count = 0;
    
    for (std::size_t k = 0; k < boids.size(); k++)
    {
        if (k == index) continue;
        
        const boid& g = boids[k];
 
        if (dist_squared(b.pos, g.pos) <= alignmentRadius * alignmentRadius) {
            alignment_vec += g.vel - b.vel;
            alignment_count++;
        }
        if (dist_squared(b.pos, g.pos) <= cohesionRadius * cohesionRadius) {
            cohesion_vec += g.pos - b.pos;
            cohesion_count++;
        }
        if (dist_squared(b.pos, g.pos) <= avoidanceRadius * avoidanceRadius) {
            if ((b.flags & FLAG_LEADER) && !(g.flags & FLAG_LEADER)) { // let leaders pass to the front
                avoidance_vec += normal(b.vel) * (avoidanceRadius - mag(b.pos - g.pos));
            } else {
                avoidance_vec += normal(b.pos - g.pos) * (avoidanceRadius - mag(b.pos - g.pos));
            }
            avoidance_count++;
        }
        if (dist_squared(b.pos, g.pos) <= tailwindRadius * tailwindRadius) { 

            if (dot(normal(b.vel), normal(g.vel)) < 0) continue; // ignore boids traveling in opposite direction
            if (!(g.flags & FLAG_LEADER)) continue; // only flock on leaders

            vec2f tail = normal(-g.vel); // vec backwards from g
            vec2f bird = b.pos - g.pos;  // vec from g to b
            vec2f rej;
            if (dot(normal(bird), tail) < 0) { // fall behind leader
                vec2f p = proj(bird, tail);
                rej = p;
            } else { // attempt to make a triangular looking flock
                float rot = tailwindAngle * (b.flags & FLAG_HANDED ? 1 : -1);
                tail = rotate(tail, rot); // rotate tail vector in handedness of b
                vec2f p = proj(bird, tail);
                rej = bird - p; // rejection from tail vector to b
            }

            float tmp = mag(rej) - .75 * tailwindRadius;
            rej = normal(rej) * ((-1 / (.75 * tailwindRadius)) * tmp * tmp + (tailwindRadius * .75));
            tailwind_vec += -rej; // move b towards tail vector
            tailwind_count++;

            if (debug_render != nullptr)
            {
                SDL_SetRenderDrawColor(debug_render, COLOR_LEADER, 255);
                RenderVec(debug_render, g.pos, tail * tailwindRadius);
            }
        }
    }

    if (alignment_count > 0) {
        alignment_vec /= alignment_count; 
        alignment_vec *= alignmentWeight;
    }
    if (cohesion_count > 0) {
        cohesion_vec /= cohesion_count;
        cohesion_vec *= cohesionWeight;            
    }
    if (avoidance_count > 0) {
        avoidance_vec *= avoidanceWeight;    
    }
    if (tailwind_count > 0) {
        tailwind_vec /= tailwind_count;
        tailwind_vec *= tailwindWeight;
    }

    vec2f obstacle_vec{ 0, 0 };
    
    vec2f closest_vec{INFINITY, INFINITY};
    for (const SDL_FRect& obstacle : obstacles) {
        vec2f ob = vec_to(b.pos, obstacle);
        if (mag(ob) > obstacleRadius) continue; // skip if outside obstacle avoidance radius
        if (dot(ob, b.vel) < 0) continue; // skip if not headed toward the obstacle
        if (mag(ob) < mag(closest_vec)) closest_vec = ob;
    }
    if (mag(closest_vec) != INFINITY) {
        obstacle_vec += -normal(closest_vec - proj(closest_vec, b.vel)) * (obstacleRadius - mag(closest_vec)); 
    }

    obstacle_vec *= obstacleWeight;

    vec2f drag_vec = -b.vel * mag(b.vel) * dragCoeff;
    vec2f base_vec = normal(b.vel) * baseAccel;

    // accelerate
    vec2f accel = alignment_vec + cohesion_vec + avoidance_vec + tailwind_vec + obstacle_vec + drag_vec + base_vec; 
    
    if (debug_render != nullptr)
    {
        SDL_SetRenderDrawColor(debug_render, COLOR_ALIGNMENT, 255);
        RenderVec(debug_render, b.pos, alignment_vec * debugVecMultiplier);
        RenderCircle(debug_render, alignmentRadius, b.pos);
        
        SDL_SetRenderDrawColor(debug_render, COLOR_COHESION, 255);
        RenderVec(debug_render, b.pos, cohesion_vec * debugVecMultiplier);
        RenderCircle(debug_render, cohesionRadius, b.pos);
        
        SDL_SetRenderDrawColor(debug_render, COLOR_AVOIDANCE, 255);
        RenderVec(debug_render, b.pos, avoidance_vec * debugVecMultiplier);
        RenderCircle(debug_render, avoidanceRadius, b.pos);
        
        SDL_SetRenderDrawColor(debug_render, COLOR_TAILWIND, 255);
        RenderVec(debug_render, b.pos, tailwind_vec * debugVecMultiplier);
        RenderCircle(debug_render, tailwindRadius, b.pos);
        
        SDL_SetRenderDrawColor(debug_render, COLOR_OBSTACLE, 255);
        RenderVec(debug_render, b.pos, obstacle_vec * debugVecMultiplier);
        RenderCircle(debug_render, obstacleRadius, b.pos);
        for (const SDL_FRect& obstacle : obstacles) {
            SDL_RenderDrawRectF(debug_render, &obstacle);
        }

        SDL_SetRenderDrawColor(debug_render, COLOR_DRAG, 255);
        RenderVec(debug_render, b.pos, drag_vec * debugVecMultiplier);
        
        SDL_SetRenderDrawColor(debug_render, COLOR_VELOCITY, 255);
        RenderVec(debug_render, b.pos, base_vec * debugVecMultiplier);
        
        SDL_SetRenderDrawColor(debug_render, COLOR_ACCEL, 255);
        RenderVec(debug_render, b.pos, accel * debugVecMultiplier);
           
        char s[200];
        sprintf(s, "Boid %llu pos(%+4.3f,%+4.3f) vel(%+3.3f,%+3.3f) %+3.3f flags %x", index, b.pos.x, b.pos.y, b.vel.x, b.vel.y, mag(b.vel), b.flags);
        RenderMessage(debug_render, 10, 13, s);
    } 

    return accel;
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
        
        if (DEBUG_ENABLE) {
            if (boid.flags & FLAG_LEADER) SDL_SetRenderDrawColor(renderer, COLOR_LEADER, 255);
            else if (boid.flags & FLAG_HANDED) SDL_SetRenderDrawColor(renderer, COLOR_LEFT, 255);
            else SDL_SetRenderDrawColor(renderer, COLOR_RIGHT, 255);
        } else SDL_SetRenderDrawColor(renderer, COLOR_BOID, 255);
            
        SDL_RenderDrawLinesF(renderer, triangle, 3);
    }

    if (DEBUG_ENABLE) {
        calc_accel(boids, 0, renderer);
    }

}


std::default_random_engine generator;
std::uniform_real_distribution<float> rand_percent(0, 1);

void init_boids()
{
    std::uniform_real_distribution<float> rand_x(edgeObstacle, WINDOW_WIDTH - edgeObstacle);
    std::uniform_real_distribution<float> rand_y(edgeObstacle, WINDOW_HEIGHT - edgeObstacle);
    std::uniform_real_distribution<float> rand_vel(-maxSpeed, maxSpeed);
    std::uniform_int_distribution<uint8_t> rand_bit(0, 2);

    for (boid& boid : boids) {
        boid.pos.x = rand_x(generator);
        boid.pos.y = rand_y(generator);
        boid.vel.x = rand_vel(generator);
        boid.vel.y = rand_vel(generator);
        boid.flags = 0; 
        boid.flags |= rand_bit(generator) << 1; // random left/right handedness
        boid.flags |= (rand_percent(generator) < leaderChance) & 1; // random leader chance
    }
}

void MoveBoids()
{
    std::vector<boid> new_boids(boids.size());

    obstacles = { { 0, edgeObstacle, edgeObstacle, WINDOW_HEIGHT - 2 * edgeObstacle },
    { edgeObstacle, 0, WINDOW_WIDTH - 2 * edgeObstacle, edgeObstacle },
    { edgeObstacle, WINDOW_HEIGHT - edgeObstacle, WINDOW_WIDTH - 2 * edgeObstacle, edgeObstacle },
    { WINDOW_WIDTH - edgeObstacle, edgeObstacle, WINDOW_HEIGHT - 2 * edgeObstacle, WINDOW_HEIGHT - 2 * edgeObstacle } };

    for (std::size_t i = 0; i < boids.size(); i++)
    {
        vec2f accel = calc_accel(boids, i);
        boid& n = new_boids[i], b = boids[i];
        n.vel = b.vel + accel;
        n.pos = b.pos + n.vel;
        n.pos.x = wrapf(n.pos.x, WINDOW_WIDTH);
        n.pos.y = wrapf(n.pos.y, WINDOW_HEIGHT);
        n.flags = b.flags;
    }

    for (std::size_t i = 0; i < new_boids.size(); i++)
    {
        boid& n = new_boids[i];
        unsigned leader_neighbors = 0;
        for (std::size_t k = 0; k < new_boids.size(); k++)
        {
            if (k == i) continue;
            const boid& g = new_boids[k];
            if (dot(normal(n.vel), normal(g.vel)) < 0) continue; // ignore boids traveling in opposite direction
            if (dist_squared(n.pos, g.pos) <= tailwindRadius * tailwindRadius) { 
                if (g.flags & FLAG_LEADER) leader_neighbors++;
            }
        }
        if (n.flags & FLAG_LEADER) {
            // lose leadership if theres other leaders
            if (rand_percent(generator) < leader_neighbors * leaderChance) {
                n.flags &= ~FLAG_LEADER;
            }
        } else {
            // gain leadership if there are none
            if (leader_neighbors == 0 && rand_percent(generator) < leaderChance) {
                n.flags |= FLAG_LEADER;
            }
        }
    }

    boids = new_boids;
}

void mainLoop();


SDL_Renderer* sdlRenderer;
const int param_rows = 4;
const int param_cols = 5;
int param_index = 0;
float *params[param_rows * param_cols] = { &alignmentRadius, &cohesionRadius, &avoidanceRadius, &tailwindRadius, &obstacleRadius,
                                           &alignmentWeight, &cohesionWeight, &avoidanceWeight, &tailwindWeight, &obstacleWeight,
                                           &maxSpeed,        &maxAccel,       nullptr,          &dragCoeff,      &edgeObstacle, 
                                           &minSpeed,        &baseAccel,      nullptr,          nullptr,         nullptr };
float delta[param_rows * param_cols];

bool running = true;
bool advance = true;
bool step = false;


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

    sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (sdlRenderer == nullptr) {
        cleanup(window);
        logSDLError("CreateRenderer");
        SDL_Quit();
        return EXIT_FAILURE;
    }

    for (int i = 0; i < param_rows * param_cols; i++) {
        if (params[i] != nullptr) delta[i] = *params[i] * .05f; // adjust by 5% of initial value
    }

    init_boids();


    #if __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, -1, 1);
    #else
    while (running)
    {
        mainLoop();
    }
    #endif

    cleanup(sdlRenderer, window);
    SDL_Quit();
    return 0;
}


void mainLoop()
{
    // Draw the screen
    SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(sdlRenderer);

    RenderBoids(sdlRenderer);

    #if !__EMSCRIPTEN__

    #define STRINGIZE(S) #S
    #define STRING(S) STRINGIZE(S)
    #define PARAM_WIDTH 10
    #define PARAM_FMT " %" STRING(PARAM_WIDTH) "f"
    // Render parameters
    RenderMessage(sdlRenderer, 5, 5, string_format(
        "Use arrow keys and +/- to edit parameters. Press ~ to toggle debug display, R to reset all boids. A to toggle single step, space to advance\n" 
        "        ALIGNMENT   COHESION  AVOIDANCE   TAILWIND  OBSTACLE\n"
        "RADIUS" PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT "\n"
        "WEIGHT" PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT "\n"
        "            SPEED      ACCEL              DRAG          EDGE\n"
        "   MAX" PARAM_FMT PARAM_FMT "           " PARAM_FMT PARAM_FMT "\n"
        "   MIN" PARAM_FMT PARAM_FMT "\n",
        alignmentRadius, cohesionRadius, avoidanceRadius, tailwindRadius, obstacleRadius,
        alignmentWeight, cohesionWeight, avoidanceWeight, tailwindWeight, obstacleWeight,
        maxSpeed, maxAccel, dragCoeff, edgeObstacle,
        minSpeed, baseAccel
    ));
    
    SDL_Rect cursor = { (11 + (param_index % param_cols) * (PARAM_WIDTH+1)) * font_width,
                        ((7 + (param_index / param_cols) + (param_index / (2 * param_cols))) * font_height), 
                        (PARAM_WIDTH+2) * font_width, font_height };
    SDL_SetRenderDrawColor(sdlRenderer, COLOR_CURSOR, 255);
    SDL_RenderDrawRect(sdlRenderer, &cursor);
    
    #endif

    if (advance) {
        // Update boids
        MoveBoids();
        if (step) { advance = false; }
    }


    SDL_RenderPresent(sdlRenderer);


    #if !__EMSCRIPTEN__  // don't process events for the debug screen when running in web
    // process events
    SDL_Event ev;
    int next_param;
    while (SDL_PollEvent(&ev))
    {
        switch (ev.type) {
        case SDL_QUIT:
            running = false;
            break;
        case SDL_KEYDOWN:
            switch (ev.key.keysym.sym) {
            
            case SDLK_UP:    if ((next_param = param_index - param_cols) >= 0                            && params[next_param] != nullptr) param_index = next_param; break;
            case SDLK_DOWN:  if ((next_param = param_index + param_cols) < (param_cols * param_rows - 1) && params[next_param] != nullptr) param_index = next_param; break;
            case SDLK_LEFT:  if ((next_param = param_index - 1) >= 0                                     && params[next_param] != nullptr) param_index = next_param; break;
            case SDLK_RIGHT: if ((next_param = param_index + 1) < (param_cols * param_rows - 1)          && params[next_param] != nullptr) param_index = next_param; break;
            
            case SDLK_EQUALS:
            case SDLK_PLUS:  *params[param_index] += delta[param_index]; break;
            case SDLK_MINUS: *params[param_index] -= delta[param_index]; break;
            
            case SDLK_BACKQUOTE: DEBUG_ENABLE = !DEBUG_ENABLE; break;
            case SDLK_r: init_boids(); break;
            case SDLK_a: step = !step; break;
            case SDLK_SPACE: advance = true; break;
            }
            break;
        }
    }
    #endif
}
