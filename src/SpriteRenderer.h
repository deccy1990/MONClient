#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

// Forward declare (we include the real header in the .cpp)
class Camera2D;

class SpriteRenderer
{
public:
    SpriteRenderer(GLuint shaderProgram, int screenWidth, int screenHeight);
    ~SpriteRenderer();

    void SetScreenSize(int screenWidth, int screenHeight);

    void Draw(GLuint texture,
        const glm::vec2& worldPosition,
        const glm::vec2& size,
        const Camera2D& camera);

private:
    void InitRenderData();

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
