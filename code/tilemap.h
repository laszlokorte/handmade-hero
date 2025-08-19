#ifndef TILEMAP_H

#include "handmade_types.h"

struct tile_position {
  int X;
  int Y;

  real32 RelX;
  real32 RelY;
};
enum tile_kind {
  TILE_EMPTY,
  TILE_WALL,
  TILE_DOOR,
};

struct tile {
  tile_kind Kind;
};

struct tile_chunk {
  tile *tiles;
};

struct chunk_tile_position {
  int ChunkX;
  int ChunkY;

  int TileX;
  int TileY;
};

struct chunk_position {
  int ChunkX;
  int ChunkY;
};

struct tile_hash_entry {
  bool Removed;
  chunk_position Position;

  tile_chunk *Chunk;
};

struct tile_map {
  int32 TileWidth;
  int32 TileHeight;

  uint32 ChunkWidth;
  uint32 ChunkHeight;

  memory_index MaxChunks;
  memory_index UsedChunks;
  tile_hash_entry *Chunks;
};

internal tile_kind GetTile(tile_map *Map, int TileX, int TileY);

internal void SetTileKind(memory_arena *Arena, tile_map *Map, int TileX,
                          int TileY, uint16 Kind);

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
