// main.cpp (cleaned + corrected for compile + consistent iso/object math)

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "tinyxml2.h"

// Engine modules
#include "SpriteRenderer.h"
#include "Camera2d.h"
#include "TileMap.h"
#include "RenderQueue.h"
#include "TileResolver.h"
#include "TileSet.h"
#include "TileMath.h"
#include "Player.h"
#include "PlayerController.h"
#include "SpriteSheet.h"
#include "TmxLoader.h"

// stb_image (ONLY define implementation in ONE .cpp file — main.cpp is a good choice)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/*
    Texture2D
    ---------
    Small helper so we can keep:
      - OpenGL texture ID
      - the original image width/height (needed for atlas UV calculations)
*/
struct Texture2D
{
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

/*
    ============================================
    Shaders (SpriteRenderer expects these uniforms)
    ============================================
*/
static const char* vertexShaderSrc = R"(
#version 330 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTex;

out vec2 TexCoord;

uniform mat4 uProjection;
uniform mat4 uModel;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 0.0, 1.0);
    gl_Position = uProjection * worldPos;
    TexCoord = aTex;
}
)";

static const char* fragmentShaderSrc = R"(
#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uTexture;

void main()
{
    FragColor = texture(uTexture, TexCoord);
}
)";

/*
    ============================================
    GLFW callbacks
    ============================================
*/
static void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    glViewport(0, 0, width, height);
}

/*
    ============================================
    Iso helpers
    ============================================
*/

// MUST match TileMap.cpp: mapOrigin(viewportW * 0.5f, 60.0f)
static glm::vec2 ComputeMapOrigin(int viewportW)
{
    return glm::vec2(viewportW * 0.5f, 60.0f);
}

// Grid -> iso tile TOP-LEFT (engine world)
static glm::vec2 GridToIsoTopLeft(const glm::vec2& gridPos, int tileW, int tileH, const glm::vec2& mapOrigin)
{
    const float halfW = tileW * 0.5f;
    const float halfH = tileH * 0.5f;
    const float isoX = (gridPos.x - gridPos.y) * halfW;
    const float isoY = (gridPos.x + gridPos.y) * halfH;
    return glm::vec2(isoX, isoY) + mapOrigin;
}

// iso TOP-LEFT pixels (in iso space) -> grid (for debugging)
static glm::vec2 IsoTopLeftPixelsToGrid(const glm::vec2& isoTopLeftPx, int tileW, int tileH)
{
    const float halfW = tileW * 0.5f;
    const float halfH = tileH * 0.5f;

    const float gridX = (isoTopLeftPx.x / halfW + isoTopLeftPx.y / halfH) * 0.5f;
    const float gridY = (isoTopLeftPx.y / halfH - isoTopLeftPx.x / halfW) * 0.5f;
    return { gridX, gridY };
}


/*
    ============================================
    Misc helpers
    ============================================
*/
static glm::vec2 SpawnPixelToGrid_Ortho(const glm::vec2& posPx, int tileW, int tileH)
{
    return glm::vec2(posPx.x / tileW, posPx.y / tileH);
}

static bool PointInRect(const glm::vec2& p, const glm::vec2& rPos, const glm::vec2& rSize)
{
    return (p.x >= rPos.x && p.x <= rPos.x + rSize.x &&
        p.y >= rPos.y && p.y <= rPos.y + rSize.y);
}

/*
    ============================================
    Shader compile/link helpers
    ============================================
*/
static unsigned int CompileShader(GLenum type, const char* src)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "Shader compile error:\n" << infoLog << "\n";
    }
    return shader;
}

static unsigned int CreateProgram(const char* vsSrc, const char* fsSrc)
{
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "Program link error:\n" << infoLog << "\n";
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

/*
    ============================================
    Texture loading
    ============================================
*/
static Texture2D LoadTextureRGBA(const char* path, bool flipY)
{
    Texture2D tex{};

    stbi_set_flip_vertically_on_load(flipY);

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int channels = 0;
    unsigned char* data = stbi_load(path, &tex.width, &tex.height, &channels, 4);
    if (!data)
    {
        std::cerr << "Failed to load texture '" << path << "': " << stbi_failure_reason() << "\n";
        tex.id = 0;
        return tex;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    return tex;
}

int main()
{
    /*
    ============================================
    GLFW + window
    ============================================
    */
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Myth Client", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    /*
    ============================================
    GLAD
    ============================================
    */
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "Working directory: " << std::filesystem::current_path() << "\n";

    LoadedMap loadedMap;
    if (!LoadTmxMap("assets/maps/StarterZone.tmx", loadedMap))
    {
        glfwTerminate();
        return -1;
    }

    int tileW = loadedMap.mapData.tileW;
    int tileH = loadedMap.mapData.tileH;
    int mapW = loadedMap.mapData.width;
    int mapH = loadedMap.mapData.height;

    /*
    ============================================
    Global render state
    ============================================
    */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /*
    ============================================
    Shader program
    ============================================
    */
    GLuint shaderProgram = CreateProgram(vertexShaderSrc, fragmentShaderSrc);
    if (!shaderProgram)
    {
        glfwTerminate();
        return -1;
    }

    /*
    ============================================
    Load tilesets + build resolver
    ============================================
    */
    std::vector<Texture2D> tilesetTextures;          // sheet-based textures (optional debug)
    std::vector<TilesetRuntime> tilesetRuntimes;     // runtime tileset defs + texture ids

    auto LoadTilesetsForMap = [&](const LoadedMap& mapData) -> bool
        {
            tilesetTextures.clear();
            tilesetRuntimes.clear();

            tilesetTextures.reserve(mapData.mapData.tilesets.size());
            tilesetRuntimes.reserve(mapData.mapData.tilesets.size());

            std::unordered_map<std::string, Texture2D> textureCache;

            auto LoadCachedTexture = [&](const std::string& path, bool flipY) -> Texture2D
                {
                    const std::string cacheKey = path + (flipY ? "|flip" : "|noflip");
                    auto it = textureCache.find(cacheKey);
                    if (it != textureCache.end())
                        return it->second;

                    Texture2D texture = LoadTextureRGBA(path.c_str(), flipY);
                    if (texture.id)
                        textureCache.emplace(cacheKey, texture);

                    return texture;
                };

            // Important: your renderer/shader UVs expect "normal" orientation.
            const bool tilesetFlipY = false;

            for (const TilesetDef& tilesetDef : mapData.mapData.tilesets)
            {
                if (tilesetDef.isImageCollection)
                {
                    // Image collection: each tileId is its own texture.
                    TileSet tileset(0, 0, tilesetDef.tileW, tilesetDef.tileH);
                    tileset.SetAnimations(tilesetDef.animations);

                    TilesetRuntime runtime{ tilesetDef, tileset, 0 };

                    for (const auto& entry : tilesetDef.tileImages)
                    {
                        const int tileId = entry.first;
                        const std::string& imagePath = entry.second.path;

                        Texture2D texture = LoadCachedTexture(imagePath, tilesetFlipY);
                        if (!texture.id)
                        {
                            std::cerr << "Failed to load tileset tile image: " << imagePath << "\n";
                            return false;
                        }

                        runtime.tileTextures.emplace(tileId, texture.id);
                    }

                    tilesetRuntimes.push_back(std::move(runtime));
                }
                else
                {
                    // Sheet-based tileset: one atlas texture.
                    Texture2D texture = LoadCachedTexture(tilesetDef.imagePath, tilesetFlipY);
                    if (!texture.id)
                    {
                        std::cerr << "Failed to load tileset image: " << tilesetDef.imagePath << "\n";
                        return false;
                    }

                    tilesetTextures.push_back(texture);

                    TileSet tileset(texture.width, texture.height, tilesetDef.tileW, tilesetDef.tileH);
                    tileset.SetAnimations(tilesetDef.animations);

                    TilesetRuntime runtime{ tilesetDef, tileset, texture.id };
                    tilesetRuntimes.push_back(std::move(runtime));
                }
            }

            return true;
        };

    if (!LoadTilesetsForMap(loadedMap))
    {
        glfwTerminate();
        return -1;
    }

    TileResolver tileResolver(tilesetRuntimes);

    /*
    ============================================
    Load player sprite sheet texture
    ============================================
    */
    Texture2D playerSheetTex = LoadTextureRGBA("assets/Playersprite/player_sheet.png", false);
    if (!playerSheetTex.id)
    {
        std::cerr << "Failed to load assets/Playersprite/player_sheet.png\n";
        glfwTerminate();
        return -1;
    }

    // IMPORTANT: fix the broken constructor lines you had ("true;" dangling)
    SpriteSheet playerSheet(
        playerSheetTex.width,
        playerSheetTex.height,
        256,   // frameW
        314,   // frameH
        false  // flipY?
    );

    /*
    ============================================
    Create renderer + camera
    ============================================
    */
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    SpriteRenderer renderer(shaderProgram, fbW, fbH);
    Camera2D camera({ 0.0f, 0.0f });

    /*
    ============================================
    Create player
    ============================================
    */
    Player player(playerSheetTex.id, { 5, 5 }, { 256.0f, 314.0f });
    player.SetGridPos({ 5.0f, 5.0f });
    player.SetSpriteSheet(playerSheet);
    player.SetFrame(0);

    PlayerController playerController(player);

    /*
    ============================================
    Collision grid
    ============================================
    */
    std::vector<int> collisionGrid(loadedMap.mapData.collision.begin(), loadedMap.mapData.collision.end());

    auto SpawnPlayerFromMap = [&](const LoadedMap& mapData, const std::string& spawnName) -> bool
        {
            // Named SpawnDef (from object type Spawn in TMX loader)
            if (!mapData.mapData.spawns.empty())
            {
                for (const SpawnDef& spawn : mapData.mapData.spawns)
                {
                    if (spawnName.empty() || spawn.name == spawnName)
                    {
                        // NOTE: This is orthographic conversion; if your spawn positions are isometric,
                        // use ObjectPixelsToGrid(...) instead (you already have that in TmxLoader).
                        glm::vec2 spawnGrid = SpawnPixelToGrid_Ortho(spawn.posPx, tileW, tileH);

                        spawnGrid.x = std::clamp(spawnGrid.x, 0.0f, (float)(mapW - 1));
                        spawnGrid.y = std::clamp(spawnGrid.y, 0.0f, (float)(mapH - 1));
                        player.SetGridPos(spawnGrid);

                        std::cout << "Player spawn from named spawn '" << spawn.name
                            << "' grid=(" << spawnGrid.x << "," << spawnGrid.y << ")\n";
                        return true;
                    }
                }
            }

            // Fallback: look for object named/type PlayerSpawn/Player
            for (const MapObject& object : mapData.mapData.objects)
            {
                std::string lowerName = object.name;
                std::string lowerType = object.type;

                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                    [](unsigned char c) { return (char)std::tolower(c); });
                std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
                    [](unsigned char c) { return (char)std::tolower(c); });

                if (lowerName == "playerspawn" || lowerType == "playerspawn" ||
                    lowerName == "player" || lowerType == "player")
                {
                    // This uses your iso-aware helper from TmxLoader.cpp
                    glm::vec2 spawnGrid = ObjectPixelsToGrid(object.positionPx, tileW, tileH);

                    spawnGrid.x = std::clamp(spawnGrid.x, 0.0f, (float)(mapW - 1));
                    spawnGrid.y = std::clamp(spawnGrid.y, 0.0f, (float)(mapH - 1));
                    player.SetGridPos(spawnGrid);

                    std::cout << "Player spawn from object id=" << object.id
                        << " grid=(" << spawnGrid.x << "," << spawnGrid.y << ")\n";
                    return true;
                }
            }

            return false;
        };

    SpawnPlayerFromMap(loadedMap, "");

    auto IsBlockedAt = [&](int tx, int ty) -> bool
        {
            if (tx < 0 || tx >= mapW || ty < 0 || ty >= mapH) return true;
            return collisionGrid[ty * mapW + tx] != 0;
        };

    {
        int spX = (int)std::floor(player.GetGridPos().x);
        int spY = (int)std::floor(player.GetGridPos().y);
        std::cout << "Spawn tile = (" << spX << "," << spY << ") collision=" << (IsBlockedAt(spX, spY) ? 1 : 0) << "\n";
    }

    auto FindFirstWalkable = [&]() -> glm::vec2
        {
            for (int y = 0; y < mapH; ++y)
                for (int x = 0; x < mapW; ++x)
                    if (collisionGrid[y * mapW + x] == 0)
                        return glm::vec2((float)x + 0.5f, (float)y + 0.5f);

            return glm::vec2(1.0f, 1.0f);
        };

    {
        glm::vec2 start = player.GetGridPos();
        int sx = (int)std::floor(start.x);
        int sy = (int)std::floor(start.y);

        if (sx < 0 || sx >= mapW || sy < 0 || sy >= mapH || collisionGrid[sy * mapW + sx] != 0)
        {
            glm::vec2 newPos = FindFirstWalkable();
            std::cout << "Spawn blocked, moving player to walkable tile at " << newPos.x << "," << newPos.y << "\n";
            player.SetGridPos(newPos);
        }
    }

    /*
    ============================================
    Create TileMaps (layers)
    ============================================
    */
    auto MakeTileLayer = [&](const std::vector<uint32_t>& tiles)
        {
            if ((int)tiles.size() == mapW * mapH) return tiles;
            return std::vector<uint32_t>(mapW * mapH, 0);
        };

    TileMap groundMap(mapW, mapH, tileW, tileH);
    TileMap wallsMap(mapW, mapH, tileW, tileH);
    TileMap overheadMap(mapW, mapH, tileW, tileH);

    groundMap.AddLayer("Ground", MakeTileLayer(loadedMap.mapData.groundGids), true, true);
    wallsMap.AddLayer("Walls", MakeTileLayer(loadedMap.mapData.wallsGids), true, true);
    overheadMap.AddLayer("Overhead", MakeTileLayer(loadedMap.mapData.overheadGids), true, true);

    /*
    ============================================
    Map changing
    ============================================
    */
    auto ChangeMap = [&](const std::string& path, const std::string& spawnName) -> bool
        {
            LoadedMap newMap;
            if (!LoadTmxMap(path, newMap))
                return false;

            if (!LoadTilesetsForMap(newMap))
                return false;

            loadedMap = std::move(newMap);

            tileW = loadedMap.mapData.tileW;
            tileH = loadedMap.mapData.tileH;
            mapW = loadedMap.mapData.width;
            mapH = loadedMap.mapData.height;

            collisionGrid.assign(loadedMap.mapData.collision.begin(), loadedMap.mapData.collision.end());

            groundMap = TileMap(mapW, mapH, tileW, tileH);
            wallsMap = TileMap(mapW, mapH, tileW, tileH);
            overheadMap = TileMap(mapW, mapH, tileW, tileH);

            groundMap.AddLayer("Ground", MakeTileLayer(loadedMap.mapData.groundGids), true, true);
            wallsMap.AddLayer("Walls", MakeTileLayer(loadedMap.mapData.wallsGids), true, true);
            overheadMap.AddLayer("Overhead", MakeTileLayer(loadedMap.mapData.overheadGids), true, true);

            if (!SpawnPlayerFromMap(loadedMap, spawnName))
                player.SetGridPos({ 5.0f, 5.0f });

            camera.SetPosition({ 0.0f, 0.0f });
            return true;
        };

    /*
    ============================================
    Main loop
    ============================================
    */
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // deltaTime
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        static float animationTimeMs = 0.0f;
        animationTimeMs += deltaTime * 1000.0f;

        // framebuffer / projection updates
        glfwGetFramebufferSize(window, &fbW, &fbH);
        renderer.SetScreenSize(fbW, fbH);

        // clear
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // input/movement
        playerController.Update(window, deltaTime, mapW, mapH, collisionGrid);

        // Door trigger (press E inside door rect) — NOTE: this assumes door rect + feet are same space (may need iso conversion later)
        static bool wasE = false;
        bool eDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
        bool ePressed = eDown && !wasE;
        wasE = eDown;

        glm::vec2 playerGrid = player.GetGridPos();
        glm::vec2 playerPixelFeet(playerGrid.x * tileW + tileW * 0.5f,
            playerGrid.y * tileH + tileH * 1.0f);

        const DoorDef* activeDoor = nullptr;
        for (const DoorDef& door : loadedMap.mapData.doors)
        {
            if (PointInRect(playerPixelFeet, door.posPx, door.sizePx))
            {
                activeDoor = &door;
                break;
            }
        }

        if (activeDoor && ePressed)
        {
            if (!ChangeMap(activeDoor->targetMap, activeDoor->targetSpawn))
                std::cerr << "Failed to change map to " << activeDoor->targetMap << "\n";
        }

        // Camera follow (dead-zone + smoothing)
        const glm::vec2 mapOrigin = ComputeMapOrigin(fbW);
        const float halfW = tileW * 0.5f;
        const float halfH = tileH * 0.5f;

        // Player iso tile top-left
        glm::vec2 playerTileTopLeft = GridToIsoTopLeft(playerGrid, tileW, tileH, mapOrigin);

        // Player feet point (bottom-center of the tile)
        glm::vec2 playerWorldFeet = playerTileTopLeft + glm::vec2(tileW * 0.5f, (float)tileH);

        glm::vec2 camPos = camera.GetPosition();
        glm::vec2 halfView((float)fbW * 0.5f, (float)fbH * 0.5f);
        glm::vec2 camCenter = camPos + halfView;

        glm::vec2 deadZoneHalf(80.0f, 60.0f);
        glm::vec2 desiredCenter = camCenter;
        glm::vec2 delta = playerWorldFeet - camCenter;

        if (delta.x > deadZoneHalf.x) desiredCenter.x = playerWorldFeet.x - deadZoneHalf.x;
        if (delta.x < -deadZoneHalf.x) desiredCenter.x = playerWorldFeet.x + deadZoneHalf.x;
        if (delta.y > deadZoneHalf.y) desiredCenter.y = playerWorldFeet.y - deadZoneHalf.y;
        if (delta.y < -deadZoneHalf.y) desiredCenter.y = playerWorldFeet.y + deadZoneHalf.y;

        glm::vec2 targetCamPos = desiredCenter - halfView;

        const float cameraFollowStrength = 10.0f;
        float lerpT = 1.0f - std::exp(-cameraFollowStrength * deltaTime);
        camPos = camPos + (targetCamPos - camPos) * lerpT;
        camera.SetPosition(camPos);

        /*
        ============================================
        Draw world
        ============================================
        */
        groundMap.DrawGround(renderer, tileResolver, camera, { fbW, fbH }, animationTimeMs);

        RenderQueue renderQueue;
        renderQueue.Clear();
        renderQueue.Reserve(2048);

        // Walls go into queue for depth sort
        wallsMap.AppendOccluders(renderQueue, tileResolver, camera, { fbW, fbH }, animationTimeMs);

        /*
        ============================================
        Tile objects from TMX (image collection trees, etc.)
        TMX x,y is in Tiled's shifted iso pixel space.
        ============================================
        */
        for (const MapObjectInstance& instance : loadedMap.mapData.objectInstances)
        {
            ResolvedTile resolved{};
            if (!tileResolver.Resolve(instance.tileIndex, animationTimeMs, resolved))
                continue;

            glm::vec2 drawSize = instance.size;
            if (drawSize.x <= 0.0f || drawSize.y <= 0.0f)
                drawSize = resolved.sizePx;
            if (drawSize.x <= 0.0f || drawSize.y <= 0.0f)
                continue;

            const float halfW = tileW * 0.5f;
            const glm::vec2 tiledIsoUnshift(-(loadedMap.mapData.height - 1) * halfW, 0.0f);

            // Candidate A: treat TMX x,y as bottom-center (expected when objectalignment=bottom)
            glm::vec2 bottomCenterWorld = mapOrigin + tiledIsoUnshift + instance.worldPos;

            // Candidate B: treat TMX x,y as bottom-left (common when objects were placed weirdly)
            glm::vec2 bottomLeftWorld = mapOrigin + tiledIsoUnshift + instance.worldPos;

            // If treating as bottom-left, convert to bottom-center by adding half the object width
            bottomLeftWorld.x += drawSize.x * 0.5f;

            // Pick A for now, but print both so we can see immediately which matches Tiled
            glm::vec2 anchorWorld = bottomCenterWorld;

            const float halfW_dbg = tileW * 0.5f;
            const glm::vec2 tiledIsoUnshift_dbg(-(loadedMap.mapData.height - 1) * halfW_dbg, 0.0f);

            glm::vec2 tiledPx = instance.worldPos;                 // what TMX gave us
            glm::vec2 unshiftedPx = tiledPx + tiledIsoUnshift_dbg; // undo Tiled's iso shift


            // Debug (keep this!)
            static int printed = 0;
            if (printed < 12)
            {
                std::cout
                    << "OBJ gid=" << instance.tileIndex
                    << " pos=(" << instance.worldPos.x << "," << instance.worldPos.y << ")"
                    << " size=(" << drawSize.x << "," << drawSize.y << ")\n"
                    << "  tiledPx=(" << tiledPx.x << "," << tiledPx.y << ")\n"
                    << "  unshiftedPx=(" << unshiftedPx.x << "," << unshiftedPx.y << ")\n"
                    << "  mapOrigin=(" << mapOrigin.x << "," << mapOrigin.y << ")\n"
                    << "  unshift=(" << tiledIsoUnshift.x << "," << tiledIsoUnshift.y << ")\n"
                    << "  anchor A(bottom-center)=(" << bottomCenterWorld.x << "," << bottomCenterWorld.y << ")\n"
                    << "  anchor B(from bottom-left)=(" << bottomLeftWorld.x << "," << bottomLeftWorld.y << ")\n";
                printed++;
            }

            RenderCmd cmd{};
            cmd.texture = resolved.textureId;
            cmd.sizePx = drawSize;
            cmd.uvMin = resolved.uvMin;
            cmd.uvMax = resolved.uvMax;

            // bottom-center → top-left
            cmd.posPx = anchorWorld
                - glm::vec2(drawSize.x * 0.5f, drawSize.y);

            // Depth from feet
            cmd.depthKey = DepthFromFeetWorldY(anchorWorld.y);

            renderQueue.Push(cmd);
        }



        // Player into queue (depth sort)
        player.AppendToQueue(renderQueue, playerTileTopLeft, tileW, tileH);

        renderQueue.SortByDepthStable();
        for (const RenderCmd& cmd : renderQueue.Items())
            renderer.Draw(cmd.texture, cmd.posPx, cmd.sizePx, camera, cmd.uvMin, cmd.uvMax);

        // Overhead layer
        overheadMap.DrawOverhead(renderer, tileResolver, camera, { fbW, fbH }, animationTimeMs);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
