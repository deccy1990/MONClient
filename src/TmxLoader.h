#pragma once

#include <glm/glm.hpp>

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

struct LoadedTileLayer
{
    std::string name;
    std::vector<int> tiles; // 0-based tile ids, -1 for empty
    bool visible = true;
    bool isCollision = false;
};

struct LoadedMap
{
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;

    int firstGid = 1;
    int atlasTileCount = 0;
    std::string tilesetImagePath;

    std::vector<LoadedTileLayer> layers;
    std::vector<int> collision; // 0 = walkable, 1 = blocked

    std::vector<MapObject> objects;

    std::unordered_map<int, TileAnimation> animations;
};

bool LoadTmxMap(const std::string& tmxPath, LoadedMap& outMap);

glm::vec2 ObjectPixelsToGrid(const glm::vec2& objectPosPx, int tileWidth, int tileHeight);
