#include "TileMap.h"

#include "Camera2d.h"
#include "RenderQueue.h"
#include "SpriteRenderer.h"
#include "TileResolver.h"
#include "TileMath.h"


namespace
{
glm::vec2 ComputeTileTopLeftWorldPos(int tileX, int tileY, float tileWidthPx, float tileHeightPx, const glm::vec2& mapOrigin)
{
    const float halfW = tileWidthPx * 0.5f;
    const float halfH = tileHeightPx * 0.5f;
    float isoX = (float)(tileX - tileY) * halfW;
    float isoY = (float)(tileX + tileY) * halfH;
    return glm::vec2(isoX, isoY) + mapOrigin;
}
}

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

void TileMap::DrawGround(SpriteRenderer& renderer,
    const TileResolver& resolver,
    const Camera2D& camera,
    const glm::ivec2& viewportSizePx,
    float animationTimeMs) const
{
    const glm::vec2 mapOrigin(viewportSizePx.x * 0.5f, 60.0f);
    glm::vec2 baseSize((float)mTileWidthPx, (float)mTileHeightPx);

    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            glm::vec2 worldPos = ComputeTileTopLeftWorldPos(x, y, (float)mTileWidthPx, (float)mTileHeightPx, mapOrigin);

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

                glm::vec2 drawSize = resolved.sizePx;
                if (drawSize.x <= 0.0f)
                    drawSize.x = baseSize.x;
                if (drawSize.y <= 0.0f)
                    drawSize.y = baseSize.y;

                glm::vec2 drawPos = worldPos;
                drawPos.y -= (drawSize.y - baseSize.y);

                renderer.Draw(resolved.textureId, drawPos, drawSize, camera, resolved.uvMin, resolved.uvMax);
            }
        }
    }
}

void TileMap::AppendOccluders(RenderQueue& queue,
    const TileResolver& resolver,
    const Camera2D& camera,
    const glm::ivec2& viewportSizePx,
    float animationTimeMs) const
{
    (void)camera;
    const glm::vec2 mapOrigin(viewportSizePx.x * 0.5f, 60.0f);
    glm::vec2 baseSize((float)mTileWidthPx, (float)mTileHeightPx);

    for (int y = 0; y < mHeight; ++y)
    {
        for (int x = 0; x < mWidth; ++x)
        {
            glm::vec2 worldPos = ComputeTileTopLeftWorldPos(x, y, (float)mTileWidthPx, (float)mTileHeightPx, mapOrigin);

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

                glm::vec2 drawSize = resolved.sizePx;
                if (drawSize.x <= 0.0f)
                    drawSize.x = baseSize.x;
                if (drawSize.y <= 0.0f)
                    drawSize.y = baseSize.y;

                glm::vec2 drawPos = worldPos;
                drawPos.y -= (drawSize.y - baseSize.y);

                RenderCmd cmd{};
                cmd.texture = resolved.textureId;
                cmd.posPx = drawPos;
                cmd.sizePx = drawSize;
                cmd.uvMin = resolved.uvMin;
                cmd.uvMax = resolved.uvMax;

                glm::vec2 feetWorld = cmd.posPx + glm::vec2(cmd.sizePx.x * 0.5f, cmd.sizePx.y);
                cmd.depthKey = DepthFromFeetWorldY(feetWorld.y);

                queue.Push(cmd);
            }
        }
    }
}

void TileMap::DrawOverhead(SpriteRenderer& renderer,
    const TileResolver& resolver,
    const Camera2D& camera,
    const glm::ivec2& viewportSizePx,
    float animationTimeMs) const
{
    DrawGround(renderer, resolver, camera, viewportSizePx, animationTimeMs);
}
