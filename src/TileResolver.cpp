#include "TileResolver.h"

#include <algorithm>

TileResolver::TileResolver(const std::vector<TilesetRuntime>& tilesets)
    : mTilesets(tilesets)
{
}

int TileResolver::FindTilesetIndex(uint32_t gid) const
{
    int bestIndex = -1;
    int bestFirstGid = -1;

    for (size_t i = 0; i < mTilesets.size(); ++i)
    {
        const TilesetDef& def = mTilesets[i].def;
        if (def.firstGid <= static_cast<int>(gid) && def.firstGid > bestFirstGid)
        {
            bestFirstGid = def.firstGid;
            bestIndex = static_cast<int>(i);
        }
    }

    return bestIndex;
}

bool TileResolver::Resolve(uint32_t gid, float animationTimeMs, ResolvedTile& outResolved) const
{
    if (gid == 0)
        return false;

    const int tilesetIndex = FindTilesetIndex(gid);
    if (tilesetIndex < 0)
        return false;

    const TilesetRuntime& runtime = mTilesets[tilesetIndex];
    const TilesetDef& def = runtime.def;

    const int localId = static_cast<int>(gid) - def.firstGid;
    if (localId < 0)
        return false;

    if (def.tileCount > 0 && localId >= def.tileCount)
        return false;

    const int resolvedId = runtime.tileset.ResolveTileId(localId, animationTimeMs);

    outResolved.tilesetIndex = tilesetIndex;
    outResolved.localId = resolvedId;
    outResolved.isFullTexture = false;
    outResolved.sizePx = glm::vec2(static_cast<float>(def.tileW), static_cast<float>(def.tileH));

    if (def.isImageCollection)
    {
        const auto imageIt = def.tileImages.find(resolvedId);
        if (imageIt == def.tileImages.end())
            return false;

        const auto textureIt = runtime.tileTextures.find(resolvedId);
        if (textureIt == runtime.tileTextures.end())
            return false;

        outResolved.textureId = textureIt->second;
        outResolved.sizePx = glm::vec2(static_cast<float>(imageIt->second.w),
            static_cast<float>(imageIt->second.h));

        outResolved.uvMin = glm::vec2(0.0f, 0.0f);
        outResolved.uvMax = glm::vec2(1.0f, 1.0f);
        outResolved.isFullTexture = true;
    }
    else
    {
        outResolved.textureId = runtime.textureId;
        glm::vec2 uvMin, uvMax;
        runtime.tileset.GetUV(resolvedId, uvMin, uvMax);
        outResolved.uvMin = uvMin;
        outResolved.uvMax = uvMax;
    }

    return true;
}
