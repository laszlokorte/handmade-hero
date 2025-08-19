#ifndef TILEMAP_H

#include "handmade_types.h"

struct tile_position {
  int X;
  int Y;

  real32 RelX;
  real32 RelY;
};

struct tile_map {
  int32 TileWidth;
  int32 TileHeight;
};

internal void TilePositionNormalize(tile_position *Pos) {
  while (Pos->RelX < -0.5) {
    Pos->RelX += 1;
    Pos->X -= 1;
  }
  while (Pos->RelY < -0.5) {
    Pos->RelY += 1;
    Pos->Y -= 1;
  }
  while (Pos->RelX > 0.5) {
    Pos->RelX -= 1;
    Pos->X += 1;
  }
  while (Pos->RelY > 0.5) {
    Pos->RelY -= 1;
    Pos->Y += 1;
  }
}

#define TILEMAP_H
#endif
