#include "TileSet.h"

#include <cmath>

TileSet::TileSet(int atlasWidthPx, int atlasHeightPx, int tileWidthPx, int tileHeightPx)
    : mAtlasW(atlasWidthPx), mAtlasH(atlasHeightPx), mTileW(tileWidthPx), mTileH(tileHeightPx)
{
    mCols = (mTileW > 0) ? (mAtlasW / mTileW) : 0;
    mRows = (mTileH > 0) ? (mAtlasH / mTileH) : 0;
}

void TileSet::SetAnimations(const std::unordered_map<int, TileAnimation>& animations)
{
    mAnimations = animations;
}

int TileSet::ResolveTileId(int tileId, float animationTimeMs) const
{
    const auto it = mAnimations.find(tileId);
    if (it == mAnimations.end())
        return tileId;

    const TileAnimation& anim = it->second;
    if (anim.frames.empty() || anim.totalDurationMs <= 0)
        return tileId;

    const int totalDuration = anim.totalDurationMs;
    const int timeInCycle = static_cast<int>(std::floor(animationTimeMs)) % totalDuration;

    int accumulated = 0;
    for (const AnimationFrame& frame : anim.frames)
    {
        accumulated += frame.durationMs;
        if (timeInCycle < accumulated)
            return frame.tileId;
    }

    return anim.frames.back().tileId;
}

void TileSet::GetUV(int tileId, glm::vec2& outUvMin, glm::vec2& outUvMax) const
{
    if (mCols <= 0 || mRows <= 0 || mAtlasW <= 0 || mAtlasH <= 0)
    {
        outUvMin = { 0.0f, 0.0f };
        outUvMax = { 1.0f, 1.0f };
        return;
    }

    const int col = tileId % mCols;
    const int rowFromTop = tileId / mCols; // authored top->bottom

    // Pixel rect in atlas (authored coordinates, top-left origin)
    const int x0 = col * mTileW;
    const int y0_fromTop = rowFromTop * mTileH;

    // Convert to normalized UVs.
    // Because stb_image is flipped vertically, we treat row 0 as "top row"
    // by flipping V in UV space:
    //
    // vTop    = 1 - (y0 / atlasH)
    // vBottom = 1 - ((y0 + tileH) / atlasH)
    //
    const float u0 = x0 / (float)mAtlasW;
    const float u1 = (x0 + mTileW) / (float)mAtlasW;

    const float v1 = 1.0f - (y0_fromTop / (float)mAtlasH);               // top
    const float v0 = 1.0f - ((y0_fromTop + mTileH) / (float)mAtlasH);    // bottom

    outUvMin = { u0, v0 }; // (u0, bottom)
    outUvMax = { u1, v1 }; // (u1, top)
}
