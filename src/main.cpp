#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <filesystem>

// Engine modules
#include "SpriteRenderer.h"
#include "Camera2D.h"
#include "TileMap.h"
#include "TileSet.h"

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
static Texture2D LoadTextureRGBA(const char* path)
{
    Texture2D tex{};

    // IMPORTANT:
    // We flip vertically so the image loads in the expected orientation for OpenGL UVs.
    // Our TileSet code compensates so tileId 0 is the top-left tile in the atlas.
    stbi_set_flip_vertically_on_load(true);

    int channels = 0;
    unsigned char* data = stbi_load(path, &tex.width, &tex.height, &channels, 4);
    if (!data)
    {
        std::cerr << "Failed to load texture '" << path << "': " << stbi_failure_reason() << "\n";
        tex.id = 0;
        return tex;
    }

    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);

    // IMPORTANT for atlases: clamp so UVs don't wrap into other tiles
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Pixel-art friendly sampling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);


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
    Texture2D atlas = LoadTextureRGBA("assets/tiles.png");
    if (!atlas.id)
    {
        glfwTerminate();
        return -1;
    }

    Texture2D testSprite = LoadTextureRGBA("assets/test.png");
    if (!testSprite.id)
    {
        // Not fatal if you want — but we'll keep it strict for now
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

        /*
            ============================================
            Camera movement (WASD)
            ============================================
            For now we use a fixed delta time.
            Later we’ll compute real deltaTime using glfwGetTime().
        */
        float cameraSpeed = 300.0f; // pixels/sec
        float deltaTime = 0.016f;   // ~60 FPS

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        {
            camera.Move({ 0.0f, -cameraSpeed * deltaTime });
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        {
            camera.Move({ 0.0f, cameraSpeed * deltaTime });
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        {
            camera.Move({ -cameraSpeed * deltaTime, 0.0f });
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        {
            camera.Move({ cameraSpeed * deltaTime, 0.0f });
        }

        /*
            ============================================
            Draw world
            ============================================
        */
        map.Draw(renderer, atlas.id, tileset, camera, { fbW, fbH });

        // Optional: draw a test sprite on top (shows layering)
        renderer.Draw(testSprite.id, { 200.0f, 200.0f }, { 128.0f, 128.0f }, camera);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
