// TmxLoader.cpp (paste-replace whole file)
//
// Fixes:
//  - Only ONE ParseObjectGroup (with group offset support)
//  - ParseNode carries group offsets through nested <group> using offsetx/offsety
//  - Tile objects (gid) stored into mapData.objectInstances with TMX object width/height if present
//  - Doors / Spawns / generic objects also receive the accumulated group offset
//  - No stray references (door/spawn/mapObject) inside ParseNode

#include "TmxLoader.h"

#include "tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

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

    std::string GetStringProp(tinyxml2::XMLElement* props, const char* key)
    {
        if (!props)
            return {};

        for (auto* prop = props->FirstChildElement("property"); prop; prop = prop->NextSiblingElement("property"))
        {
            const char* name = prop->Attribute("name");
            if (!name || std::string(name) != key)
                continue;

            const char* valueAttr = prop->Attribute("value");
            if (valueAttr)
                return valueAttr;

            const char* textValue = prop->GetText();
            return textValue ? textValue : "";
        }

        return {};
    }

    bool PropertyIsTrue(const std::unordered_map<std::string, std::string>& props, const std::string& key)
    {
        auto it = props.find(key);
        if (it == props.end())
            return false;

        std::string value = ToLower(it->second);
        return value == "true" || value == "1" || value == "yes";
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

    const TilesetDef* FindTilesetForGid(const std::vector<TilesetDef>& tilesets, uint32_t gid)
    {
        const TilesetDef* best = nullptr;
        int bestFirstGid = -1;

        for (const TilesetDef& def : tilesets)
        {
            if (def.firstGid <= static_cast<int>(gid) && def.firstGid > bestFirstGid)
            {
                bestFirstGid = def.firstGid;
                best = &def;
            }
        }

        return best;
    }

    bool LoadTilesetFromElement(const tinyxml2::XMLElement* tilesetElement,
        const std::filesystem::path& imageBaseDir,
        int firstGid,
        int fallbackTileW,
        int fallbackTileH,
        TilesetDef& outDef)
    {
        using namespace tinyxml2;

        outDef = TilesetDef{};
        outDef.firstGid = firstGid;
        outDef.tileW = tilesetElement->IntAttribute("tilewidth", fallbackTileW);
        outDef.tileH = tilesetElement->IntAttribute("tileheight", fallbackTileH);
        outDef.columns = tilesetElement->IntAttribute("columns", 0);
        outDef.tileCount = tilesetElement->IntAttribute("tilecount", 0);

        XMLElement* image = tilesetElement->FirstChildElement("image");
        if (image)
        {
            const char* imageSource = image->Attribute("source");
            if (!imageSource)
            {
                std::cerr << "Tileset image missing source attribute\n";
                return false;
            }

            outDef.imageW = image->IntAttribute("width", 0);
            outDef.imageH = image->IntAttribute("height", 0);
            outDef.imagePath = (imageBaseDir / imageSource).lexically_normal().string();

            if (outDef.tileCount <= 0)
            {
                int rows = 0;
                if (outDef.columns > 0 && outDef.tileW > 0 && outDef.tileH > 0)
                {
                    if (outDef.imageW > 0 && outDef.imageH > 0)
                        rows = (outDef.imageH / outDef.tileH);
                }

                if (outDef.columns > 0 && rows > 0)
                    outDef.tileCount = outDef.columns * rows;
            }
        }
        else
        {
            outDef.isImageCollection = true;

            for (XMLElement* tile = tilesetElement->FirstChildElement("tile");
                tile;
                tile = tile->NextSiblingElement("tile"))
            {
                const int tileId = tile->IntAttribute("id", -1);
                if (tileId < 0)
                    continue;

                XMLElement* tileImage = tile->FirstChildElement("image");
                if (!tileImage)
                    continue;

                const char* src = tileImage->Attribute("source");
                if (!src)
                    continue;

                TileImageDef img{};
                img.path = (imageBaseDir / src).lexically_normal().string();
                img.w = tileImage->IntAttribute("width", 0);
                img.h = tileImage->IntAttribute("height", 0);

                outDef.tileImages[tileId] = img;

                outDef.tileW = std::max(outDef.tileW, img.w);
                outDef.tileH = std::max(outDef.tileH, img.h);
            }

            if (outDef.tileImages.empty())
            {
                std::cerr << "Image collection tileset has no <tile><image> entries\n";
                return false;
            }

            if (outDef.tileCount <= 0)
                outDef.tileCount = static_cast<int>(outDef.tileImages.size());
        }

        for (XMLElement* tile = tilesetElement->FirstChildElement("tile");
            tile;
            tile = tile->NextSiblingElement("tile"))
        {
            const int tileId = tile->IntAttribute("id", -1);
            if (tileId < 0)
                continue;

            const auto props = ParseProperties(tile->FirstChildElement("properties"));
            if (!props.empty())
            {
                TilePropertyFlags flags{};
                flags.blocking = PropertyIsTrue(props, "blocking");
                flags.water = PropertyIsTrue(props, "water");
                flags.slow = PropertyIsTrue(props, "slow");
                if (flags.blocking || flags.water || flags.slow)
                    outDef.tileFlags[tileId] = flags;
            }

            XMLElement* animation = tile->FirstChildElement("animation");
            if (!animation)
                continue;

            TileAnimation anim{};

            for (XMLElement* frame = animation->FirstChildElement("frame");
                frame;
                frame = frame->NextSiblingElement("frame"))
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
                outDef.animations[tileId] = anim;
        }

        return true;
    }

    bool LoadTilesetFromTsx(const std::filesystem::path& tsxPath,
        int firstGid,
        int fallbackTileW,
        int fallbackTileH,
        TilesetDef& outDef)
    {
        using namespace tinyxml2;

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

        const std::filesystem::path tsxDir = tsxPath.parent_path();
        return LoadTilesetFromElement(tsxTileset, tsxDir, firstGid, fallbackTileW, fallbackTileH, outDef);
    }
} // namespace

glm::vec2 ObjectPixelsToGrid(const glm::vec2& objectPosPx, int tileWidth, int tileHeight)
{
    const float halfW = tileWidth * 0.5f;
    const float halfH = tileHeight * 0.5f;

    if (halfW <= 0.0f || halfH <= 0.0f)
        return glm::vec2(0.0f);

    const float isoXTopLeft = objectPosPx.x - halfW;
    const float isoYTopLeft = objectPosPx.y - static_cast<float>(tileHeight);

    const float gridX = (isoXTopLeft / halfW + isoYTopLeft / halfH) * 0.5f;
    const float gridY = (isoYTopLeft / halfH - isoXTopLeft / halfW) * 0.5f;

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

    MapData& mapData = outMap.mapData;
    mapData.width = map->IntAttribute("width");
    mapData.height = map->IntAttribute("height");
    mapData.tileW = map->IntAttribute("tilewidth");
    mapData.tileH = map->IntAttribute("tileheight");

    const int expectedCount = mapData.width * mapData.height;

    const std::filesystem::path tmxFsPath(tmxPath);
    const std::filesystem::path tmxDir = tmxFsPath.parent_path();

    // --- Tilesets (.tsx)
    for (XMLElement* tileset = map->FirstChildElement("tileset");
        tileset;
        tileset = tileset->NextSiblingElement("tileset"))
    {
        const int firstGid = tileset->IntAttribute("firstgid", 1);
        const char* tsxSource = tileset->Attribute("source");

        TilesetDef tilesetDef{};
        if (tsxSource)
        {
            const std::filesystem::path tsxPath = (tmxDir / tsxSource).lexically_normal();
            if (!LoadTilesetFromTsx(tsxPath, firstGid, mapData.tileW, mapData.tileH, tilesetDef))
                return false;
        }
        else
        {
            if (!LoadTilesetFromElement(tileset, tmxDir, firstGid, mapData.tileW, mapData.tileH, tilesetDef))
                return false;
        }

        mapData.tilesets.push_back(std::move(tilesetDef));
    }

    if (mapData.tilesets.empty())
    {
        std::cerr << "TMX missing <tileset> element\n";
        return false;
    }

    std::sort(mapData.tilesets.begin(), mapData.tilesets.end(),
        [](const TilesetDef& a, const TilesetDef& b) { return a.firstGid < b.firstGid; });

    // --- Tile layers
    std::vector<uint8_t> collisionTiles(expectedCount, 0);
    bool hasCollisionLayer = false;

    auto ParseLayer = [&](XMLElement* layer)
        {
            const char* name = layer->Attribute("name");
            const std::string layerName = name ? name : "";
            const bool visible = GetBoolAttribute(layer, "visible", true);

            const std::string lowerName = ToLower(layerName);
            const bool isCollision = (lowerName == "collision");

            XMLElement* data = layer->FirstChildElement("data");
            if (!data)
            {
                std::cerr << "Layer '" << layerName << "' missing <data>\n";
                return;
            }

            const char* encoding = data->Attribute("encoding");
            if (!encoding || std::string(encoding) != "csv")
            {
                std::cerr << "Layer '" << layerName << "' is not CSV encoded\n";
                return;
            }

            std::vector<int> rawGids = ParseCsvTiles(data->GetText(), expectedCount);
            if ((int)rawGids.size() != expectedCount)
            {
                std::cerr << "Layer '" << layerName << "' size mismatch. Expected "
                    << expectedCount << " entries but got " << rawGids.size() << "\n";
                return;
            }

            if (isCollision)
            {
                hasCollisionLayer = true;
                for (int i = 0; i < expectedCount; ++i)
                {
                    const uint32_t gid = static_cast<uint32_t>(rawGids[i]) & TMX_GID_MASK;
                    collisionTiles[i] = gid == 0 ? 0 : 1;
                }
                return;
            }

            std::vector<uint32_t> tiles(expectedCount, 0);
            for (int i = 0; i < expectedCount; ++i)
            {
                const uint32_t gid = static_cast<uint32_t>(rawGids[i]) & TMX_GID_MASK;
                tiles[i] = gid;
            }

            if (lowerName == "ground")
                mapData.groundGids = std::move(tiles);
            else if (lowerName == "walls")
                mapData.wallsGids = std::move(tiles);
            else if (lowerName == "overhead")
                mapData.overheadGids = std::move(tiles);

            (void)visible;
        };

    // --- Object groups (with group offsets)
    auto ParseObjectGroup = [&](XMLElement* objectGroup, glm::vec2 groupOffsetPx)
        {
            for (XMLElement* object = objectGroup->FirstChildElement("object");
                object;
                object = object->NextSiblingElement("object"))
            {
                // Apply group offsets to ALL objects
                const float x = GetFloatAttribute(object, "x", 0.0f) + groupOffsetPx.x;
                const float y = GetFloatAttribute(object, "y", 0.0f) + groupOffsetPx.y;
                const float w = GetFloatAttribute(object, "width", 0.0f);
                const float h = GetFloatAttribute(object, "height", 0.0f);

                // Tile object?
                uint32_t rawGid = 0;
                if (const char* gidStr = object->Attribute("gid"))
                    rawGid = static_cast<uint32_t>(std::strtoul(gidStr, nullptr, 10));

                const uint32_t gid = rawGid & TMX_GID_MASK;
                if (gid != 0)
                {
                    MapObjectInstance inst{};
                    inst.tileIndex = gid;
                    inst.worldPos = glm::vec2(x, y);

                    // Prefer real object size if present (trees = 256x256)
                    if (w > 0.0f && h > 0.0f)
                        inst.size = glm::vec2(w, h);
                    else
                        inst.size = glm::vec2((float)mapData.tileW, (float)mapData.tileH);

                    mapData.objectInstances.push_back(std::move(inst));
                    continue;
                }

                // Non-tile object (Door / Spawn / generic)
                const char* name = object->Attribute("name");
                const char* type = object->Attribute("type");
                const std::string typeName = type ? type : "";
                XMLElement* props = object->FirstChildElement("properties");

                if (!typeName.empty() && typeName == "Door")
                {
                    DoorDef door{};
                    door.posPx = { x, y };
                    door.sizePx = { w, h };
                    door.targetMap = GetStringProp(props, "targetMap");
                    door.targetSpawn = GetStringProp(props, "targetSpawn");
                    mapData.doors.push_back(std::move(door));
                    continue;
                }

                if (!typeName.empty() && typeName == "Spawn")
                {
                    SpawnDef spawn{};
                    spawn.name = name ? name : "";
                    spawn.posPx = { x, y };
                    mapData.spawns.push_back(std::move(spawn));
                    continue;
                }

                MapObject mapObject{};
                mapObject.id = object->IntAttribute("id", 0);
                mapObject.name = name ? name : "";
                mapObject.type = typeName;
                mapObject.positionPx = { x, y };
                mapObject.sizePx = { w, h };
                mapObject.properties = ParseProperties(props);
                mapData.objects.push_back(std::move(mapObject));
            }
        };

    // --- Walk tree with nested group offsets
    std::function<void(XMLElement*, glm::vec2)> ParseNode;

    ParseNode = [&](XMLElement* node, glm::vec2 parentOffsetPx)
        {
            if (!node)
                return;

            const std::string tag = node->Name();

            // Groups can carry offsets: <group offsetx="" offsety="">
            const float offX = GetFloatAttribute(node, "offsetx", 0.0f);
            const float offY = GetFloatAttribute(node, "offsety", 0.0f);
            const glm::vec2 nodeOffsetPx = parentOffsetPx + glm::vec2(offX, offY);

            if (tag == "layer")
            {
                ParseLayer(node);
            }
            else if (tag == "objectgroup")
            {
                ParseObjectGroup(node, nodeOffsetPx);
            }
            else if (tag == "group")
            {
                for (XMLElement* child = node->FirstChildElement();
                    child;
                    child = child->NextSiblingElement())
                {
                    ParseNode(child, nodeOffsetPx);
                }
            }
        };

    for (XMLElement* node = map->FirstChildElement(); node; node = node->NextSiblingElement())
        ParseNode(node, glm::vec2(0.0f));

    if (hasCollisionLayer)
        mapData.collision = std::move(collisionTiles);
    else
        mapData.collision.assign(expectedCount, 0);

    mapData.tileFlags.assign(expectedCount, TilePropertyFlags{});

    auto AccumulateTileFlags = [&](int index, uint32_t gidValue)
        {
            if (gidValue == 0)
                return;

            const TilesetDef* def = FindTilesetForGid(mapData.tilesets, gidValue);
            if (!def)
                return;

            const int localId = static_cast<int>(gidValue) - def->firstGid;
            auto flagIt = def->tileFlags.find(localId);
            if (flagIt == def->tileFlags.end())
                return;

            TilePropertyFlags& flags = mapData.tileFlags[index];
            flags.blocking = flags.blocking || flagIt->second.blocking;
            flags.water = flags.water || flagIt->second.water;
            flags.slow = flags.slow || flagIt->second.slow;
        };

    for (int i = 0; i < expectedCount; ++i)
    {
        if ((int)mapData.groundGids.size() == expectedCount)
            AccumulateTileFlags(i, mapData.groundGids[i]);
        if ((int)mapData.wallsGids.size() == expectedCount)
            AccumulateTileFlags(i, mapData.wallsGids[i]);
        if ((int)mapData.overheadGids.size() == expectedCount)
            AccumulateTileFlags(i, mapData.overheadGids[i]);

        if (mapData.tileFlags[i].blocking)
            mapData.collision[i] = 1;
    }

    std::cout << "TMX loaded: " << tmxPath << "\n";
    std::cout << "  map size: " << mapData.width << " x " << mapData.height << " tiles\n";
    std::cout << "  tile size: " << mapData.tileW << " x " << mapData.tileH << " px\n";
    std::cout << "  tilesets: " << mapData.tilesets.size() << "\n";
    std::cout << "  layers: ground=" << (mapData.HasGround() ? "yes" : "no")
        << " walls=" << (mapData.HasWalls() ? "yes" : "no")
        << " overhead=" << (mapData.HasOverhead() ? "yes" : "no")
        << " collision=" << (hasCollisionLayer ? "yes" : "no") << "\n";
    std::cout << "  objects: " << mapData.objects.size() << "\n";
    std::cout << "  tile objects: " << mapData.objectInstances.size() << "\n";

    return true;
}
