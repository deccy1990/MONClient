#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>

#include <cstdint>
#include <vector>

#include "TileSet.h"
#include "TmxLoader.h"

struct ResolvedTile
{
    GLuint textureId = 0;
    glm::vec2 uvMin{ 0.0f, 0.0f };
    glm::vec2 uvMax{ 0.0f, 0.0f };
    int tilesetIndex = -1;
    int localId = -1;
};

struct TilesetRuntime
{
    TilesetDef def;
    TileSet tileset;
    GLuint textureId = 0;
};

class TileResolver
{
public:
    explicit TileResolver(const std::vector<TilesetRuntime>& tilesets);

    bool Resolve(uint32_t gid, float animationTimeMs, ResolvedTile& outResolved) const;

private:
    int FindTilesetIndex(uint32_t gid) const;

    const std::vector<TilesetRuntime>& mTilesets;
};
