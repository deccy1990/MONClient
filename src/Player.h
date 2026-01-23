#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>

class SpriteRenderer;
class Camera2D;

/*
    Player
    ------
    Holds tile position and draws itself anchored to the isometric tile.

    The player is drawn so its "feet" sit on the bottom-center of the tile.
*/
class Player
{
public:
    Player(GLuint texture, const glm::ivec2& tilePos, const glm::vec2& sizePx)
        : mTexture(texture), mTilePos(tilePos), mSizePx(sizePx) {
    }

    const glm::ivec2& GetTilePos() const { return mTilePos; }
    void SetTilePos(const glm::ivec2& p) { mTilePos = p; }

    // Draw player using the already-computed world position of the tile (top-left of the tile sprite)
    void DrawOnTile(SpriteRenderer& renderer,
        const Camera2D& camera,
        const glm::vec2& tileTopLeftWorldPos,
        int tileW,
        int tileH) const;

private:
    GLuint mTexture = 0;
    glm::ivec2 mTilePos{ 0, 0 };
    glm::vec2 mSizePx{ 64.0f, 96.0f }; // typical sprite size (taller than tile)
};
