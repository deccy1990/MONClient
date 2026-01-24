#include "SpriteRenderer.h"
#include "Camera2d.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

SpriteRenderer::SpriteRenderer(GLuint shaderProgram, int screenWidth, int screenHeight)
    : mShaderProgram(shaderProgram), mScreenWidth(screenWidth), mScreenHeight(screenHeight)
{
    mProjectionLoc = glGetUniformLocation(mShaderProgram, "uProjection");
    mModelLoc = glGetUniformLocation(mShaderProgram, "uModel");
    mTextureLoc = glGetUniformLocation(mShaderProgram, "uTexture");

    glUseProgram(mShaderProgram);
    glUniform1i(mTextureLoc, 0);

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
    // Default: draw the full texture
    Draw(texture, worldPosition, size, camera, { 0.0f, 0.0f }, { 1.0f, 1.0f });
}

void SpriteRenderer::Draw(GLuint texture,
    const glm::vec2& worldPosition,
    const glm::vec2& size,
    const Camera2D& camera,
    const glm::vec2& uvMin,
    const glm::vec2& uvMax)
{
    // Convert world -> screen
    glm::vec2 screenPos = worldPosition - camera.GetPosition();

    glm::mat4 projection = glm::ortho(
        0.0f, (float)mScreenWidth,
        (float)mScreenHeight, 0.0f,
        -1.0f, 1.0f
    );

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(screenPos, 0.0f));
    model = glm::scale(model, glm::vec3(size, 1.0f));

    // Update vertex data (positions are unit quad; UVs change per tile)
    float verts[] = {
        // x, y,   u, v
        0.0f, 0.0f,  uvMin.x, uvMin.y,
        1.0f, 0.0f,  uvMax.x, uvMin.y,
        1.0f, 1.0f,  uvMax.x, uvMax.y,
        0.0f, 1.0f,  uvMin.x, uvMax.y
    };

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

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
    // Initial vertex data (UVs will be overwritten per draw via glBufferSubData)
    float vertices[] = {
        // x, y,   u, v
        0.0f, 0.0f,  0.0f, 0.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        0.0f, 1.0f,  0.0f, 1.0f
    };

    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}
