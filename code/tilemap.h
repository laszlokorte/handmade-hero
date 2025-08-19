#ifndef TILEMAP_H

#include "handmade_types.h"

struct tile_position {
    int X;
    int Y;

    real32 RelX;
    real32 RelY;
};

struct tile_map {
    uint32 TileWidth;
    uint32 TileHeight;
};

#define TILEMAP_H
#endif
