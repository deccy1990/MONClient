#include "Camera2D.h"

Camera2D::Camera2D(const glm::vec2& position)
    : mPosition(position)
{
}

void Camera2D::Move(const glm::vec2& delta)
{
    mPosition += delta;
}

void Camera2D::SetPosition(const glm::vec2& position)
{
    mPosition = position;
}

const glm::vec2& Camera2D::GetPosition() const
{
    return mPosition;
}
