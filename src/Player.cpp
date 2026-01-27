#include "Player.h"
#include "SpriteRenderer.h"
#include "Camera2d.h"
#include "TileMath.h"

/*
    We anchor the player's feet at the bottom-center of the tile:
      tileTopLeftWorldPos is the position you use to draw the tile (64x32 quad).

    feetWorld = tileTopLeft + (tileW/2, tileH)
    playerTopLeft = feetWorld - (playerW/2, playerH)
*/
void Player::DrawOnTile(SpriteRenderer& renderer,
    const Camera2D& camera,
    const glm::vec2& tileTopLeftWorldPos,
    int tileW,
    int tileH) const
{
    // Feet position: center-bottom of tile
    glm::vec2 feetWorld =
        tileTopLeftWorldPos + glm::vec2(tileW * 0.5f, (float)tileH);

    // Convert feet position to sprite top-left
    glm::vec2 playerTopLeft;
    playerTopLeft = feetWorld - spritePivotPx + feetPixelOffset + visualOffsetPx;

    glm::vec2 uvMin;
    glm::vec2 uvMax;
    mSheet.GetUV(mFrame, uvMin, uvMax);
    renderer.Draw(mTexture, playerTopLeft, mSizePx, camera, uvMin, uvMax);
}

void Player::AppendToQueue(RenderQueue& queue,
    const glm::vec2& tileTopLeftWorldPos,
    int tileW,
    int tileH) const
{
    glm::vec2 feetWorld = tileTopLeftWorldPos + glm::vec2(tileW * 0.5f, (float)tileH);

    glm::vec2 playerTopLeft;
    playerTopLeft = feetWorld - spritePivotPx + feetPixelOffset + visualOffsetPx;

    RenderCmd cmd{};
    cmd.texture = mTexture;
    cmd.posPx = playerTopLeft;
    cmd.sizePx = mSizePx;
    mSheet.GetUV(mFrame, cmd.uvMin, cmd.uvMax);
    cmd.depthKey = DepthFromFeetWorldY(feetWorld.y);

    queue.Push(cmd);
}
