#include "PlayerController.h"
#include "Player.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

static constexpr glm::vec2 playerHalfExtents(0.05f, 0.05f);

PlayerController::PlayerController(Player& player)
    : mPlayer(player)
{
}

bool PlayerController::IsBlockedAt(
    int tx, int ty,
    int mapW, int mapH,
    const std::vector<int>& collisionGrid
) const
{
    if (tx < 0 || tx >= mapW || ty < 0 || ty >= mapH)
        return true;

    return collisionGrid[ty * mapW + tx] != 0;
}

bool PlayerController::CollidesAt(
    const glm::vec2& pos,
    int mapW, int mapH,
    const std::vector<int>& collisionGrid
) const
{
    glm::vec2 corners[4] =
    {
        { pos.x - playerHalfExtents.x, pos.y - playerHalfExtents.y },
        { pos.x + playerHalfExtents.x, pos.y - playerHalfExtents.y },
        { pos.x - playerHalfExtents.x, pos.y + playerHalfExtents.y },
        { pos.x + playerHalfExtents.x, pos.y + playerHalfExtents.y }
    };

    for (const glm::vec2& c : corners)
    {
        int tx = (int)std::floor(c.x);
        int ty = (int)std::floor(c.y);

        if (IsBlockedAt(tx, ty, mapW, mapH, collisionGrid))
            return true;
    }

    return false;
}

void PlayerController::Update(
    GLFWwindow* window,
    float deltaTime,
    int mapW,
    int mapH,
    const std::vector<int>& collisionGrid
)
{
    // ------------------------------------
    // Input → intent (screen space)
    // ------------------------------------
    glm::vec2 screenDir(0.0f);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) screenDir.y -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) screenDir.y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) screenDir.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) screenDir.x += 1.0f;

    glm::vec2 intentDir = screenDir;

    // ------------------------------------
    // Run toggle (Ctrl)
    // ------------------------------------
    bool ctrlDown =
        (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
        (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

    if (ctrlDown && !wasCtrlDown)
    {
        runEnabled = !runEnabled;

        if (mPlayer.isMoving)
        {
            mPlayer.animTimer = 0.0f;
            mPlayer.animFrame = 0;
            mPlayer.runKickTimer = 0.10f;
        }
    }

    wasCtrlDown = ctrlDown;

    if (screenDir.x != 0.0f || screenDir.y != 0.0f)
        screenDir = glm::normalize(screenDir);

    bool newMoving = (intentDir.x != 0.0f || intentDir.y != 0.0f);

    if (newMoving && !mPlayer.wasMoving)
    {
        // just started moving
        mPlayer.animTimer = 0.0f;
        mPlayer.animFrame = 0;
    }

    if (!newMoving && mPlayer.wasMoving)
    {
        // just stopped moving
        mPlayer.animTimer = 0.0f;
        mPlayer.animFrame = 0; // idle frame
    }

    mPlayer.isMoving = newMoving;
    mPlayer.wasMoving = newMoving;
    mPlayer.isRunning = runEnabled && mPlayer.isMoving;

    // Visual lean (pixels). Tune these.
    const float walkLean = 1.5f;
    const float runLean = 2.5f;

    glm::vec2 nd = intentDir;
    if (nd.x != 0.0f || nd.y != 0.0f)
        nd = glm::normalize(nd);

    float lean = mPlayer.isRunning ? runLean : walkLean;
    mPlayer.visualOffsetPx = mPlayer.isMoving ? (nd * lean) : glm::vec2(0.0f);

    // Update facing based on screen intent
    if (newMoving)
    {
        // Pick dominant axis (prevents diagonal flicker)
        float ax = std::abs(intentDir.x);
        float ay = std::abs(intentDir.y);

        if (ax > ay)
            mPlayer.facing = (intentDir.x > 0.0f) ? Player::FacingDir::Right : Player::FacingDir::Left;
        else if (ay > ax)
            mPlayer.facing = (intentDir.y > 0.0f) ? Player::FacingDir::Down : Player::FacingDir::Up;
        // else equal: keep current facing
    }

    // Convert screen direction -> grid direction using iso basis vectors:
    // screenRight = (+1, -1) in grid
    // screenDown  = (+1, +1) in grid
    glm::vec2 gridDir = screenDir.x * glm::vec2(1.0f, -1.0f) +
        screenDir.y * glm::vec2(1.0f, 1.0f);

    // Normalize so diagonal isnt faster
    if (gridDir.x != 0.0f || gridDir.y != 0.0f)
        gridDir = glm::normalize(gridDir);
    mPlayer.moveVec = gridDir;

    // ------------------------------------
    // Animate (4 frames per direction)
    // ------------------------------------
    const float walkFps = 9.0f;
    const float runFps = 13.0f;

    if (mPlayer.isMoving)
    {
        const float animFps = mPlayer.isRunning ? runFps : walkFps;
        const float frameTime = 1.0f / animFps;

        mPlayer.animTimer += deltaTime;
        while (mPlayer.animTimer >= frameTime)
        {
            mPlayer.animTimer -= frameTime;
            mPlayer.animFrame = (mPlayer.animFrame + 1) % 4; // 0..3
        }
    }
    else
    {
        // Idle: reset to first frame
        mPlayer.animFrame = 0;
        mPlayer.animTimer = 0.0f;
    }

    // Bobbing: use animFrame as a simple step wave.
    // Frames 0..3 -> [-1, 0, +1, 0] style bob.
    float bobAmp = mPlayer.isRunning ? 1.6f : 1.0f; // pixels

    float bob = 0.0f;
    switch (mPlayer.animFrame)
    {
    case 0: bob = -0.5f; break;
    case 1: bob = 0.0f; break;
    case 2: bob = 0.5f; break;
    case 3: bob = 0.0f; break;
    }
    if (!mPlayer.isMoving) bob = 0.0f;

    mPlayer.visualOffsetPx.y += bob * bobAmp;

    mPlayer.runKickTimer = std::max(0.0f, mPlayer.runKickTimer - deltaTime);
    if (mPlayer.runKickTimer > 0.0f && mPlayer.isMoving)
    {
        float t = mPlayer.runKickTimer / 0.10f; // 1 -> 0
        // extra lean forward, fades out
        mPlayer.visualOffsetPx += nd * (t * 1.5f);
    }

    // Frame index in row-major order
    // frameIndex = row * cols + col
    int frameIndex = static_cast<int>(mPlayer.facing) * 4 + mPlayer.animFrame;
    mPlayer.SetFrame(frameIndex);

    // ------------------------------------
    // Movement speed in tiles per second.
    // ------------------------------------
    float walkTilesPerSec = 3.0f;
    float runTilesPerSec = 5.0f;
    float moveSpeed = runEnabled ? runTilesPerSec : walkTilesPerSec;

    // ------------------------------------
    // Integrate movement with hitbox collision (slide along walls)
    // ------------------------------------
    glm::vec2 desiredMove = mPlayer.moveVec * moveSpeed * deltaTime;
    glm::vec2 pos = mPlayer.GetGridPos();

    // --- X axis ---
    if (desiredMove.x != 0.0f)
    {
        glm::vec2 testPos = pos;
        testPos.x += desiredMove.x;

        if (!CollidesAt(testPos, mapW, mapH, collisionGrid))
            pos.x = testPos.x;
    }

    // --- Y axis ---
    if (desiredMove.y != 0.0f)
    {
        glm::vec2 testPos = pos;
        testPos.y += desiredMove.y;

        if (!CollidesAt(testPos, mapW, mapH, collisionGrid))
            pos.y = testPos.y;
    }

    // Clamp within map bounds
    pos.x = std::clamp(pos.x, 0.0f, (float)(mapW - 1));
    pos.y = std::clamp(pos.y, 0.0f, (float)(mapH - 1));

    auto SnapAxis = [](float& v)
    {
        float center = std::round(v - 0.5f) + 0.5f;
        float dist = v - center;

        if (std::abs(dist) < 0.02f)
            v = center;
    };

    if (mPlayer.isMoving)
    {
        // Snap only on the axis NOT being moved
        if (std::abs(mPlayer.moveVec.x) > std::abs(mPlayer.moveVec.y))
            SnapAxis(pos.y);
        else
            SnapAxis(pos.x);
    }

    mPlayer.SetGridPos(pos);

    float targetOffset = 0.0f;

    if (mPlayer.isMoving && mPlayer.moveVec.y > 0.2f)
        targetOffset = 1.0f;

    if (mPlayer.isMoving && mPlayer.moveVec.y < -0.2f)
        targetOffset = -1.0f;

    mPlayer.verticalVisualOffset += (targetOffset - mPlayer.verticalVisualOffset) * 12.0f * deltaTime;
}
