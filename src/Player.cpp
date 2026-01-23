#include "Player.h"
#include "SpriteRenderer.h"
#include "Camera2D.h"

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
    glm::vec2 feetWorld = tileTopLeftWorldPos + glm::vec2(tileW * 0.5f, (float)tileH);
    glm::vec2 playerTopLeft = feetWorld - glm::vec2(mSizePx.x * 0.5f, mSizePx.y);

    renderer.Draw(mTexture, playerTopLeft, mSizePx, camera);
}

