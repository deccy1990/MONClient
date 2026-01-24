#include "TmxLoader.h"

#include "tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace
{
    constexpr uint32_t TMX_FLIPPED_HORIZONTALLY_FLAG = 0x80000000u;
    constexpr uint32_t TMX_FLIPPED_VERTICALLY_FLAG = 0x40000000u;
    constexpr uint32_t TMX_FLIPPED_DIAGONALLY_FLAG = 0x20000000u;
    constexpr uint32_t TMX_GID_MASK =
        ~(TMX_FLIPPED_HORIZONTALLY_FLAG | TMX_FLIPPED_VERTICALLY_FLAG | TMX_FLIPPED_DIAGONALLY_FLAG);

    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    std::vector<int> ParseCsvTiles(const char* csvText, int expectedCount)
    {
        std::vector<int> tiles;
        tiles.reserve(expectedCount);

        if (!csvText)
            return tiles;

        std::stringstream ss(csvText);
        std::string cell;

        while (std::getline(ss, cell, ','))
        {
            // Trim whitespace/newlines around each token.
            const auto begin = cell.find_first_not_of(" \t\r\n");
            if (begin == std::string::npos)
                continue;

            const auto end = cell.find_last_not_of(" \t\r\n");
            const std::string trimmed = cell.substr(begin, end - begin + 1);

            tiles.push_back(std::stoi(trimmed));
        }

        return tiles;
    }

    std::unordered_map<std::string, std::string> ParseProperties(tinyxml2::XMLElement* propertiesElement)
    {
        std::unordered_map<std::string, std::string> props;
        if (!propertiesElement)
            return props;

        for (tinyxml2::XMLElement* prop = propertiesElement->FirstChildElement("property");
            prop;
            prop = prop->NextSiblingElement("property"))
        {
            const char* name = prop->Attribute("name");
            if (!name)
                continue;

            const char* valueAttr = prop->Attribute("value");
            if (valueAttr)
            {
                props[name] = valueAttr;
                continue;
            }

            const char* textValue = prop->GetText();
            props[name] = textValue ? textValue : "";
        }

        return props;
    }

    bool GetBoolAttribute(tinyxml2::XMLElement* element, const char* name, bool defaultValue)
    {
        const char* attr = element->Attribute(name);
        if (!attr)
            return defaultValue;

        char* end = nullptr;
        const long value = std::strtol(attr, &end, 10);
        if (end == attr)
            return defaultValue;

        return value != 0;
    }

    float GetFloatAttribute(tinyxml2::XMLElement* element, const char* name, float defaultValue)
    {
        const char* attr = element->Attribute(name);
        if (!attr)
            return defaultValue;

        char* end = nullptr;
        const float value = std::strtof(attr, &end);
        if (end == attr)
            return defaultValue;

        return value;
    }

    int GidToLocal(uint32_t gid, int firstGid, int atlasTileCount)
    {
        if (gid == 0)
            return -1;

        const int localId = static_cast<int>(gid) - firstGid;
        if (localId < 0)
            return -1;

        if (atlasTileCount > 0 && localId >= atlasTileCount)
            return -1;

        return localId;
    }
}

glm::vec2 ObjectPixelsToGrid(const glm::vec2& objectPosPx, int tileWidth, int tileHeight)
{
    const float halfW = tileWidth * 0.5f;
    const float halfH = tileHeight * 0.5f;

    if (halfW <= 0.0f || halfH <= 0.0f)
        return glm::vec2(0.0f);

    // Tiled's isometric object coordinates place the origin at the bottom-center of a tile.
    // Convert that bottom-center to our tile top-left convention before inverting the iso transform.
    const float isoXTopLeft = objectPosPx.x - halfW;
    const float isoYTopLeft = objectPosPx.y - static_cast<float>(tileHeight);

    const float gridX = (isoXTopLeft / halfW + isoYTopLeft / halfH) * 0.5f;
    const float gridY = (isoYTopLeft / halfH - isoXTopLeft / halfW) * 0.5f;

    // Place the player at the center of the tile for smoother collision behavior.
    return glm::vec2(gridX + 0.5f, gridY + 0.5f);
}

bool LoadTmxMap(const std::string& tmxPath, LoadedMap& outMap)
{
    using namespace tinyxml2;

    outMap = LoadedMap{};

    XMLDocument doc;
    const XMLError err = doc.LoadFile(tmxPath.c_str());
    if (err != XML_SUCCESS)
    {
        std::cerr << "Failed to load TMX: " << tmxPath << " error=" << doc.ErrorStr() << "\n";
        return false;
    }

    XMLElement* map = doc.FirstChildElement("map");
    if (!map)
    {
        std::cerr << "TMX missing <map> root element\n";
        return false;
    }

    outMap.width = map->IntAttribute("width");
    outMap.height = map->IntAttribute("height");
    outMap.tileWidth = map->IntAttribute("tilewidth");
    outMap.tileHeight = map->IntAttribute("tileheight");

    const int expectedCount = outMap.width * outMap.height;

    const std::filesystem::path tmxFsPath(tmxPath);
    const std::filesystem::path tmxDir = tmxFsPath.parent_path();

    // --- Tileset and TSX animations
    XMLElement* tileset = map->FirstChildElement("tileset");
    if (!tileset)
    {
        std::cerr << "TMX missing <tileset> element\n";
        return false;
    }

    outMap.firstGid = tileset->IntAttribute("firstgid", 1);

    const char* tsxSource = tileset->Attribute("source");
    if (!tsxSource)
    {
        std::cerr << "Tileset is expected to be external (.tsx) but no source was provided\n";
        return false;
    }

    const std::filesystem::path tsxPath = (tmxDir / tsxSource).lexically_normal();

    XMLDocument tsxDoc;
    const XMLError tsxErr = tsxDoc.LoadFile(tsxPath.string().c_str());
    if (tsxErr != XML_SUCCESS)
    {
        std::cerr << "Failed to load TSX: " << tsxPath << " error=" << tsxDoc.ErrorStr() << "\n";
        return false;
    }

    XMLElement* tsxTileset = tsxDoc.FirstChildElement("tileset");
    if (!tsxTileset)
    {
        std::cerr << "TSX missing <tileset> root element\n";
        return false;
    }

    outMap.atlasTileCount = tsxTileset->IntAttribute("tilecount", 0);
    if (outMap.atlasTileCount <= 0)
    {
        const int columns = tsxTileset->IntAttribute("columns", 0);
        const int tileWidth = tsxTileset->IntAttribute("tilewidth", outMap.tileWidth);
        const int tileHeight = tsxTileset->IntAttribute("tileheight", outMap.tileHeight);

        int rows = 0;
        if (columns > 0 && tileWidth > 0 && tileHeight > 0)
        {
            const int imageWidth = tsxTileset->FirstChildElement("image")
                ? tsxTileset->FirstChildElement("image")->IntAttribute("width", 0)
                : 0;
            const int imageHeight = tsxTileset->FirstChildElement("image")
                ? tsxTileset->FirstChildElement("image")->IntAttribute("height", 0)
                : 0;

            if (imageWidth > 0 && imageHeight > 0)
                rows = (imageHeight / tileHeight);
        }

        if (columns > 0 && rows > 0)
            outMap.atlasTileCount = columns * rows;
    }

    XMLElement* image = tsxTileset->FirstChildElement("image");
    if (!image)
    {
        std::cerr << "TSX missing <image> element\n";
        return false;
    }

    const char* imageSource = image->Attribute("source");
    if (!imageSource)
    {
        std::cerr << "TSX image missing source attribute\n";
        return false;
    }

    const std::filesystem::path tsxDir = tsxPath.parent_path();
    outMap.tilesetImagePath = (tsxDir / imageSource).lexically_normal().string();

    for (XMLElement* tile = tsxTileset->FirstChildElement("tile"); tile; tile = tile->NextSiblingElement("tile"))
    {
        const int tileId = tile->IntAttribute("id", -1);
        if (tileId < 0)
            continue;

        XMLElement* animation = tile->FirstChildElement("animation");
        if (!animation)
            continue;

        TileAnimation anim{};

        for (XMLElement* frame = animation->FirstChildElement("frame"); frame; frame = frame->NextSiblingElement("frame"))
        {
            AnimationFrame animFrame{};
            animFrame.tileId = frame->IntAttribute("tileid", tileId);
            animFrame.durationMs = frame->IntAttribute("duration", 0);

            if (animFrame.durationMs <= 0)
                animFrame.durationMs = 100;

            anim.totalDurationMs += animFrame.durationMs;
            anim.frames.push_back(animFrame);
        }

        if (!anim.frames.empty() && anim.totalDurationMs > 0)
            outMap.animations[tileId] = anim;
    }

    // --- Tile layers
    outMap.mapData.width = outMap.width;
    outMap.mapData.height = outMap.height;
    outMap.mapData.tileW = outMap.tileWidth;
    outMap.mapData.tileH = outMap.tileHeight;
    outMap.mapData.firstGid = outMap.firstGid;

    std::vector<uint8_t> collisionTiles(expectedCount, 0);
    bool hasCollisionLayer = false;

    for (XMLElement* layer = map->FirstChildElement("layer"); layer; layer = layer->NextSiblingElement("layer"))
    {
        LoadedTileLayer loadedLayer{};
        const char* name = layer->Attribute("name");
        loadedLayer.name = name ? name : "";
        loadedLayer.visible = GetBoolAttribute(layer, "visible", true);

        const std::string lowerName = ToLower(loadedLayer.name);
        loadedLayer.isCollision = (lowerName == "collision");

        XMLElement* data = layer->FirstChildElement("data");
        if (!data)
        {
            std::cerr << "Layer '" << loadedLayer.name << "' missing <data>\n";
            continue;
        }

        const char* encoding = data->Attribute("encoding");
        if (!encoding || std::string(encoding) != "csv")
        {
            std::cerr << "Layer '" << loadedLayer.name << "' is not CSV encoded\n";
            continue;
        }

        std::vector<int> rawGids = ParseCsvTiles(data->GetText(), expectedCount);
        if ((int)rawGids.size() != expectedCount)
        {
            std::cerr << "Layer '" << loadedLayer.name << "' size mismatch. Expected "
                << expectedCount << " entries but got " << rawGids.size() << "\n";
            continue;
        }

        if (loadedLayer.isCollision)
        {
            hasCollisionLayer = true;
            for (int i = 0; i < expectedCount; ++i)
            {
                const uint32_t gid = static_cast<uint32_t>(rawGids[i]) & TMX_GID_MASK;
                collisionTiles[i] = gid == 0 ? 0 : 1;
            }
            continue;
        }

        std::vector<int> tiles(expectedCount, -1);
        for (int i = 0; i < expectedCount; ++i)
        {
            const uint32_t gid = static_cast<uint32_t>(rawGids[i]) & TMX_GID_MASK;
            tiles[i] = GidToLocal(gid, outMap.firstGid, outMap.atlasTileCount);
        }

        if (lowerName == "ground")
            outMap.mapData.ground = std::move(tiles);
        else if (lowerName == "walls")
            outMap.mapData.walls = std::move(tiles);
        else if (lowerName == "overhead")
            outMap.mapData.overhead = std::move(tiles);
    }

    if (hasCollisionLayer)
        outMap.mapData.collision = std::move(collisionTiles);
    else
        outMap.mapData.collision.assign(expectedCount, 0);

    // --- Object layer: <objectgroup name="objects">
    for (XMLElement* objectGroup = map->FirstChildElement("objectgroup"); objectGroup; objectGroup = objectGroup->NextSiblingElement("objectgroup"))
    {
        const char* groupName = objectGroup->Attribute("name");
        if (!groupName || ToLower(groupName) != "objects")
            continue;

        for (XMLElement* object = objectGroup->FirstChildElement("object"); object; object = object->NextSiblingElement("object"))
        {
            MapObject mapObject{};
            mapObject.id = object->IntAttribute("id", 0);

            const char* name = object->Attribute("name");
            const char* type = object->Attribute("type");
            mapObject.name = name ? name : "";
            mapObject.type = type ? type : "";

            mapObject.positionPx = glm::vec2(
                GetFloatAttribute(object, "x", 0.0f),
                GetFloatAttribute(object, "y", 0.0f));
            mapObject.sizePx = glm::vec2(
                GetFloatAttribute(object, "width", 0.0f),
                GetFloatAttribute(object, "height", 0.0f));

            mapObject.properties = ParseProperties(object->FirstChildElement("properties"));
            outMap.objects.push_back(std::move(mapObject));
        }
    }

    std::cout << "TMX loaded: " << tmxPath << "\n";
    std::cout << "  map size: " << outMap.width << " x " << outMap.height << " tiles\n";
    std::cout << "  tile size: " << outMap.tileWidth << " x " << outMap.tileHeight << " px\n";
    std::cout << "  layers: ground=" << (outMap.mapData.HasGround() ? "yes" : "no")
              << " walls=" << (outMap.mapData.HasWalls() ? "yes" : "no")
              << " overhead=" << (outMap.mapData.HasOverhead() ? "yes" : "no")
              << " collision=" << (hasCollisionLayer ? "yes" : "no") << "\n";
    std::cout << "  objects: " << outMap.objects.size() << "\n";
    std::cout << "  animated tiles: " << outMap.animations.size() << "\n";

    return true;
}
