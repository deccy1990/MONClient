#include "SpriteRenderer.h"
#include "Camera2D.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

SpriteRenderer::SpriteRenderer(GLuint shaderProgram, int screenWidth, int screenHeight)
    : mShaderProgram(shaderProgram), mScreenWidth(screenWidth), mScreenHeight(screenHeight)
{
    // Cache uniform locations once
    mProjectionLoc = glGetUniformLocation(mShaderProgram, "uProjection");
    mModelLoc = glGetUniformLocation(mShaderProgram, "uModel");
    mTextureLoc = glGetUniformLocation(mShaderProgram, "uTexture");

    // Tell shader that uTexture uses texture unit 0
    glUseProgram(mShaderProgram);
    glUniform1i(mTextureLoc, 0);

    // Create quad geometry buffers
    InitRenderData();
}

SpriteRenderer::~SpriteRenderer()
{
    if (mEBO) glDeleteBuffers(1, &mEBO);
    if (mVBO) glDeleteBuffers(1, &mVBO);
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
}

void SpriteRenderer::SetScreenSize(int screenWidth, int screenHeight)
{
    mScreenWidth = screenWidth;
    mScreenHeight = screenHeight;
}

void SpriteRenderer::Draw(GLuint texture,
    const glm::vec2& worldPosition,
    const glm::vec2& size,
    const Camera2D& camera)
{
    // Convert world position -> screen position using camera
    glm::vec2 screenPos = worldPosition - camera.GetPosition();

    // Orthographic projection: (0,0) top-left, (w,h) bottom-right
    glm::mat4 projection = glm::ortho(
        0.0f, (float)mScreenWidth,
        (float)mScreenHeight, 0.0f,
        -1.0f, 1.0f
    );

    // Model matrix: translate then scale a unit quad
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(screenPos, 0.0f));
    model = glm::scale(model, glm::vec3(size, 1.0f));

    glUseProgram(mShaderProgram);
    glUniformMatrix4fv(mProjectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(mModelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void SpriteRenderer::InitRenderData()
{
    float vertices[] = {
        // aPos (x,y)   // aTex (u,v)
         0.0f, 0.0f,     0.0f, 0.0f, // top-left
         1.0f, 0.0f,     1.0f, 0.0f, // top-right
         1.0f, 1.0f,     1.0f, 1.0f, // bottom-right
         0.0f, 1.0f,     0.0f, 1.0f  // bottom-left
    };

    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}
