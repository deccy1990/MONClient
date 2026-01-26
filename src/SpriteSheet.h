#pragma once
#include <glm/glm.hpp>

/*
    SpriteSheet
    -----------
    Converts a frame index (0..N-1) into UVs for a uniform grid sprite-sheet.

    Assumptions:
      - frames are laid out in a grid (columns x rows)
      - each frame is frameW x frameH pixels
      - texture size is texW x texH pixels

    Important:
      If you load the sprite-sheet with stbi_set_flip_vertically_on_load(true),
      then "row 0" should mean the TOP row (like your TileSet does).
*/
struct SpriteSheet
{
    int texW = 0;
    int texH = 0;

    int frameW = 0;
    int frameH = 0;

    int cols = 0; // frames across
    int rows = 0; // frames down

    // Set this to true if you loaded the texture with flipY=true.
    // This makes row 0 refer to the TOP row.
    bool flippedYOnLoad = true;

    SpriteSheet() = default;

    SpriteSheet(int textureW, int textureH, int frameWidth, int frameHeight, bool flipYOnLoad)
        : texW(textureW)
        , texH(textureH)
        , frameW(frameWidth)
        , frameH(frameHeight)
        , flippedYOnLoad(flipYOnLoad)
    {
        cols = (frameW > 0) ? (texW / frameW) : 0;
        rows = (frameH > 0) ? (texH / frameH) : 0;
    }

    // frameIndex: 0..(cols*rows - 1)
    void GetUV(int frameIndex, glm::vec2& uvMin, glm::vec2& uvMax) const
    {
        if (cols <= 0 || rows <= 0 || texW <= 0 || texH <= 0)
        {
            uvMin = { 0.0f, 0.0f };
            uvMax = { 1.0f, 1.0f };
            return;
        }

        // Clamp frame index into valid range
        const int maxFrames = cols * rows;
        if (frameIndex < 0) frameIndex = 0;
        if (frameIndex >= maxFrames) frameIndex = maxFrames - 1;

        const int col = frameIndex % cols;
        const int row = frameIndex / cols; // row 0,1,2... in "sheet order"

        const float u0 = (col * frameW) / (float)texW;
        const float u1 = ((col + 1) * frameW) / (float)texW;

        // V depends on whether you flipped the image at load time.
        // OpenGL UV origin is bottom-left.
        if (flippedYOnLoad)
        {
            // If image was flipped on load, treat row 0 as the TOP row
            const float v1 = 1.0f - (row * frameH) / (float)texH;
            const float v0 = 1.0f - ((row + 1) * frameH) / (float)texH;

            uvMin = { u0, v0 };
            uvMax = { u1, v1 };
        }
        else
        {
            // If image was NOT flipped, treat row 0 as the BOTTOM row
            const float v0 = (row * frameH) / (float)texH;
            const float v1 = ((row + 1) * frameH) / (float)texH;

            uvMin = { u0, v0 };
            uvMax = { u1, v1 };
        }
    }
};
