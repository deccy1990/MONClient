#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <filesystem>
#include <cstdint>
#include "tinyxml2.h"

// Engine modules
#include "SpriteRenderer.h"
#include "Camera2d.h"
#include "TileMap.h"
#include "TileResolver.h"
#include "TileSet.h"
#include "Player.h"
#include "TmxLoader.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// stb_image (ONLY define implementation in ONE .cpp file  main.cpp is a good choice)
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
    // Keep OpenGL viewport aligned with window framebuffer size
    glViewport(0, 0, width, height);
}

static glm::vec2 GridToIsoTopLeft(const glm::vec2& gridPos, float tileW, float tileH, const glm::vec2& mapOrigin)
{
    float halfW = tileW * 0.5f;
    float halfH = tileH * 0.5f;
    float isoX = (gridPos.x - gridPos.y) * halfW;
    float isoY = (gridPos.x + gridPos.y) * halfH;
    return glm::vec2(isoX, isoY) + mapOrigin;
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
    - Loads PNG/JPG into OpenGL
    - Returns Texture2D {id, width, height}
*/
static Texture2D LoadTextureRGBA(const char* path, bool flipY)
{
    Texture2D tex{};

    // Control vertical flip per-texture
    stbi_set_flip_vertically_on_load(flipY);

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);

    // Pixel-art friendly sampling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int channels = 0;
    unsigned char* data = stbi_load(path, &tex.width, &tex.height, &channels, 4);
    if (!data)
    {
        std::cerr << "Failed to load texture '" << path
            << "': " << stbi_failure_reason() << "\n";
        tex.id = 0;
        return tex;
    }

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        tex.width,
        tex.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data
    );

    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);

    return tex;
}

// Loads a CSV of integers into a flat vector (row-major).
// Returns true on success and outputs width/height.
static bool LoadCSVIntGrid(const std::string& path,
    std::vector<int>& out,
    int& outW,
    int& outH)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open CSV: " << path << "\n";
        return false;
    }

    std::vector<int> values;
    std::string line;

    int width = -1;
    int height = 0;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        std::stringstream ss(line);
        std::string cell;

        int rowCount = 0;

        while (std::getline(ss, cell, ','))
        {
            // Trim spaces (optional but helps with hand-edited CSVs)
            while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ' || cell.back() == '\t'))
                cell.pop_back();
            size_t start = 0;
            while (start < cell.size() && (cell[start] == ' ' || cell[start] == '\t'))
                start++;

            int v = std::stoi(cell.substr(start));
            values.push_back(v);
            rowCount++;
        }

        if (width == -1) width = rowCount;
        else if (rowCount != width)
        {
            std::cerr << "CSV row width mismatch in " << path << "\n";
            return false;
        }

        height++;
    }

    if (width <= 0 || height <= 0)
    {
        std::cerr << "CSV empty/invalid: " << path << "\n";
        return false;
    }

    out = std::move(values);
    outW = width;
    outH = height;
    return true;
}

// ------------------------------------
// TMX test: load and print basic map info
// ------------------------------------
static bool DebugLoadTMX(const char* tmxPath)
{
    using namespace tinyxml2;

    XMLDocument doc;
    XMLError err = doc.LoadFile(tmxPath);
    if (err != XML_SUCCESS)
    {
        std::cerr << "Failed to load TMX: " << tmxPath
            << " error=" << doc.ErrorStr() << "\n";
        return false;
    }

    XMLElement* map = doc.FirstChildElement("map");
    if (!map)
    {
        std::cerr << "TMX missing <map> root element\n";
        return false;
    }

    int width = map->IntAttribute("width");
    int height = map->IntAttribute("height");
    int tileW = map->IntAttribute("tilewidth");
    int tileH = map->IntAttribute("tileheight");

    const char* orientation = map->Attribute("orientation");

    std::cout << "TMX loaded: " << tmxPath << "\n";
    std::cout << "  orientation: " << (orientation ? orientation : "(none)") << "\n";
    std::cout << "  map size: " << width << " x " << height << " tiles\n";
    std::cout << "  tile size: " << tileW << " x " << tileH << " px\n";

    for (XMLElement* ts = map->FirstChildElement("tileset"); ts; ts = ts->NextSiblingElement("tileset"))
    {
        int firstGid = ts->IntAttribute("firstgid");
        const char* source = ts->Attribute("source");
        const char* name = ts->Attribute("name");

        std::cout << "  tileset: firstgid=" << firstGid
            << " source=" << (source ? source : "(inline)")
            << " name=" << (name ? name : "(none)")
            << "\n";
    }

    for (XMLElement* layer = map->FirstChildElement("layer"); layer; layer = layer->NextSiblingElement("layer"))
    {
        const char* lname = layer->Attribute("name");
        int lw = layer->IntAttribute("width");
        int lh = layer->IntAttribute("height");

        std::cout << "  layer: " << (lname ? lname : "(unnamed)")
            << " size=" << lw << "x" << lh << "\n";

        XMLElement* data = layer->FirstChildElement("data");
        if (!data)
        {
            std::cout << "    (no <data>)\n";
            continue;
        }

        const char* encoding = data->Attribute("encoding");
        if (!encoding || std::string(encoding) != "csv")
        {
            std::cout << "    data encoding is not CSV (encoding="
                << (encoding ? encoding : "(none)") << ")\n";
            continue;
        }

        const char* csv = data->GetText();
        if (!csv)
        {
            std::cout << "    (empty csv)\n";
            continue;
        }

        std::string preview(csv);
        if (preview.size() > 60) preview.resize(60);
        std::cout << "    csv preview: " << preview << "...\n";
    }

    return true;
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
    if (!LoadTmxMap("assets/maps/testmap.tmx", loadedMap))
    {
        glfwTerminate();
        return -1;
    }

    const int tileW = loadedMap.mapData.tileW;
    const int tileH = loadedMap.mapData.tileH;
    const int mapW = loadedMap.mapData.width;
    const int mapH = loadedMap.mapData.height;

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
    Load textures (ONCE)
    ============================================
    Required files:
    - tileset images referenced by the TMX/TSX
    - assets/test.png    (optional sprite test)
	- assets/player.png  (player sprite)
	- assets/collision.png (collision debug view)
    */
    std::vector<Texture2D> tilesetTextures;
    std::vector<TilesetRuntime> tilesetRuntimes;
    tilesetTextures.reserve(loadedMap.mapData.tilesets.size());
    tilesetRuntimes.reserve(loadedMap.mapData.tilesets.size());

    for (const TilesetDef& tilesetDef : loadedMap.mapData.tilesets)
    {
        Texture2D texture = LoadTextureRGBA(tilesetDef.imagePath.c_str(), true);
        if (!texture.id)
        {
            std::cerr << "Failed to load tileset image: " << tilesetDef.imagePath << "\n";
            glfwTerminate();
            return -1;
        }

        tilesetTextures.push_back(texture);

        TileSet tileset(texture.width, texture.height, tilesetDef.tileW, tilesetDef.tileH);
        tileset.SetAnimations(tilesetDef.animations);

        TilesetRuntime runtime{ tilesetDef, tileset, texture.id };
        tilesetRuntimes.push_back(std::move(runtime));
    }

    TileResolver tileResolver(tilesetRuntimes);

    Texture2D playerTex = LoadTextureRGBA("assets/player.png", false);
    if (!playerTex.id)
    {
        std::cerr << "Failed to load assets/player.png\n";
        glfwTerminate();
        return -1;
    }



    /*
    ============================================
    Create renderer + camera
    ============================================
    */
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    SpriteRenderer renderer(shaderProgram, fbW, fbH);

    // Camera position is world-space top-left of viewport
    Camera2D camera({ 0.0f, 0.0f });

    // Create player (fallback tile position 5,5 and sprite size 64x96)
    Player player(playerTex.id, { 5, 5 }, { 64.0f, 96.0f });
    player.SetGridPos({ 5.0f, 5.0f });


   /*
   ============================================
   Create TileMap
   ============================================
   Chosen tile size: 64x32
   */
    if (!tilesetTextures.empty())
        std::cout << "tileset[0] size: " << tilesetTextures.front().width << " x " << tilesetTextures.front().height << "\n";


    std::vector<uint8_t> collisionGrid = loadedMap.mapData.collision;

    // Spawn player from object layer when available
    for (const MapObject& object : loadedMap.mapData.objects)
    {
        std::string lowerName = object.name;
        std::string lowerType = object.type;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lowerName == "playerspawn" || lowerType == "playerspawn" || lowerName == "player" || lowerType == "player")
        {
            glm::vec2 spawnGrid = ObjectPixelsToGrid(object.positionPx, tileW, tileH);
            spawnGrid.x = std::clamp(spawnGrid.x, 0.0f, (float)(mapW - 1));
            spawnGrid.y = std::clamp(spawnGrid.y, 0.0f, (float)(mapH - 1));
            player.SetGridPos(spawnGrid);
            std::cout << "Player spawn from object id=" << object.id
                      << " grid=(" << spawnGrid.x << "," << spawnGrid.y << ")\n";
            break;
        }
    }

    // ------------------------------------
    // Spawn sanity check
    // ------------------------------------
    auto IsBlockedAtSpawn = [&](int tx, int ty) -> bool
    {
        if (tx < 0 || tx >= mapW || ty < 0 || ty >= mapH) return true;
        return collisionGrid[ty * mapW + tx] != 0;
    };

    int spX = (int)std::floor(player.GetGridPos().x);
    int spY = (int)std::floor(player.GetGridPos().y);

    std::cout << "Spawn tile = (" << spX << "," << spY << ") collision="
              << (IsBlockedAtSpawn(spX, spY) ? 1 : 0) << "\n";

    auto FindFirstWalkable = [&]() -> glm::vec2
    {
        for (int y = 0; y < mapH; ++y)
            for (int x = 0; x < mapW; ++x)
                if (collisionGrid[y * mapW + x] == 0)
                    return glm::vec2((float)x + 0.5f, (float)y + 0.5f);
        return glm::vec2(1.0f, 1.0f);
    };

    glm::vec2 start = player.GetGridPos();
    int sx = (int)std::floor(start.x);
    int sy = (int)std::floor(start.y);

    if (sx < 0 || sx >= mapW || sy < 0 || sy >= mapH || collisionGrid[sy * mapW + sx] != 0)
    {
        glm::vec2 newPos = FindFirstWalkable();
        std::cout << "Spawn blocked, moving player to walkable tile at "
                  << newPos.x << "," << newPos.y << "\n";
        player.SetGridPos(newPos);
    }

    auto MakeTileLayer = [&](const std::vector<uint32_t>& tiles)
    {
        if ((int)tiles.size() == mapW * mapH)
            return tiles;

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
       Main loop
       ============================================
        */
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // ------------------------------------
        // Run toggle (Ctrl) - toggles runEnabled on/off
        // ------------------------------------
        static bool runEnabled = false;
        static bool wasCtrlDown = false;

        bool ctrlDown =
            (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
            (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

        if (ctrlDown && !wasCtrlDown)
            runEnabled = !runEnabled;

        wasCtrlDown = ctrlDown;

        // ------------------------------------
        // deltaTime (real)
        // ------------------------------------
        static double lastTime = glfwGetTime();
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;

        // Safety clamp (avoid huge jumps after breakpoints/tab-out)
        if (deltaTime > 0.05f) deltaTime = 0.05f;

        static float animationTimeMs = 0.0f;
        animationTimeMs += deltaTime * 1000.0f;

        // ------------------------------------
        // Update framebuffer size (projection depends on it)
        // ------------------------------------
        glfwGetFramebufferSize(window, &fbW, &fbH);
        renderer.SetScreenSize(fbW, fbH);

        // ------------------------------------
        // Clear
        // ------------------------------------
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ------------------------------------
        // Movement speed (stored for later smooth movement + stamina)
        // NOTE: tile-step movement doesnt use moveSpeed yet; we will in the next step.
        // ------------------------------------
        float walkSpeed = 120.0f;
        float runSpeed = 200.0f;
        float moveSpeed = runEnabled ? runSpeed : walkSpeed;
        (void)moveSpeed; // suppress unused warning for now

        // ------------------------------------
        // Smooth player movement (hold WASD)
        // ------------------------------------
        // We move in *grid space* (tile coordinates as floats), then convert to iso for rendering.
        // This gives smooth movement while keeping the logic tile-friendly (for collision later).

        // ------------------------------------
        // Isometric-correct input mapping
        // WASD represent SCREEN directions, then we convert to GRID movement.
        // ------------------------------------
        glm::vec2 screenDir(0.0f, 0.0f);

        // Screen space: +x right, +y down
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) screenDir.y -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) screenDir.y += 1.0f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) screenDir.x -= 1.0f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) screenDir.x += 1.0f;

        if (screenDir.x != 0.0f || screenDir.y != 0.0f)
            screenDir = glm::normalize(screenDir);

        // Convert screen direction -> grid direction using iso basis vectors:
        // screenRight = (+1, -1) in grid
        // screenDown  = (+1, +1) in grid
        glm::vec2 gridDir = screenDir.x * glm::vec2(1.0f, -1.0f) +
            screenDir.y * glm::vec2(1.0f, 1.0f);

        if (gridDir.x != 0.0f || gridDir.y != 0.0f)
            gridDir = glm::normalize(gridDir);


        // Normalize so diagonal isnt faster
        if (gridDir.x != 0.0f || gridDir.y != 0.0f)
            gridDir = glm::normalize(gridDir);

        // Speed in tiles per second (tune to taste)
        float walkTilesPerSec = 3.0f;
        float runTilesPerSec = 5.0f;
        float maxSpeed = runEnabled ? runTilesPerSec : walkTilesPerSec;

        // Acceleration/deceleration in tiles/sec^2
        float accel = 18.0f;
        float decel = 22.0f;

        // Persistent velocity (grid units / second)
        static glm::vec2 vel(0.0f, 0.0f);

        // Apply acceleration or deceleration
        if (gridDir.x != 0.0f || gridDir.y != 0.0f)
        {
            vel += gridDir * accel * deltaTime;

            // Clamp to max speed
            float speed = glm::length(vel);
            if (speed > maxSpeed)
                vel = (vel / speed) * maxSpeed;
        }
        else
        {
            // Decelerate toward zero when no input
            float speed = glm::length(vel);
            if (speed > 0.0f)
            {
                float drop = decel * deltaTime;
                float newSpeed = std::max(0.0f, speed - drop);
                vel = (speed > 0.0f) ? (vel / speed) * newSpeed : glm::vec2(0.0f);
            }
        }

        // ------------------------------------
        // Collision settings (grid units)
        // ------------------------------------
        // Player hitbox in TILE units (tune later)
        const glm::vec2 playerHalfExtents(0.05f, 0.05f);

        // 0 = walkable, 1 = blocked
        auto IsBlockedAt = [&](int tx, int ty) -> bool
            {
                if (tx < 0 || tx >= mapW || ty < 0 || ty >= mapH)
                    return true; // outside map = solid

                return collisionGrid[ty * mapW + tx] != 0;
            };

        // Checks if player AABB (in grid space) overlaps any blocked tiles
        auto CollidesAt = [&](const glm::vec2& pos) -> bool
            {
                glm::vec2 corners[4] =
                {
                    { pos.x - playerHalfExtents.x, pos.y - playerHalfExtents.y },
                    { pos.x + playerHalfExtents.x, pos.y - playerHalfExtents.y },
                    { pos.x - playerHalfExtents.x, pos.y + playerHalfExtents.y },
                    { pos.x + playerHalfExtents.x, pos.y + playerHalfExtents.y }
                };

                for (const glm::vec2& c : corners)
                {
                    int tx = (int)std::floor(c.x);
                    int ty = (int)std::floor(c.y);

                    if (IsBlockedAt(tx, ty))
                        return true;
                }

                return false;
            };




        // ------------------------------------
        // Integrate movement with hitbox collision (slide along walls)
        // ------------------------------------
        glm::vec2 gp = player.GetGridPos();

        // X
        glm::vec2 tryX = gp;
        tryX.x += vel.x * deltaTime;

        if (!CollidesAt(tryX)) gp.x = tryX.x;
        else vel.x = 0.0f;

        // Y
        glm::vec2 tryY = gp;
        tryY.y += vel.y * deltaTime;

        if (!CollidesAt(tryY)) gp.y = tryY.y;
        else vel.y = 0.0f;

        // Clamp within map bounds
        gp.x = std::clamp(gp.x, 0.0f, (float)(mapW - 1));
        gp.y = std::clamp(gp.y, 0.0f, (float)(mapH - 1));

        player.SetGridPos(gp);


        // ------------------------------------
        // Camera follow with dead-zone + smoothing
        // ------------------------------------
        const float halfW = tileW * 0.5f;
        const float halfH = tileH * 0.5f;

        // Player iso tile top-left (world)
        float isoX = (gp.x - gp.y) * halfW;
        float isoY = (gp.x + gp.y) * halfH;

        // Must match TileMap.cpp origin
        glm::vec2 mapOrigin(fbW * 0.5f, 60.0f);

        // Tile top-left world pos for the player (same convention as tiles)
        glm::vec2 playerTileTopLeft = glm::vec2(isoX, isoY) + mapOrigin;

        // "Feet" point is best for camera focus (bottom-center of tile)
        glm::vec2 playerWorldFeet = playerTileTopLeft + glm::vec2(tileW * 0.5f, (float)tileH);

        // Current camera center in world coords
        glm::vec2 camPos = camera.GetPosition();
        glm::vec2 halfView((float)fbW * 0.5f, (float)fbH * 0.5f);
        glm::vec2 camCenter = camPos + halfView;

        // Dead-zone size (tune these)
        // Larger = camera moves less often
        glm::vec2 deadZoneHalf(80.0f, 60.0f);

        // Keep camera center unless player exits the dead-zone rectangle
        glm::vec2 desiredCenter = camCenter;
        glm::vec2 delta = playerWorldFeet - camCenter;

        if (delta.x > deadZoneHalf.x) desiredCenter.x = playerWorldFeet.x - deadZoneHalf.x;
        if (delta.x < -deadZoneHalf.x) desiredCenter.x = playerWorldFeet.x + deadZoneHalf.x;

        if (delta.y > deadZoneHalf.y) desiredCenter.y = playerWorldFeet.y - deadZoneHalf.y;
        if (delta.y < -deadZoneHalf.y) desiredCenter.y = playerWorldFeet.y + deadZoneHalf.y;

        // Desired camera top-left from desired center
        glm::vec2 targetCamPos = desiredCenter - halfView;

        // Smooth camera follow (frame-rate independent)
        const float cameraFollowStrength = 10.0f; // slightly lower feels nicer with dead-zone
        float t = 1.0f - std::exp(-cameraFollowStrength * deltaTime);

        camPos = camPos + (targetCamPos - camPos) * t;
        camera.SetPosition(camPos);


        // ------------------------------------
        // Draw world
        // ------------------------------------
        groundMap.Draw(renderer, tileResolver, camera, { fbW, fbH }, nullptr, animationTimeMs);
        wallsMap.Draw(renderer, tileResolver, camera, { fbW, fbH }, nullptr, animationTimeMs);

        struct DepthDrawEntry
        {
            float feetY = 0.0f;
            size_t index = 0;
            bool isPlayer = false;
        };

        std::vector<DepthDrawEntry> depthEntries;
        depthEntries.reserve(loadedMap.mapData.objectInstances.size() + 1);

        for (size_t i = 0; i < loadedMap.mapData.objectInstances.size(); ++i)
        {
            const MapObjectInstance& instance = loadedMap.mapData.objectInstances[i];
            DepthDrawEntry entry{};
            entry.index = i;
            entry.isPlayer = false;
            entry.feetY = mapOrigin.y + instance.worldPos.y + instance.size.y;
            depthEntries.push_back(entry);
        }

        glm::vec2 playerTileTopLeft = GridToIsoTopLeft(player.GetGridPos(), (float)tileW, (float)tileH, mapOrigin);
        DepthDrawEntry playerEntry{};
        playerEntry.index = 0;
        playerEntry.isPlayer = true;
        playerEntry.feetY = playerTileTopLeft.y + tileH;
        depthEntries.push_back(playerEntry);

        std::stable_sort(depthEntries.begin(), depthEntries.end(),
            [](const DepthDrawEntry& a, const DepthDrawEntry& b)
            {
                return a.feetY < b.feetY;
            });

        for (const DepthDrawEntry& entry : depthEntries)
        {
            if (entry.isPlayer)
            {
                player.DrawOnTile(renderer, camera, playerTileTopLeft, tileW, tileH);
                continue;
            }

            const MapObjectInstance& instance = loadedMap.mapData.objectInstances[entry.index];
            ResolvedTile resolved{};
            if (!tileResolver.Resolve(instance.tileIndex, animationTimeMs, resolved))
                continue;

            glm::vec2 drawPos = mapOrigin + instance.worldPos;
            renderer.Draw(resolved.textureId, drawPos, instance.size, camera, resolved.uvMin, resolved.uvMax);
        }

        overheadMap.Draw(renderer, tileResolver, camera, { fbW, fbH }, nullptr, animationTimeMs);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
