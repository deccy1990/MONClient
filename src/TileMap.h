#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

class SpriteRenderer;
class Camera2D;

/*
    TileMap
    -------
    Minimal tile map:
      - Stores tile IDs in a 2D grid (width x height)
      - Draws tiles using your SpriteRenderer + Camera2D
      - Uses a single texture for all tiles for now (fast to start)
        (Later: texture atlas / tileset with per-tile UVs)
*/
class TileMap
{
public:
    TileMap(int width, int height, int tileSizePx);

    int GetWidth() const { return mWidth; }
    int GetHeight() const { return mHeight; }
    int GetTileSizePx() const { return mTileSizePx; }

    // Set tile ID at (x,y). 0 can mean "empty".
    void SetTile(int x, int y, int tileId);

    // Get tile ID at (x,y). Returns 0 for out of bounds.
    int GetTile(int x, int y) const;

    /*
        Draw the map.

        Parameters:
          - renderer: your SpriteRenderer used to draw sprites
          - tileTexture: OpenGL texture used for tiles (single tile image for now)
          - camera: camera controlling which part of the world is visible
          - viewportSizePx: framebuffer size in pixels (for culling)
    */
    void Draw(SpriteRenderer& renderer,
        GLuint tileTexture,
        const Camera2D& camera,
        const glm::ivec2& viewportSizePx) const;

private:
    int Index(int x, int y) const { return y * mWidth + x; }

private:
    int mWidth = 0;
    int mHeight = 0;
    int mTileSizePx = 0;
    std::vector<int> mTiles; // tile IDs
};
