#include "TileMap.h"

#include "SpriteRenderer.h"
#include "Camera2D.h"
#include "TileSet.h"

#include <algorithm> // std::clamp
#include <cmath>     // std::floor

TileMap::TileMap(int width, int height, int tileWidthPx, int tileHeightPx)
    : mWidth(width),
    mHeight(height),
    mTileWidthPx(tileWidthPx),
    mTileHeightPx(tileHeightPx),
    mTiles(width* height, -1)
{
}

void TileMap::SetTile(int x, int y, int tileId)
{
    if (x < 0 || x >= mWidth || y < 0 || y >= mHeight)
        return;

    mTiles[Index(x, y)] = tileId;
}

int TileMap::GetTile(int x, int y) const
{
    if (x < 0 || x >= mWidth || y < 0 || y >= mHeight)
        return 0;

    return mTiles[Index(x, y)];
}

void TileMap::Draw(SpriteRenderer& renderer,
    GLuint atlasTexture,
    const TileSet& tileset,
    const Camera2D& camera,
    const glm::ivec2& viewportSizePx) const
{
    // --- Visible world rectangle (camera is top-left in world pixels)
    const glm::vec2 camPos = camera.GetPosition();

    const float viewLeft = camPos.x;
    const float viewTop = camPos.y;
    const float viewRight = camPos.x + (float)viewportSizePx.x;
    const float viewBottom = camPos.y + (float)viewportSizePx.y;

    // --- Convert visible world pixels -> tile index range
    int minTileX = (int)std::floor(viewLeft / (float)mTileWidthPx);
    int minTileY = (int)std::floor(viewTop / (float)mTileHeightPx);
    int maxTileX = (int)std::floor(viewRight / (float)mTileWidthPx);
    int maxTileY = (int)std::floor(viewBottom / (float)mTileHeightPx);

    // Small margin to avoid popping at edges
    minTileX -= 1; minTileY -= 1;
    maxTileX += 1; maxTileY += 1;

    // Clamp to map bounds
    minTileX = std::clamp(minTileX, 0, mWidth - 1);
    minTileY = std::clamp(minTileY, 0, mHeight - 1);
    maxTileX = std::clamp(maxTileX, 0, mWidth - 1);
    maxTileY = std::clamp(maxTileY, 0, mHeight - 1);

    // --- Draw visible tiles
    for (int y = minTileY; y <= maxTileY; ++y)
    {
        for (int x = minTileX; x <= maxTileX; ++x)
        {
            int id = GetTile(x, y);

            // Use -1 for "empty". Tile 0 is a valid atlas tile.
            if (id < 0)
                continue;

            // World position in pixels (top-left of tile)
            glm::vec2 worldPos((float)(x * mTileWidthPx), (float)(y * mTileHeightPx));
            glm::vec2 size((float)mTileWidthPx, (float)mTileHeightPx);

            // NEW: get atlas UVs for this tile id
            glm::vec2 uvMin, uvMax;
            tileset.GetUV(id, uvMin, uvMax);

            // NEW: draw with atlas + UVs
            renderer.Draw(atlasTexture, worldPos, size, camera, uvMin, uvMax);
        }
    }
}
