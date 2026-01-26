#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>

#include <cmath>

#include "RenderQueue.h"

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
    // Pixel offset from the sprite bottom-center to where the "feet" are.
    // Usually (0, 0) is fine if your sprite is tightly cropped, but we keep it configurable.
    glm::vec2 feetPixelOffset{ 0.0f, 0.0f };

    Player(GLuint texture, const glm::ivec2& tilePos, const glm::vec2& sizePx)
        : mTexture(texture), mTilePos(tilePos), mSizePx(sizePx) {
    }
    // Smooth position in tile-grid space (float). Example: (5.2, 5.0)
    const glm::vec2& GetGridPos() const { return mGridPos; }
    void SetGridPos(const glm::vec2& gp) { mGridPos = gp; }

    // Depth key for isometric sorting (larger = drawn later / in front)
    float GetDepthKey() const { return mGridPos.x + mGridPos.y; }


    glm::ivec2 GetTilePos() const { return glm::ivec2((int)std::round(mGridPos.x), (int)std::round(mGridPos.y)); }
    void SetTilePos(const glm::ivec2& p) { mTilePos = p; }

    // Draw player using the already-computed world position of the tile (top-left of the tile sprite)
    void DrawOnTile(SpriteRenderer& renderer,
        const Camera2D& camera,
        const glm::vec2& tileTopLeftWorldPos,
        int tileW,
        int tileH) const;
    void AppendToQueue(RenderQueue& queue,
        const glm::vec2& tileTopLeftWorldPos,
        int tileW,
        int tileH) const;
    // Returns feet position in world space given a tile top-left (iso) and tile size.
    // Useful for depth sorting and interaction.
    glm::vec2 ComputeFeetWorld(const glm::vec2& tileTopLeftWorldPos, int tileW, int tileH) const
    {
        return tileTopLeftWorldPos + glm::vec2(tileW * 0.5f, (float)tileH);
    }


private:
    GLuint mTexture = 0;
    glm::ivec2 mTilePos{ 0, 0 };
    glm::vec2 mSizePx{ 64.0f, 96.0f }; // typical sprite size (taller than tile)
    glm::vec2 mGridPos{ 0.0f, 0.0f };

};
