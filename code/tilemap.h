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

struct tile_distance {
  int DX;
  int DY;

  real32 DRelX;
  real32 DRelY;
};

internal tile_distance TileDistance(tile_position a, tile_position b) {
    tile_distance Result;
    Result.DX = a.X - b.X;
    Result.DY = a.Y - b.Y;
    Result.DRelX = a.RelX - b.RelX;
    Result.DRelY = a.RelY - b.RelY;

    return Result;
}

#define TILEMAP_H
#endif
