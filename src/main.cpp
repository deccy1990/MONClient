#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <filesystem>

// Engine modules
#include "SpriteRenderer.h"
#include "Camera2D.h"
#include "TileMap.h"
#include "TileSet.h"
#include "Player.h"
#include <cmath>

inline bool IsBlockedTile(int tileId)
{
    return tileId == 6;
}



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
    // Keep OpenGL viewport aligned with window framebuffer size
    glViewport(0, 0, width, height);
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
          - assets/tiles.png   (your atlas: 4x2 grid, 8 tiles, each 64x32)
          - assets/test.png    (optional sprite test)
		  - assets/player.png  (player sprite)
		  - assets/collision.png (collision debug view)
    */
    Texture2D atlas = LoadTextureRGBA("assets/tiles.png", true);
    if (!atlas.id)
    {
        std::cerr << "Failed to load assets/tiles.png\n";
        glfwTerminate();
        return -1;
    }

    Texture2D collisionTex = LoadTextureRGBA("assets/collision.png", false);
    if (!collisionTex.id) return -1;


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

    // Create player (tile position 5,5 and sprite size 64x96)
    Player player(playerTex.id, { 5, 5 }, { 64.0f, 96.0f });
    player.SetGridPos({ 5.0f, 5.0f });


    /*
        ============================================
        Create TileSet (atlas UV mapper) + TileMap
        ============================================
        Chosen tile size: 64x32
    */
    const int tileW = 64;
    const int tileH = 32;

    TileSet tileset(atlas.width, atlas.height, tileW, tileH);

    const int mapW = 100;
    const int mapH = 100;

    //Atlas debug size

    std::cout << "atlas size: " << atlas.width << " x " << atlas.height << "\n";


    //Quick “proof test” (optional, 30 seconds)

    TileMap map(mapW, mapH, tileW, tileH);

    for (int i = 0; i < 8; ++i)
    {
        glm::vec2 a, b;
        tileset.GetUV(i, a, b);
        std::cout << "tile " << i << " uvMin=(" << a.x << "," << a.y << ") uvMax=(" << b.x << "," << b.y << ")\n";
    }


    // Fill the map with IDs 1..8 (0 reserved as "empty")
    for (int y = 0; y < mapH; ++y)
    {
        for (int x = 0; x < mapW; ++x)
        {
            int id = (x + y) % 8;  // valid atlas ids: 0..7
            map.SetTile(x, y, id);
        }
    }
    // ------------------------------------
// DEBUG collision wall (tile ID 6 = blocked)
// ------------------------------------
    for (int x = 10; x < 30; ++x)
    {
        map.SetTile(x, 15, 6);
    }
    // Confirm what ID is actually stored there
    std::cout << "Wall test tile id at (10,15) = " << map.GetTile(10, 15) << "\n";



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
        // NOTE: tile-step movement doesn’t use moveSpeed yet; we will in the next step.
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


        // Normalize so diagonal isn’t faster
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
        const glm::vec2 playerHalfExtents(0.25f, 0.20f);

        // Which tile IDs are blocked?
        auto IsBlockedTile = [](int id) -> bool
            {
                return (id == 6); // stone wall tile
                // If you want tile 7 solid too, use: return (id == 6 || id == 7);
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

                    int tid = map.GetTile(tx, ty);

                    // Treat outside map as solid
                    if (tid < 0)
                        return true;

                    if (IsBlockedTile(tid))
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

        player.SetGridPos(gp);


        // Clamp within map bounds (keep a small margin so you don’t go past edges)
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
        map.Draw(renderer, atlas.id, tileset, camera, { fbW, fbH }, &player);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
