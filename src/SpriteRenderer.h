#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

class Camera2D;

/*
    SpriteRenderer
    --------------
    Draws a unit quad with a texture.
    Supports:
      - Default UVs (0..1)
      - Custom UV rectangle for texture atlases
*/
class SpriteRenderer
{
public:
    SpriteRenderer(GLuint shaderProgram, int screenWidth, int screenHeight);
    ~SpriteRenderer();

    void SetScreenSize(int screenWidth, int screenHeight);

    // Draw full texture
    void Draw(GLuint texture,
        const glm::vec2& worldPosition,
        const glm::vec2& size,
        const Camera2D& camera);

    // Draw with atlas UVs
    void Draw(GLuint texture,
        const glm::vec2& worldPosition,
        const glm::vec2& size,
        const Camera2D& camera,
        const glm::vec2& uvMin,
        const glm::vec2& uvMax);

private:
    void InitRenderData();

private:
    GLuint mShaderProgram = 0;
    GLint  mProjectionLoc = -1;
    GLint  mModelLoc = -1;
    GLint  mTextureLoc = -1;

    GLuint mVAO = 0;
    GLuint mVBO = 0;
    GLuint mEBO = 0;

    int mScreenWidth = 800;
    int mScreenHeight = 600;
};
