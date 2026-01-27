#pragma once

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <vector>

class Player;
class Camera2D;

class PlayerController
{
public:
    PlayerController(Player& player);

    void Update(
        GLFWwindow* window,
        float deltaTime,
        int mapW,
        int mapH,
        const std::vector<int>& collisionGrid
    );

private:
    Player& mPlayer;

    // --- persistent input state ---
    bool runEnabled = false;
    bool wasCtrlDown = false;

    // --- helpers ---
    bool IsBlockedAt(int tx, int ty, int mapW, int mapH, const std::vector<int>& collisionGrid) const;
    bool CollidesAt(const glm::vec2& pos, int mapW, int mapH, const std::vector<int>& collisionGrid) const;
};
