#include "TileMap.h"
#include "SpriteRenderer.h"
#include "Camera2D.h"

#include <algorithm> // std::clamp
#include <cmath>     // std::floor


TileMap::TileMap(int width, int height, int tileSizePx)
    : mWidth(width), mHeight(height), mTileSizePx(tileSizePx), mTiles(width* height, 0)
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
    GLuint tileTexture,
    const Camera2D& camera,
    const glm::ivec2& viewportSizePx) const
{

// ------------------------------
// Compute visible tile bounds
// ------------------------------
// Camera position is the world-space top-left of the viewport.
    const glm::vec2 camPos = camera.GetPosition();

    // Visible world rectangle in pixels
    const float viewLeft = camPos.x;
    const float viewTop = camPos.y;
    const float viewRight = camPos.x + (float)viewportSizePx.x;
    const float viewBottom = camPos.y + (float)viewportSizePx.y;

    // Convert world pixel bounds -> tile index bounds
    // Use floor for min, and floor for max (then add margin)
    int minTileX = (int)std::floor(viewLeft / (float)mTileSizePx);
    int minTileY = (int)std::floor(viewTop / (float)mTileSizePx);
    int maxTileX = (int)std::floor(viewRight / (float)mTileSizePx);
    int maxTileY = (int)std::floor(viewBottom / (float)mTileSizePx);

    // Margin so tiles at edges don't pop in/out due to rounding
    minTileX -= 1; minTileY -= 1;
    maxTileX += 1; maxTileY += 1;

    // Clamp bounds to map
    minTileX = std::clamp(minTileX, 0, mWidth - 1);
    minTileY = std::clamp(minTileY, 0, mHeight - 1);
    maxTileX = std::clamp(maxTileX, 0, mWidth - 1);
    maxTileY = std::clamp(maxTileY, 0, mHeight - 1);



    // Add a small margin so edges don’t pop
    minTileX -= 1; minTileY -= 1;
    maxTileX += 1; maxTileY += 1;

    // Clamp to map bounds
    minTileX = std::clamp(minTileX, 0, mWidth - 1);
    minTileY = std::clamp(minTileY, 0, mHeight - 1);
    maxTileX = std::clamp(maxTileX, 0, mWidth - 1);
    maxTileY = std::clamp(maxTileY, 0, mHeight - 1);

    // Draw visible tiles
    for (int y = minTileY; y <= maxTileY; ++y)
    {
        for (int x = minTileX; x <= maxTileX; ++x)
        {
            int id = GetTile(x, y);

            // ID 0 means empty tile (nothing drawn)
            if (id == 0)
                continue;

            // World position in pixels (top-left of tile)
            glm::vec2 worldPos((float)(x * mTileSizePx), (float)(y * mTileSizePx));
            glm::vec2 size((float)mTileSizePx, (float)mTileSizePx);

            // For now: all tiles use the same texture.
            // Later: use an atlas and choose UVs based on id.
            renderer.Draw(tileTexture, worldPos, size, camera);
        }
    }
}
