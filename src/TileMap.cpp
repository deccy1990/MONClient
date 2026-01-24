#include "TileMap.h"

#include "Camera2d.h"
#include "Player.h"
#include "SpriteRenderer.h"
#include "TileResolver.h"

#include <algorithm>
#include <cmath>

TileMap::TileMap(int width, int height, int tileWidthPx, int tileHeightPx)
    : mWidth(width)
    , mHeight(height)
    , mTileWidthPx(tileWidthPx)
    , mTileHeightPx(tileHeightPx)
{
}

void TileMap::AddLayer(const std::string& name, const std::vector<uint32_t>& tiles, bool visible, bool renderable)
{
    if ((int)tiles.size() != mWidth * mHeight)
        return;

    TileLayer layer{};
    layer.name = name;
    layer.tiles = tiles;
    layer.visible = visible;
    layer.renderable = renderable;

    mLayers.push_back(std::move(layer));
}

uint32_t TileMap::GetLayerTile(const TileLayer& layer, int x, int y) const
{
    if (x < 0 || x >= mWidth || y < 0 || y >= mHeight)
        return 0;

    return layer.tiles[Index(x, y)];
}

void TileMap::Draw(SpriteRenderer& renderer,
    const TileResolver& resolver,
    const Camera2D& camera,
    const glm::ivec2& viewportSizePx,
    const Player* player,
    float animationTimeMs) const
{
    int playerSum = -1;
    if (player)
    {
        const float halfW = mTileWidthPx * 0.5f;
        const float halfH = mTileHeightPx * 0.5f;

        glm::vec2 gp = player->GetGridPos();

        float isoX = (gp.x - gp.y) * halfW;
        float isoY = (gp.x + gp.y) * halfH;

        glm::vec2 tileTopLeftWorld(isoX, isoY);

        glm::vec2 mapOrigin(viewportSizePx.x * 0.5f, 60.0f);
        tileTopLeftWorld += mapOrigin;

        glm::vec2 feetWorld = tileTopLeftWorld + glm::vec2(mTileWidthPx * 0.5f, (float)mTileHeightPx);
        playerSum = (int)std::floor((feetWorld.y - mapOrigin.y) / halfH);
    }

    playerSum = std::clamp(playerSum, 0, (mWidth - 1) + (mHeight - 1));

    for (int sum = 0; sum <= (mWidth - 1) + (mHeight - 1); ++sum)
    {
        int xStart = std::max(0, sum - (mHeight - 1));
        int xEnd = std::min(mWidth - 1, sum);

        for (int x = xStart; x <= xEnd; ++x)
        {
            int y = sum - x;

            const float halfW = mTileWidthPx * 0.5f;
            const float halfH = mTileHeightPx * 0.5f;

            if (player && sum == playerSum)
            {
                glm::vec2 gp = player->GetGridPos();

                float isoX = (gp.x - gp.y) * halfW;
                float isoY = (gp.x + gp.y) * halfH;

                glm::vec2 tileTopLeftWorld(isoX, isoY);
                tileTopLeftWorld.x += viewportSizePx.x * 0.5f;
                tileTopLeftWorld.y += 60.0f;

                player->DrawOnTile(renderer, camera, tileTopLeftWorld, mTileWidthPx, mTileHeightPx);
            }

            float isoX = (float)(x - y) * halfW;
            float isoY = (float)(x + y) * halfH;

            glm::vec2 worldPos(isoX, isoY);
            worldPos.x += viewportSizePx.x * 0.5f;
            worldPos.y += 60.0f;

            glm::vec2 size((float)mTileWidthPx, (float)mTileHeightPx);

            for (const TileLayer& layer : mLayers)
            {
                if (!layer.visible || !layer.renderable)
                    continue;

                uint32_t gid = GetLayerTile(layer, x, y);
                if (gid == 0)
                    continue;

                ResolvedTile resolved{};
                if (!resolver.Resolve(gid, animationTimeMs, resolved))
                    continue;

                renderer.Draw(resolved.textureId, worldPos, size, camera, resolved.uvMin, resolved.uvMax);
            }

            if (player &&
                player->GetTilePos().x == x &&
                player->GetTilePos().y == y)
            {
                player->DrawOnTile(renderer, camera, worldPos, mTileWidthPx, mTileHeightPx);
            }
        }
    }
}
