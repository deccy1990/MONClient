#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <filesystem>

#include "SpriteRenderer.h"
#include "Camera2D.h"
#include "TileMap.h"



// stb_image is used to load image files (PNG, JPG, etc.)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

/*
    ============================================
    Shaders
    ============================================
    These match what SpriteRenderer expects:
      uniform mat4 uProjection;
      uniform mat4 uModel;
      uniform sampler2D uTexture;
*/

const char* vertexShaderSrc = R"(
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

const char* fragmentShaderSrc = R"(
#version 330 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D uTexture;

void main()
{
    FragColor = texture(uTexture, TexCoord);
}
)";

// Keep viewport in sync with framebuffer size
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// Compile a shader and print errors
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

// Link a shader program and print errors
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

// Load a PNG/JPG file into an OpenGL texture and return the texture ID
static GLuint LoadTextureRGBA(const char* path)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Sprite-friendly sampling (pixel art friendly); change later if you want smoothing
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int w = 0, h = 0, channels = 0;
    stbi_set_flip_vertically_on_load(true);

    // Force RGBA output for consistency (sprites often need alpha)
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data)
    {
        std::cerr << "Failed to load texture '" << path << "': " << stbi_failure_reason() << "\n";
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return texture;
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
        return -1;
    }

    std::cout << "Working directory: " << std::filesystem::current_path() << "\n";

    /*
        ============================================
        Global render state
        ============================================
        Alpha blending is crucial for sprites with transparency.
    */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /*
        ============================================
        Create shader program + load texture
        ============================================
    */
    GLuint shaderProgram = CreateProgram(vertexShaderSrc, fragmentShaderSrc);
    if (!shaderProgram)
        return -1;

    GLuint tex = LoadTextureRGBA("assets/test.png");
    if (!tex)
        return -1;

    /*
        ============================================
        Create sprite renderer
        ============================================
        We pass initial screen size. We'll update it each frame in case of resize.
    */
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(window, &fbW, &fbH);

    SpriteRenderer renderer(shaderProgram, fbW, fbH);

    // ------------------------------------
// Create camera (world-space origin)
// ------------------------------------
    Camera2D camera({ 0.0f, 0.0f });


    // ------------------------------------
    // Load tile texture
    // ------------------------------------
    GLuint tileTex = LoadTextureRGBA("assets/tile.png");
    if (!tileTex)
        return -1;


    // ------------------------------------
// Create a tile map
// ------------------------------------
// Map dimensions in tiles
    const int mapW = 100;
    const int mapH = 100;

    // Tile size in pixels (pick something like 32/48/64)
    const int tileSize = 64;

    TileMap map(mapW, mapH, tileSize);

    // Fill the map with visible tiles (ID = 1)
    for (int y = 0; y < mapH; ++y)
    {
        for (int x = 0; x < mapW; ++x)
        {
            map.SetTile(x, y, 1);
        }
    }


    // Fill the map with a simple pattern so you can see movement clearly.
    // Tile ID 1 = draw tile (non-zero)
    for (int y = 0; y < mapH; ++y)
    {
        for (int x = 0; x < mapW; ++x)
        {
            // Checkerboard pattern (just for visibility)
            int id = ((x + y) % 2 == 0) ? 1 : 1; // currently both are 1 (single texture)
            map.SetTile(x, y, id);
        }
    }


    /*
        ============================================
        Render loop
        ============================================
    */
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Keep renderer's projection correct on resize
        glfwGetFramebufferSize(window, &fbW, &fbH);
        renderer.SetScreenSize(fbW, fbH);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        float cameraSpeed = 300.0f; // pixels per second
        float deltaTime = 0.016f;   // fixed step for now (we’ll improve this later)

        // Keyboard scrolling (WASD + arrows)
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

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        renderer.SetScreenSize(fbW, fbH);

        // Draw tile map FIRST (background)
        map.Draw(renderer, tileTex, camera, { fbW, fbH });


        // Draw the same sprite at different positions to prove reusability
        renderer.Draw(tex, { 100, 100 }, { 256, 256 }, camera);
        renderer.Draw(tex, { 400, 150 }, { 128, 128 }, camera);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}
