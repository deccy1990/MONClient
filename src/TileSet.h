#pragma once
#include <glm/glm.hpp>

/*
    TileSet
    -------
    Maps tileId (0-based) to UV coords inside a uniform grid atlas.

    IMPORTANT:
    - We are using stbi_set_flip_vertically_on_load(true),
      which makes OpenGL UV origin (0,0) bottom-left.
    - Most atlas images are authored with tile (0) at TOP-LEFT.
      So we flip V when computing UVs, so tileId 0 is top-left in the image.
*/
class TileSet
{
public:
    TileSet(int atlasWidthPx, int atlasHeightPx, int tileWidthPx, int tileHeightPx);

    // tileId: 0-based index into atlas grid (left->right, top->bottom)
    void GetUV(int tileId, glm::vec2& outUvMin, glm::vec2& outUvMax) const;

private:
    int mAtlasW = 0;
    int mAtlasH = 0;
    int mTileW = 0;
    int mTileH = 0;
    int mCols = 0;
    int mRows = 0;
};
