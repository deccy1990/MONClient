#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <string>
#include <vector>

class SpriteRenderer;
class Camera2D;
class TileSet;
class Player;

struct TileLayer
{
    std::string name;
    std::vector<int> tiles;
    bool visible = true;
    bool renderable = true;
};

/*
    TileMap
    -------
    Stores tile IDs in multiple 2D layers and draws them using a TileSet (atlas UV lookup).
*/
class TileMap
{
public:
    TileMap(int width, int height, int tileWidthPx, int tileHeightPx);

    void AddLayer(const std::string& name, const std::vector<int>& tiles, bool visible, bool renderable);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }

    int GetTileWidthPx() const { return mTileWidthPx; }
    int GetTileHeightPx() const { return mTileHeightPx; }

    void Draw(SpriteRenderer& renderer,
        GLuint atlasTexture,
        const TileSet& tileset,
        const Camera2D& camera,
        const glm::ivec2& viewportSizePx,
        const Player* player,
        float animationTimeMs) const;

private:
    int Index(int x, int y) const { return y * mWidth + x; }

    int GetLayerTile(const TileLayer& layer, int x, int y) const;

private:
    int mWidth = 0;
    int mHeight = 0;

    int mTileWidthPx = 0;
    int mTileHeightPx = 0;

    std::vector<TileLayer> mLayers;
};
