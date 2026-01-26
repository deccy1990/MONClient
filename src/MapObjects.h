#pragma once

#include <glm/glm.hpp>

#include <string>

struct DoorDef
{
    glm::vec2 posPx{ 0.0f, 0.0f };
    glm::vec2 sizePx{ 0.0f, 0.0f };
    std::string targetMap;
    std::string targetSpawn;
};

struct SpawnDef
{
    std::string name;
    glm::vec2 posPx{ 0.0f, 0.0f };
};
