
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
#include "vec.hh"
#include "util.hh"
#include "render.hh"

/* Define window size */
const int WINDOW_WIDTH = 1920;
const int WINDOW_HEIGHT = 1080;

unsigned DEBUG_ENABLE = 0;

float minSpeed = 2;
float maxSpeed = 12;
float maxAccel = 5.7f;
float baseAccel = 0.6f;
float dragCoeff = 0.004f;

float flockAngle = M_PI / 4;

float alignmentRadius = 96; 
float cohesionRadius = 100;
float avoidanceRadius = 25;
float flockRadius = 150; 
float obstacleRadius = 200;

float edgeObstacle = 100;

float alignmentWeight = 0.05f;
float cohesionWeight = 0.005f;
float avoidanceWeight = 0.05f;
float flockWeight = 0.02f;
float obstacleWeight = 0.01f;

float leaderChance = 0.01;
float handedChance = 0.001;

const int boid_size = 10;

const float debugVecMultiplier = 200;

#define COLOR_CURSOR    255, 255, 255  // WHITE
#define COLOR_VELOCITY  255,   0, 255  // PURPLE
#define COLOR_ALIGNMENT  32,  64, 255  // BLUE
#define COLOR_COHESION    0, 240,   0  // GREEN
#define COLOR_AVOIDANCE 255,   0,   0  // RED
#define COLOR_FLOCK     255, 255,  64  // YELLOW
#define COLOR_ACCEL       0, 255, 255  // CYAN
#define COLOR_OBSTACLE  255, 128,   0  // ORANGE
#define COLOR_DRAG      128, 128, 128  // GREY

#define COLOR_SKY      0, 128, 255 // SKY BLUE
#define COLOR_GROUND  64, 200,   0 // GRASS GREEN 
#define COLOR_BOID   255, 255, 255 // WHITE
#define COLOR_LEADER   0, 160, 200 // DEEP BLUE
#define COLOR_LEFT     0, 240,   0 // GREEN
#define COLOR_RIGHT  255,   0,   0 // RED

#define FLAG_LEADER 0b001
#define FLAG_HANDED 0b010

enum state { 
    FLYING, 
    TUMBLE, 
    STUNED, 
    WALKIN,
};

struct boid { 
    vec2f pos, vel, dir; 
    unsigned flags;
    state state;
    unsigned state_timer;
};

std::vector<boid> boids(100);
std::vector<SDL_FRect> obstacles;


/** calculate the acceleration of the boid at index, based on neighboring boids */
vec2f calc_accel(const std::vector<boid>& boids, std::size_t index, SDL_Renderer* debug_render = nullptr)
{
    const boid& b = boids[index];

    vec2f alignment_vec{ 0, 0 }, cohesion_vec{ 0, 0 }, avoidance_vec{ 0, 0 }, flock_vec{ 0, 0 };
    int alignment_count = 0, cohesion_count = 0, avoidance_count = 0, flock_count = 0;
    
    for (std::size_t k = 0; k < boids.size(); k++)
    {
        if (k == index) continue;
        
        const boid& g = boids[k];
 
        if (g.state != FLYING) continue;
    
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
        if (dist_squared(b.pos, g.pos) <= flockRadius * flockRadius) { 

            if (!(g.flags & FLAG_LEADER)) continue; // only flock on leaders
            if (dot(b.dir, g.dir) < 0) continue; // ignore boids not traveling in the same direction

            vec2f tail = -g.dir; // vec backwards from g
            vec2f g_to_b = b.pos - g.pos;  // vec from g to b
            vec2f rej;
            if (dot(g_to_b, g.dir) > 0) { // fall behind leader
                vec2f p = proj(g_to_b, tail);
                rej = p;
            } else { // attempt to make a triangular looking flock
                float rot = flockAngle * (b.flags & FLAG_HANDED ? 1 : -1);
                tail = rotate(tail, rot); // rotate tail vector in handedness of b
                vec2f p = proj(g_to_b, tail);
                rej = g_to_b - p; // rejection from tail vector to b
            }

            float tmp = mag(rej) - .75 * flockRadius;
            rej = normal(rej) * ((-1 / (.75 * flockRadius)) * tmp * tmp + (flockRadius * .75));
            flock_vec += -rej; // move b towards tail vector
            flock_count++;

            if (debug_render != nullptr)
            {
                SDL_SetRenderDrawColor(debug_render, COLOR_LEADER, 255);
                RenderVec(debug_render, g.pos, tail * flockRadius);
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
    if (flock_count > 0) {
        flock_vec /= flock_count;
        flock_vec *= flockWeight;
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
    vec2f accel = alignment_vec + cohesion_vec + avoidance_vec + flock_vec + obstacle_vec + drag_vec + base_vec; 
    
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
        
        SDL_SetRenderDrawColor(debug_render, COLOR_FLOCK, 255);
        RenderVec(debug_render, b.pos, flock_vec * debugVecMultiplier);
        RenderCircle(debug_render, flockRadius, b.pos);
        
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
           
        std::string s = string_format("Boid %zu pos(%+4.3f,%+4.3f) vel(%+3.3f,%+3.3f) %+3.3f flags %x state %d state timer %u", 
                                       index, b.pos.x, b.pos.y, b.vel.x, b.vel.y, mag(b.vel), b.flags, b.state, b.state_timer);
        RenderMessage(debug_render, 10, 13, s);
    } 

    return accel;
}

void RenderBoids(SDL_Renderer* renderer)
{
    for (std::size_t i = 0; i < boids.size(); i++)
    {
        const boid& boid = boids[i];
        const vec2f& direction = boid.dir;
        SDL_FPoint triangle[3]{ {boid.pos.x + boid_size * (-direction.y - direction.x), boid.pos.y + boid_size * (direction.x - direction.y)},
                                {boid.pos.x + boid_size * direction.x , boid.pos.y + boid_size * direction.y },
                                {boid.pos.x + boid_size * (direction.y - direction.x), boid.pos.y + boid_size * (-direction.x - direction.y)} };
        
        if (DEBUG_ENABLE == 2) {
            if (boid.flags & FLAG_LEADER) SDL_SetRenderDrawColor(renderer, COLOR_LEADER, 255);
            else if (boid.flags & FLAG_HANDED) SDL_SetRenderDrawColor(renderer, COLOR_LEFT, 255);
            else SDL_SetRenderDrawColor(renderer, COLOR_RIGHT, 255);
        } else SDL_SetRenderDrawColor(renderer, COLOR_BOID, 255);
            
        SDL_RenderDrawLinesF(renderer, triangle, 3);

        if (boid.state == STUNED) {
            const float star_radius = 2;
            const unsigned star_count = 3;
            SDL_FRect stars[star_count];
            vec2f center = boid.pos + vec2f{0, -boid_size * 1.5f};
            
            for (unsigned i = 0; i < star_count; i++) {
                float angle = boid.state_timer + i * 2 * M_PI / star_count;
                vec2f pos = rotate({0, boid_size}, angle);
                pos.y /= 2; pos += center;
                stars[i] = SDL_FRect { pos.x - star_radius, pos.y - star_radius, star_radius, star_radius };
            }    

            SDL_RenderFillRectsF(renderer, stars, star_count);

        }

    }

    if (DEBUG_ENABLE == 2) {
        calc_accel(boids, 0, renderer);
    }

}


std::default_random_engine generator;
std::uniform_real_distribution<float> rand_percent(0, 1);

void InitBoids()
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
        boid.dir = normal(boid.vel);
        boid.flags = 0; 
        boid.flags |= rand_bit(generator) << 1; // random left/right handedness
        boid.flags |= (rand_percent(generator) < leaderChance) & 1; // random leader chance
        boid.state_timer = 0;
        boid.state = FLYING;
    }
}

void UpdateBoids()
{
    std::vector<boid> old_boids(boids);

    obstacles = { { 0, edgeObstacle, edgeObstacle, WINDOW_HEIGHT - 2 * edgeObstacle },
    { edgeObstacle, 0, WINDOW_WIDTH - 2 * edgeObstacle, edgeObstacle },
    { edgeObstacle, WINDOW_HEIGHT - edgeObstacle, WINDOW_WIDTH - 2 * edgeObstacle, edgeObstacle },
    { WINDOW_WIDTH - edgeObstacle, edgeObstacle, WINDOW_HEIGHT - 2 * edgeObstacle, WINDOW_HEIGHT - 2 * edgeObstacle } };

    
    for (std::size_t i = 0; i < boids.size(); i++)
    {
        boid& n = boids[i];

        if (n.state_timer) { --n.state_timer; } 
        
        switch (n.state) {
          
            case FLYING: {
                // update position and velocity
                vec2f accel = calc_accel(old_boids, i);
                n.vel += accel;

                if (n.pos.y < 0 && n.vel.y < 0)
                    n.vel.y = -n.vel.y;

                n.pos += n.vel;
                n.dir = normal(n.vel);

                if (!n.state_timer) {
                    if (n.pos.y > WINDOW_HEIGHT - edgeObstacle) {
                        n.state = TUMBLE;
                        n.state_timer = 120; 
                    }
                }
                break;
            }
            case TUMBLE: {

                const float friction_coeff = 0.2;
                n.vel -= n.vel * friction_coeff;
                n.pos += n.vel;              
                
                // if a circle of radius r rolls a distance d, it has rotated d / r radians
                n.dir = rotate(n.dir, n.vel.x / boid_size); 
            
                if (!n.state_timer || mag(n.vel) < 0.5) {
                    n.state = STUNED;
                    n.state_timer = 120;
                }
                break; 
            }
            case STUNED:
                
                if (!n.state_timer) {
                    n.vel = n.dir;
                    n.state = WALKIN;
                    n.state_timer = 30;
                }
                break;

            case WALKIN: {

                n.vel += {0, -0.05};                
                n.pos += n.vel;
                n.dir = normal(n.vel);

                if (n.pos.y <= WINDOW_HEIGHT - edgeObstacle) {
                    n.state = FLYING;
                    n.state_timer = 5; // ground invuln time
                }
                break;
            }
        }

        n.pos.x = wrap<float>(n.pos.x, WINDOW_WIDTH);
        
    }
    
    // update flags
    for (std::size_t i = 0; i < boids.size(); i++)
    {
        boid& n = boids[i];
        if (n.state != FLYING) continue;
        unsigned leader_neighbors = 0;
        int handed_disparity = 0;
        
        for (std::size_t k = 0; k < boids.size(); k++)
        {
            if (k == i) continue;
            const boid& g = boids[k];
            if (g.state != FLYING) continue;

            if (dot(n.dir, g.dir) < 0) continue; // ignore boids traveling in opposite direction
            if (dist_squared(n.pos, g.pos) <= flockRadius * flockRadius) { 
                if (g.flags & FLAG_LEADER) leader_neighbors++;
                handed_disparity += g.flags & FLAG_HANDED ? 1 : -1;
            }
        }
        
        if (n.flags & FLAG_HANDED) {
            if (handed_disparity > 0) {
                if (rand_percent(generator) < handed_disparity * handedChance) {
                    n.flags &= ~FLAG_HANDED;   
                }
            }
        } else {
            if (handed_disparity < 0) {
                if (rand_percent(generator) < -handed_disparity * handedChance) {
                    n.flags |= FLAG_HANDED;   
                }
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

}


void mainLoop();

SDL_Renderer* sdlRenderer;
SDL_Texture* targetTexture;

const int param_rows = 4;
const int param_cols = 5;
int param_index = 0;
float *params[param_rows * param_cols] = { &alignmentRadius, &cohesionRadius, &avoidanceRadius, &flockRadius, &obstacleRadius,
                                           &alignmentWeight, &cohesionWeight, &avoidanceWeight, &flockWeight, &obstacleWeight,
                                           &maxSpeed,        &maxAccel,       nullptr,          &dragCoeff,      &edgeObstacle, 
                                           &minSpeed,        &baseAccel,      nullptr,          nullptr,         nullptr };
float delta[param_rows * param_cols];

bool running = true;
bool advance = true;
bool step = false;
bool follow = false;

int zoom = 0;
vec2f zoom_pos = {0, 0};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
int main(int argc, char* argv[]) { // args are required for SDL_main
#pragma GCC diagnostic pop

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

    sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (sdlRenderer == nullptr) {
        cleanup(window);
        logSDLError("CreateRenderer");
        SDL_Quit();
        return EXIT_FAILURE;
    }

    targetTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (targetTexture == nullptr) {
        cleanup(window, sdlRenderer);
        logSDLError("CreateTexture");
        SDL_Quit();
        return EXIT_FAILURE;
    }

    for (int i = 0; i < param_rows * param_cols; i++) {
        if (params[i] != nullptr) delta[i] = *params[i] * .05f; // adjust by 5% of initial value
    }

    InitBoids();


    #if __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, -1, 1);
    #else
    while (running) mainLoop();
    #endif

    cleanup(sdlRenderer, window, targetTexture);
    SDL_Quit();
    return 0;
}


void mainLoop()
{
    if (advance) {
        // Update boids
        UpdateBoids();
        if (step) { advance = false; }
    }

    // Draw the screen
    SDL_SetRenderTarget(sdlRenderer, targetTexture);

    if (DEBUG_ENABLE == 2) SDL_SetRenderDrawColor(sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);    
    else SDL_SetRenderDrawColor(sdlRenderer, COLOR_SKY, SDL_ALPHA_OPAQUE);    
    SDL_RenderClear(sdlRenderer);

    SDL_FRect ground = { 0, WINDOW_HEIGHT - edgeObstacle, WINDOW_WIDTH, edgeObstacle };
    SDL_SetRenderDrawColor(sdlRenderer, COLOR_GROUND, SDL_ALPHA_OPAQUE);    
    SDL_RenderFillRectF(sdlRenderer, &ground);

    RenderBoids(sdlRenderer);

    #if !__EMSCRIPTEN__ // don't display debug parameters in web

    if(DEBUG_ENABLE) {
        #define STRINGIZE(S) #S
        #define STRING(S) STRINGIZE(S)
        #define PARAM_WIDTH 10
        #define PARAM_FMT " %" STRING(PARAM_WIDTH) "f"
        // Render parameters
        RenderMessage(sdlRenderer, 5, 5, string_format(
            "Use arrow keys and +/- to edit parameters. Press ~ to toggle debug display, R to reset all boids. A to toggle single step, space to advance\n" 
            "        ALIGNMENT   COHESION  AVOIDANCE   FLOCK  OBSTACLE\n"
            "RADIUS" PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT "\n"
            "WEIGHT" PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT PARAM_FMT "\n"
            "            SPEED      ACCEL              DRAG          EDGE\n"
            "   MAX" PARAM_FMT PARAM_FMT "           " PARAM_FMT PARAM_FMT "\n"
            "   MIN" PARAM_FMT PARAM_FMT "\n",
            alignmentRadius, cohesionRadius, avoidanceRadius, flockRadius, obstacleRadius,
            alignmentWeight, cohesionWeight, avoidanceWeight, flockWeight, obstacleWeight,
            maxSpeed, maxAccel, dragCoeff, edgeObstacle,
            minSpeed, baseAccel
        ));
    
        SDL_Rect cursor = { (11 + (param_index % param_cols) * (PARAM_WIDTH+1)) * font_width,
                            ((7 + (param_index / param_cols) + (param_index / (2 * param_cols))) * font_height), 
                            (PARAM_WIDTH+2) * font_width, 
                            font_height };
        SDL_SetRenderDrawColor(sdlRenderer, COLOR_CURSOR, 255);
        SDL_RenderDrawRect(sdlRenderer, &cursor);
    }
    #endif

    SDL_SetRenderTarget(sdlRenderer, nullptr);
    const float zoom_sens = 0.25f;
    float zoom_mul = std::pow(2.f, -zoom * zoom_sens);
    if (follow) {
        zoom_pos.x = boids[0].pos.x - WINDOW_WIDTH * zoom_mul / 2;
        zoom_pos.y = boids[0].pos.y - WINDOW_HEIGHT * zoom_mul / 2;
    }

    SDL_Rect zoom_window = { static_cast<int>(zoom_pos.x),
                             static_cast<int>(zoom_pos.y),
                             static_cast<int>(WINDOW_WIDTH * zoom_mul),
                             static_cast<int>(WINDOW_HEIGHT * zoom_mul) };
    SDL_RenderCopy(sdlRenderer, targetTexture, &zoom_window, nullptr);
    SDL_RenderPresent(sdlRenderer);

    // process events
    #if !__EMSCRIPTEN__  // don't process events for the debug screen when running in web
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
            
            case SDLK_BACKQUOTE: DEBUG_ENABLE = (DEBUG_ENABLE + 1) % 3; break;
            
            case SDLK_UP:    if ((next_param = param_index - param_cols) >= 0                            && params[next_param] != nullptr) param_index = next_param; break;
            case SDLK_DOWN:  if ((next_param = param_index + param_cols) < (param_cols * param_rows - 1) && params[next_param] != nullptr) param_index = next_param; break;
            case SDLK_LEFT:  if ((next_param = param_index - 1) >= 0                                     && params[next_param] != nullptr) param_index = next_param; break;
            case SDLK_RIGHT: if ((next_param = param_index + 1) < (param_cols * param_rows - 1)          && params[next_param] != nullptr) param_index = next_param; break;
            
            case SDLK_EQUALS:
            case SDLK_PLUS:  *params[param_index] += delta[param_index]; break;
            case SDLK_MINUS: *params[param_index] -= delta[param_index]; break;
            
            case SDLK_r: InitBoids(); break;
            case SDLK_a: step = !step; break;
            case SDLK_f: follow = !follow; break;
            case SDLK_SPACE: advance = true; break;
            }
            break;
        case SDL_MOUSEWHEEL: {
            
            float prev_zoom_mul = std::pow(2.f, -zoom * zoom_sens);
            zoom += ev.wheel.y;
            if (zoom < 0) zoom = 0;
            float zoom_mul = std::pow(2.f, -zoom * zoom_sens);
            float zoom_diff = prev_zoom_mul - zoom_mul;
            
            if (!follow) {
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);            
                zoom_pos.y += zoom_diff * static_cast<float>(mouseY);
                zoom_pos.x += zoom_diff * static_cast<float>(mouseX);
                zoom_pos.y = clamp(zoom_pos.y, 0.f, WINDOW_HEIGHT * (1 - zoom_mul));
                zoom_pos.x = clamp(zoom_pos.x, 0.f, WINDOW_WIDTH * (1 - zoom_mul));
            }

            break;
        }
            
        }
    }
    #endif
}
