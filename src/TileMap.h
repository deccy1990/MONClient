#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <cstdint>
#include <string>
#include <vector>

#include "RenderQueue.h"

class SpriteRenderer;
class Camera2D;
class TileResolver;

struct TileLayer
{
    std::string name;
    std::vector<uint32_t> tiles;
    bool visible = true;
    bool renderable = true;
};

/*
    TileMap
    -------
    Stores raw TMX gids in multiple 2D layers and draws them using a TileResolver.
*/
class TileMap
{
public:
    TileMap(int width, int height, int tileWidthPx, int tileHeightPx);

    void AddLayer(const std::string& name, const std::vector<uint32_t>& tiles, bool visible, bool renderable);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }

    int GetTileWidthPx() const { return mTileWidthPx; }
    int GetTileHeightPx() const { return mTileHeightPx; }

    void DrawGround(SpriteRenderer& renderer,
        const TileResolver& resolver,
        const Camera2D& camera,
        const glm::ivec2& viewportSizePx,
        float animationTimeMs) const;

    void AppendOccluders(RenderQueue& queue,
        const TileResolver& resolver,
        const Camera2D& camera,
        const glm::ivec2& viewportSizePx,
        float animationTimeMs) const;

    void DrawOverhead(SpriteRenderer& renderer,
        const TileResolver& resolver,
        const Camera2D& camera,
        const glm::ivec2& viewportSizePx,
        float animationTimeMs) const;

private:
    int Index(int x, int y) const { return y * mWidth + x; }

    uint32_t GetLayerTile(const TileLayer& layer, int x, int y) const;

private:
    int mWidth = 0;
    int mHeight = 0;

    int mTileWidthPx = 0;
    int mTileHeightPx = 0;

    std::vector<TileLayer> mLayers;
};
