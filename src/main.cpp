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
    */
    Texture2D atlas = LoadTextureRGBA("assets/tiles.png", true);
    if (!atlas.id)
    {
        std::cerr << "Failed to load assets/tiles.png\n";
        glfwTerminate();
        return -1;
    }

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


    /*
        ============================================
        Main loop
        ============================================
    */
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Update framebuffer size (needed for projection + culling)
        glfwGetFramebufferSize(window, &fbW, &fbH);
        renderer.SetScreenSize(fbW, fbH);

        // Basic clear
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- Player movement (tile-based stepping) ---
 // This version steps one tile per key press.
 // Next upgrade: smooth movement + interpolation.

        static bool wasW = false, wasA = false, wasS = false, wasD = false;

        bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

        glm::ivec2 p = player.GetTilePos();

        if (w && !wasW) p.y -= 1;
        if (s && !wasS) p.y += 1;
        if (a && !wasA) p.x -= 1;
        if (d && !wasD) p.x += 1;

        // Clamp player within map bounds
        p.x = std::max(0, std::min(mapW - 1, p.x));
        p.y = std::max(0, std::min(mapH - 1, p.y));

        player.SetTilePos(p);

        wasW = w; wasA = a; wasS = s; wasD = d;

        // --- Camera follow player (simple snap follow) ---
        const float halfW = tileW * 0.5f;
        const float halfH = tileH * 0.5f;

        // Convert player tile -> iso world position (tile top-left)
        float isoX = (float)(p.x - p.y) * halfW;
        float isoY = (float)(p.x + p.y) * halfH;

        // Match the same origin offset used in TileMap.cpp
        glm::vec2 mapOrigin(
            fbW * 0.5f,  // horizontal center of the screen
            60.0f        // vertical offset (same as TileMap)
        );

        // Player tile top-left in world space
        glm::vec2 playerTileTopLeft(isoX, isoY);
        playerTileTopLeft += mapOrigin;

        // Center camera on player
        glm::vec2 camPos = playerTileTopLeft - glm::vec2(fbW * 0.5f, fbH * 0.5f);
        camera.SetPosition(camPos);



        /*
            ============================================
            Draw world
            ============================================
        */
        map.Draw(renderer, atlas.id, tileset, camera, { fbW, fbH }, &player);


        

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
