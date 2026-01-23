#include "TileMap.h"

#include "SpriteRenderer.h"
#include "Camera2D.h"
#include "TileSet.h"
#include "Player.h"


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
        return -1;

    return mTiles[Index(x, y)];
}


void TileMap::Draw(SpriteRenderer& renderer,
    GLuint atlasTexture,
    const TileSet& tileset,
    const Camera2D& camera,
    const glm::ivec2& viewportSizePx,
    const Player* player) const
{
    int playerSum = -1;
    if (player)
    {
        playerSum = (int)std::floor(player->GetDepthKey());
    }

    // NOTE: camera is top-left in world pixels.
    // We are currently not doing isometric-aware culling; we draw the full map.
    // (We'll add culling later once everything else is stable.)

    // --- Draw tiles in isometric depth order (diagonals back -> front)
    for (int sum = 0; sum <= (mWidth - 1) + (mHeight - 1); ++sum)
    {
        int xStart = std::max(0, sum - (mHeight - 1));
        int xEnd = std::min(mWidth - 1, sum);

        for (int x = xStart; x <= xEnd; ++x)
        {
            int y = sum - x;

            int id = GetTile(x, y);
            if (id < 0)
                continue;

            const float halfW = mTileWidthPx * 0.5f;
            const float halfH = mTileHeightPx * 0.5f;

            // Draw player after finishing this diagonal (correct iso depth)
            if (player && sum == playerSum)
            {
                const float halfW = mTileWidthPx * 0.5f;
                const float halfH = mTileHeightPx * 0.5f;

                glm::vec2 gp = player->GetGridPos();

                float isoX = (gp.x - gp.y) * halfW;
                float isoY = (gp.x + gp.y) * halfH;

                glm::vec2 tileTopLeftWorld(isoX, isoY);
                tileTopLeftWorld.x += viewportSizePx.x * 0.5f;
                tileTopLeftWorld.y += 60.0f;

                player->DrawOnTile(renderer, camera, tileTopLeftWorld, mTileWidthPx, mTileHeightPx);
            }


            // Grid -> isometric world position (tile top-left)
            float isoX = (float)(x - y) * halfW;
            float isoY = (float)(x + y) * halfH;

            glm::vec2 worldPos(isoX, isoY);

            // Map origin (must match main.cpp camera follow origin)
            worldPos.x += viewportSizePx.x * 0.5f;
            worldPos.y += 60.0f;

            glm::vec2 size((float)mTileWidthPx, (float)mTileHeightPx);

            // UV lookup from atlas
            glm::vec2 uvMin, uvMax;
            tileset.GetUV(id, uvMin, uvMax);

            // Draw tile
            renderer.Draw(atlasTexture, worldPos, size, camera, uvMin, uvMax);

            // Draw player immediately after its tile (correct isometric depth)
            if (player &&
                player->GetTilePos().x == x &&
                player->GetTilePos().y == y)
            {
                player->DrawOnTile(renderer, camera, worldPos, mTileWidthPx, mTileHeightPx);
            }
        }
    }
}
