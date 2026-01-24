#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct AnimationFrame
{
    int tileId = 0;      // 0-based tile id within the tileset
    int durationMs = 0;  // duration in milliseconds
};

struct TileAnimation
{
    std::vector<AnimationFrame> frames;
    int totalDurationMs = 0;
};

struct MapObject
{
    int id = 0;
    std::string name;
    std::string type;
    glm::vec2 positionPx{ 0.0f, 0.0f };
    glm::vec2 sizePx{ 0.0f, 0.0f };
    std::unordered_map<std::string, std::string> properties;
};

struct TilesetDef
{
    int firstGid = 1;
    int tileCount = 0;
    int columns = 0;
    int tileW = 0;
    int tileH = 0;
    std::string imagePath;
    int imageW = 0;
    int imageH = 0;
    std::unordered_map<int, TileAnimation> animations;
};

struct MapData
{
    int width = 0;
    int height = 0;
    int tileW = 0;
    int tileH = 0;

    std::vector<TilesetDef> tilesets;

    std::vector<uint32_t> groundGids;
    std::vector<uint32_t> wallsGids;
    std::vector<uint32_t> overheadGids;

    std::vector<uint8_t> collision;
    std::vector<MapObject> objects;

    bool HasGround() const { return (int)groundGids.size() == width * height; }
    bool HasWalls() const { return (int)wallsGids.size() == width * height; }
    bool HasOverhead() const { return (int)overheadGids.size() == width * height; }
    bool HasCollision() const { return (int)collision.size() == width * height; }
};

struct LoadedMap
{
    MapData mapData;
};

bool LoadTmxMap(const std::string& tmxPath, LoadedMap& outMap);

glm::vec2 ObjectPixelsToGrid(const glm::vec2& objectPosPx, int tileWidth, int tileHeight);
