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

    glm::vec2 uvMin, uvMax;
    runtime.tileset.GetUV(resolvedId, uvMin, uvMax);

    outResolved.textureId = runtime.textureId;
    outResolved.uvMin = uvMin;
    outResolved.uvMax = uvMax;
    outResolved.tilesetIndex = tilesetIndex;
    outResolved.localId = resolvedId;

    return true;
}
