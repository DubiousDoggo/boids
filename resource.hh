#ifndef RESOURCE_HH
#define RESOURCE_HH

#include <iostream>
#include <vector>
#include <SDL2\SDL.h>
#include "cleanup.hh"

// Apparently the c++17 filesystem library is broken on the current version of
// mingw https://sourceforge.net/p/mingw-w64/bugs/737/
/*
#include <filesystem>
using std::filesystem::path;

const path getResource(const path &resource)
{
    static path resPath;
    if (resPath.empty())
    {
        char *basePath = SDL_GetBasePath();
        if (basePath == nullptr)
        {
            std::cerr << "Error loading resources: " << SDL_GetError() << std::endl;
            return "";
        }
        resPath = path{basePath} / "res";
        SDL_free(basePath);
    }
    return resPath / resource;
}
*/

#ifdef _WIN32
#define PATH_SEPERATOR '\\'
#else
#define PATH_SEPERATOR '/'
#endif

const std::string getResource(const std::string &resource)
{
    static std::string resPath;
    if (resPath.empty())
    {
        char *basePath = SDL_GetBasePath();
        if (basePath == nullptr)
        {
            std::cout << "Error loading resources: " << SDL_GetError() << std::endl;
            return "";
        }
        resPath = std::string{basePath} + "res" + PATH_SEPERATOR;
        SDL_free(basePath);
    }
    return resPath + resource;
}

/**
 * \brief Loads an image into a texture on the rendering device
 * \param render The renderer to load the texture onto
 * \param file The image file to load
 * \return the loaded texture, or nullptr if something went wrong
 */
SDL_Texture *loadTexture(SDL_Renderer *render, const std::string &file)
{
    SDL_Texture *texture = nullptr;
    SDL_Surface *surface = SDL_LoadBMP(getResource(file).c_str());
    if (surface == nullptr)
        return nullptr;
    texture = SDL_CreateTextureFromSurface(render, surface);
    cleanup(surface);

    return texture;
}

/**
 * \brief Loads an image into a texture on the rendering device with color 
 * keyed transparency
 * \param render The renderer to load the texture onto
 * \param file The image file to load
 * \return the loaded texture, or nullptr if something went wrong
 */
SDL_Texture *loadTextureTransparent(SDL_Renderer *render, const std::string &file)
{
    SDL_Texture *texture = nullptr;
    SDL_Surface *surface = SDL_LoadBMP(getResource(file).c_str());
    if (surface == nullptr)
        return nullptr;
    SDL_SetColorKey(surface, SDL_TRUE, SDL_MapRGB(surface->format, 0x00, 0x00, 0xFF));
    texture = SDL_CreateTextureFromSurface(render, surface);
    cleanup(surface);

    return texture;
}

#endif