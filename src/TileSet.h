#pragma once

#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>

#include "TmxLoader.h"

/*
    TileSet
    -------
    Maps tileId (0-based) to UV coords inside a uniform grid atlas.

    IMPORTANT:
    - We are loading atlas images without flipping them on load.
    - Most atlas images are authored with tile (0) at TOP-LEFT.
      So we flip V when computing UVs, so tileId 0 is top-left in the image.
*/
class TileSet
{
public:
    TileSet(int atlasWidthPx, int atlasHeightPx, int tileWidthPx, int tileHeightPx);

    void SetAnimations(const std::unordered_map<int, TileAnimation>& animations);

    // Resolve an animated tile to the correct frame based on accumulated time (ms).
    int ResolveTileId(int tileId, float animationTimeMs) const;

    // tileId: 0-based index into atlas grid (left->right, top->bottom)
    void GetUV(int tileId, glm::vec2& outUvMin, glm::vec2& outUvMax) const;

private:
    int mAtlasW = 0;
    int mAtlasH = 0;
    int mTileW = 0;
    int mTileH = 0;
    int mCols = 0;
    int mRows = 0;

    std::unordered_map<int, TileAnimation> mAnimations;
};
