#pragma once

#include <glm/glm.hpp>

/*
    Camera2D
    --------
    Minimal 2D camera storing the top-left corner of the view in world pixels.

    screenPos = worldPos - cameraPos
*/
class Camera2D
{
public:
    Camera2D() = default;
    explicit Camera2D(const glm::vec2& position) : mPosition(position) {}

    // Move camera by a delta amount (pixels)
    void Move(const glm::vec2& delta) { mPosition += delta; }

    // Set camera position directly
    void SetPosition(const glm::vec2& position) { mPosition = position; }

    // Read camera position
    const glm::vec2& GetPosition() const { return mPosition; }

private:
    glm::vec2 mPosition{ 0.0f, 0.0f };
};
