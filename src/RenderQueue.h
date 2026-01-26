#pragma once
#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <glad/glad.h>

/*
    RenderCmd
    ---------
    One draw call worth of data for SpriteRenderer.
    depthKey determines draw order (lower first, higher last).
*/
struct RenderCmd
{
    GLuint texture = 0;
    glm::vec2 posPx{ 0.0f, 0.0f };   // TOP-LEFT in world pixels
    glm::vec2 sizePx{ 0.0f, 0.0f };
    glm::vec2 uvMin{ 0.0f, 0.0f };
    glm::vec2 uvMax{ 1.0f, 1.0f };
    float depthKey = 0.0f;         // feet-based / iso-diagonal-based
};

/*
    RenderQueue
    -----------
    Collect render commands, then sort + draw once.
*/
class RenderQueue
{
public:
    void Clear() { mCmds.clear(); }
    void Reserve(size_t n) { mCmds.reserve(n); }

    void Push(const RenderCmd& cmd) { mCmds.push_back(cmd); }

    // Stable sort avoids “shimmer” when depthKeys are equal.
    void SortByDepthStable()
    {
        std::stable_sort(mCmds.begin(), mCmds.end(),
            [](const RenderCmd& a, const RenderCmd& b)
            {
                return a.depthKey < b.depthKey;
            });
    }

    const std::vector<RenderCmd>& Items() const { return mCmds; }

private:
    std::vector<RenderCmd> mCmds;
};
