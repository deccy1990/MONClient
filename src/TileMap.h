#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

class SpriteRenderer;
class Camera2D;
class TileSet;
class Player;

/*
    TileMap
    -------
    Stores tile IDs in a 2D grid and draws them using a TileSet (atlas UV lookup).
*/
class TileMap
{
public:
    TileMap(int width, int height, int tileWidthPx, int tileHeightPx);

    void SetTile(int x, int y, int tileId);
    int  GetTile(int x, int y) const;

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }

    int GetTileWidthPx() const { return mTileWidthPx; }
    int GetTileHeightPx() const { return mTileHeightPx; }

    void Draw(SpriteRenderer& renderer,
        GLuint atlasTexture,
        const TileSet& tileset,
        const Camera2D& camera,
        const glm::ivec2& viewportSizePx,
        const class Player* player) const;


private:
    int Index(int x, int y) const { return y * mWidth + x; }

private:
    int mWidth = 0;
    int mHeight = 0;

    int mTileWidthPx = 0;
    int mTileHeightPx = 0;

    std::vector<int> mTiles;
};