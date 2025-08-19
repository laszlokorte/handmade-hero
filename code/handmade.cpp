#include "./handmade.h"
#include "handmade_types.h"
#include "math.h"
#include "entropy.h"
#include "tilemap.h"

global_variable game_state global_game_state = {};
struct bit_scan_result {
  bool Found;
  uint32 Index;
};
internal bit_scan_result FindLowestBit(uint32 Value) {
  bit_scan_result Result = {};
  for (uint32 Test = 0; Test < 32; ++Test) {
    if (Value & (1 << Test)) {
      Result.Index = Test;
      Result.Found = true;
      break;
    }
  }

  return Result;
}

#pragma pack(push, 1)
struct bitmap_header {
  uint16 FileType;
  uint32 FileSize;
  uint16 Reserved1;
  uint16 Reserved2;
  uint32 BitmapOffset;
  uint32 Size;
  int32 Width;
  int32 Height;
  uint16 Planes;
  uint16 BitsPerPixel;
  uint32 Compression;
  uint32 SizeOfBitmap;
  int32 HorzResolution;
  int32 VertResolution;
  uint32 ColorsUsed;
  uint32 ColorsImportant;
  uint32 RedMask;
  uint32 GreenMask;
  uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context *Context,
             debug_platform_read_entire_file *ReadEntireFile, char *FileName) {
  debug_read_file_result ReadResult = ReadEntireFile(Context, FileName);
  loaded_bitmap Result = {};

  if (ReadResult.ContentSize != 0) {
    bitmap_header *Header = (bitmap_header *)ReadResult.Contents;

    if (Header->Compression == 3) {

      uint32 RedMask = Header->RedMask;
      uint32 GreenMask = Header->GreenMask;
      uint32 BlueMask = Header->BlueMask;
      uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

      bit_scan_result RedShift = FindLowestBit(RedMask);
      bit_scan_result GreenShift = FindLowestBit(GreenMask);
      bit_scan_result BlueShift = FindLowestBit(BlueMask);
      bit_scan_result AlphaShift = FindLowestBit(AlphaMask);

      uint32 *Pixels =
          (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);

      uint32 *SourceDest = Pixels;
      for (int32 Y = 0; Y < Header->Height; Y++) {
        for (int32 X = 0; X < Header->Width; X++) {
          uint32 Red = (*SourceDest & RedMask) >> RedShift.Index;
          uint32 Green = (*SourceDest & GreenMask) >> GreenShift.Index;
          uint32 Blue = (*SourceDest & BlueMask) >> BlueShift.Index;
          uint32 Alpha = (*SourceDest & AlphaMask) >> AlphaShift.Index;
          *SourceDest = Alpha << 24 | Red << 16 | Green << 8 | Blue;
          SourceDest++;
        }
      }

      Result.Memory = Pixels;
      Result.Width = Header->Width;
      Result.Height = Header->Height;
    }
  }

  return Result;
}

internal void GameOutputSound(bool muted, int32 Note, real32 Volume,
                              game_sound_output_buffer *SoundBuffer) {

  local_persist real32 tSin = 0;
  real32 ToneVolumne = muted ? 0 : (2.0f * powf(2.0f, 8 + 4 * Volume));
  real32 toneHz = 440;
  real32 f = toneHz / (real32)SoundBuffer->SamplesPerSecond *
             powf(2.0f, NOTE_HALFTONE * (real32)Note);
  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
       ++SampleIndex) {
    int16 SampleValue = (int16)(sinf(2.0f * Pi32 * tSin) * ToneVolumne);
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

    tSin = fmodf(tSin + f, 1.0);
  }
}

internal int RoundRealToInt(real32 Real) { return (int)(Real + 0.5); }

internal int Clamp(int value, int min, int max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

internal void FillRect(game_offscreen_buffer *Buffer, real32 XMinReal,
                       real32 YMinReal, real32 XMaxReal, real32 YMaxReal,
                       game_color_rgb Color) {
  int XMin = Clamp(RoundRealToInt(XMinReal), 0, Buffer->Width);
  int XMax = Clamp(RoundRealToInt(XMaxReal), 0, Buffer->Width);
  int YMin = Clamp(RoundRealToInt(YMinReal), 0, Buffer->Height);
  int YMax = Clamp(RoundRealToInt(YMaxReal), 0, Buffer->Height);

  int Red = RoundRealToInt(255.0f * Color.r);
  int Green = RoundRealToInt(255.0f * Color.g);
  int Blue = RoundRealToInt(255.0f * Color.b);

  int RGB = Red << 16 | Green << 8 | Blue;

  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row =
      (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * XMin + Stride * YMin;
  for (int y = YMin; y < YMax; y++) {
    uint8 *Pixel = Row;
    for (int x = XMin; x < XMax; x++) {
      *(int *)Pixel = RGB;
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal int32 SampleBitmap(loaded_bitmap *Bitmap, int x, int y) {
  return Bitmap
      ->Memory[(x % Bitmap->Width) +
               (Bitmap->Height - 1 - ((y % Bitmap->Height))) * Bitmap->Width];
}

internal uint32 lerpColor(real32 t, uint32 c1, uint32 c2) {
  uint32 a1 = (c1 >> 24) & 0xff;
  uint32 r1 = (c1 >> 16) & 0xff;
  uint32 g1 = (c1 >> 8) & 0xff;
  uint32 b1 = (c1 >> 0) & 0xff;
  uint32 a2 = (c2 >> 24) & 0xff;
  uint32 r2 = (c2 >> 16) & 0xff;
  uint32 g2 = (c2 >> 8) & 0xff;
  uint32 b2 = (c2 >> 0) & 0xff;

  real32 rm = r1 * (1.0f - t) + r2 * t;
  real32 gm = g1 * (1.0f - t) + g2 * t;
  real32 bm = b1 * (1.0f - t) + b2 * t;
  real32 am = a1 * (1.0f - t) + a2 * t;
  return ((int)am << 24) | ((int)rm << 16) | ((int)gm << 8) | ((int)bm << 0);
}

internal uint32 AlphaBlendARGB(uint32 Bg, uint32 Fg) {
  real32 Alpha = (((Fg >> 24) & 0xff) / 255.0f);
  return lerpColor(Alpha, Bg, Fg);
}

internal uint32 SampleBitmapBilinear(loaded_bitmap *Bitmap, real32 x,
                                     real32 y) {
  real32 xr = x * Bitmap->Width;
  real32 yr = y * Bitmap->Height;
  int x0 = (int)xr;
  int y0 = (int)yr;
  int x1 = (int)xr + 1;
  int y1 = (int)yr + 1;
  real32 xt = xr - x0;
  real32 yt = yr - y0;

  int c00 = SampleBitmap(Bitmap, x0, y0);
  int c01 = SampleBitmap(Bitmap, x0, y1);
  int c10 = SampleBitmap(Bitmap, x1, y0);
  int c11 = SampleBitmap(Bitmap, x1, y1);

  return lerpColor(xt, lerpColor(yt, c00, c01), lerpColor(yt, c10, c11));
}

internal void FillRectTexture(game_offscreen_buffer *Buffer, real32 XMinReal,
                              real32 YMinReal, real32 XMaxReal, real32 YMaxReal,
                              loaded_bitmap *Bitmap) {
  real32 RealWidth = YMaxReal - YMinReal;
  real32 RealHeight = YMaxReal - YMinReal;

  int XMinRealRound = RoundRealToInt(XMinReal);
  int XMaxRealRound = RoundRealToInt(XMaxReal);
  int YMinRealRound = RoundRealToInt(YMinReal);
  int YMaxRealRound = RoundRealToInt(YMaxReal);
  int XMin = Clamp(XMinRealRound, 0, Buffer->Width);
  int XMax = Clamp(XMaxRealRound, 0, Buffer->Width);
  int YMin = Clamp(YMinRealRound, 0, Buffer->Height);
  int YMax = Clamp(YMaxRealRound, 0, Buffer->Height);

  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row =
      (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * XMin + Stride * YMin;
  for (int y = YMin; y < YMax; y++) {
    uint8 *Pixel = Row;
    for (int x = XMin; x < XMax; x++) {
      real32 u = (x - XMinRealRound) / (RealWidth);
      real32 v = (y - YMinRealRound) / (RealHeight);
      int ARGB = SampleBitmapBilinear(Bitmap, u, v);
      *(int *)Pixel = AlphaBlendARGB(*(int *)Pixel, ARGB);
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal void RenderRect(game_offscreen_buffer *Buffer, int X, int Y, int Width,
                         int Height, int Color) {
  if (X < 0) {
    Width += X;
    X = 0;
  }
  if (Y < 0) {
    Height += Y;
    Y = 0;
  }
  if (X >= Buffer->Width || Y >= Buffer->Height) {
    return;
  }
  if (X + Width >= Buffer->Width) {
    Width = Buffer->Width - X;
  }
  if (Y + Height >= Buffer->Height) {
    Height = Buffer->Height - Y;
  }
  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row = (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * X + Stride * Y;
  for (int y = 0; y < Height; y++) {
    uint8 *Pixel = Row;
    for (int x = 0; x < Width; x++) {
      *(int *)Pixel = Color;
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal void ClearScreen(game_offscreen_buffer *Buffer, int ClearColor) {
  unsigned int *canvas = (unsigned int *)(Buffer->Memory);
  int cx = Buffer->Width / 2;
  int cy = Buffer->Height / 2;
  for (int x = 0; x < Buffer->Width; x++) {
    for (int y = 0; y < Buffer->Height; y++) {
      canvas[(y)*Buffer->Width + (x)] = ClearColor;
    }
  }
}

internal void RenderGradient(game_offscreen_buffer *Buffer, int xoff, int yoff,
                             int zoff) {
  unsigned int *canvas = (unsigned int *)(Buffer->Memory);
  int cx = Buffer->Width / 2;
  int cy = Buffer->Height / 2;
  for (int x = 0; x < Buffer->Width; x++) {
    for (int y = 0; y < Buffer->Height; y++) {
      int yy = y - cy;
      int xx = x - cx;

      uint8 green = (uint8)(xx + xoff);
      uint8 blue = (uint8)(yy + yoff);
      uint8 red = (uint8)(zoff / 2);
      if ((zoff / 256) % 2 == 0) {
        red = 255 - red;
      }
      canvas[(y)*Buffer->Width + (x)] = (green << 8) | blue | (red << 16);
    }
  }
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples) {
  Assert(Memory->PermanentStorageSize > sizeof(game_state));

  game_state *GameState = (game_state *)Memory->PermanentStorage;

  if (Memory->Initialized) {
    GameOutputSound(GameState->Muted, GameState->Note,
                    ((real32)GameState->Volume) / GameState->VolumeRange,
                    SoundBuffer);
  }
}

internal tile_chunk *GetOrCreateTileChunk(memory_arena *Arena, tile_map *Map,
                                          int ChunkX, int ChunkY) {
  for (int i = 0; i < Map->MaxChunks; i++) {
    uint32 Hash = (ChunkX * 7 + ChunkY * 13 + i * 11) % Map->MaxChunks;

    tile_hash_entry *Entry = &Map->Chunks[Hash];
    if (Entry->Removed || Entry->Chunk == 0) {
      Entry->Chunk = ArenaPushStruct(Arena, tile_chunk);
      if (!Entry->Chunk) {
        break;
      }
      Entry->Chunk->tiles =
          ArenaPushArray(Arena, tile, Map->ChunkHeight * Map->ChunkWidth);
      if (!Entry->Chunk->tiles) {
        break;
      }
      Entry->Position.ChunkX = ChunkX;
      Entry->Position.ChunkY = ChunkY;
      Entry->Removed = false;
      return Entry->Chunk;

    } else {
      if (Entry->Position.ChunkX == ChunkX &&
          Entry->Position.ChunkY == ChunkY) {
        return Entry->Chunk;
      } else {
        continue;
      }
    }
  }
  return 0;
}

internal tile_chunk *GetTileChunk(tile_map *Map, int ChunkX, int ChunkY) {
  for (int i = 0; i < Map->MaxChunks; i++) {
    uint32 Hash = (ChunkX * 7 + ChunkY * 13 + i * 11) % Map->MaxChunks;

    tile_hash_entry Entry = Map->Chunks[Hash];
    if (Entry.Removed) {
      continue;
    } else if (Entry.Chunk != 0) {
      if (Entry.Position.ChunkX == ChunkX && Entry.Position.ChunkY == ChunkY) {
        return Entry.Chunk;
      } else {
        continue;
      }
    } else {
      break;
    }
  }
  return 0;
}

internal chunk_tile_position GetChunkPosition(tile_map *Map, int TileX,
                                              int TileY) {
  chunk_tile_position Result = {};
  Result.ChunkX = TileX / Map->ChunkWidth;
  Result.ChunkY = TileY / Map->ChunkHeight;
  Result.TileX = TileX % Map->ChunkWidth;
  Result.TileY = TileY % Map->ChunkHeight;

  return Result;
}

internal tile_kind GetTileKind(tile_map *Map, int TileX, int TileY) {
  chunk_tile_position ChunkTilePos = GetChunkPosition(Map, TileX, TileY);
  tile_chunk *Chunk =
      GetTileChunk(Map, ChunkTilePos.ChunkX, ChunkTilePos.ChunkY);

  if (Chunk != 0) {
    tile Tile =
        Chunk->tiles[ChunkTilePos.TileX + Map->ChunkWidth * ChunkTilePos.TileY];

    return Tile.Kind;
  } else {
    return TILE_EMPTY;
  }
}

internal void SetTileKind(memory_arena *Arena, tile_map *Map, int TileX,
                          int TileY, tile_kind Kind) {

  chunk_tile_position ChunkTilePos = GetChunkPosition(Map, TileX, TileY);
  tile_chunk *Chunk = GetOrCreateTileChunk(Arena, Map, ChunkTilePos.ChunkX,
                                           ChunkTilePos.ChunkY);

  if (Chunk) {
    tile *Tile =
        &Chunk
             ->tiles[ChunkTilePos.TileX + Map->ChunkWidth * ChunkTilePos.TileY];
    Tile->Kind = Kind;
  }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender) {
  Assert(Memory->PermanentStorageSize > sizeof(game_state));

  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->Initialized) {
    game_state NewGameState = {};
    *GameState = NewGameState;
    GameState->Time = 0;
    GameState->Note = 0;
    GameState->VolumeRange = 5;
    GameState->Volume = 0;
    GameState->Muted = true;
    GameState->JumpTime = 0;
    GameState->Camera.pos.X = 0;
    GameState->Camera.pos.Y = 0;
    GameState->Camera.pos.RelX = 0;
    GameState->Camera.pos.RelY = 0;
    GameState->TileMap.TileWidth = 128;
    GameState->TileMap.TileHeight = 128;
    GameState->TileMap.ChunkHeight = 64;
    GameState->TileMap.ChunkWidth = 64;
    GameState->TileMap.MaxChunks = 256;
    char FileName[] = "./data/logo.bmp";
    GameState->Logo =
        DEBUGLoadBMP(Context, Memory->DebugPlatformReadEntireFile, FileName);
    InitializeArena(&GameState->WorldArena,
                    Memory->PermanentStorageSize - sizeof(*GameState),
                    Memory->PermanentStorage + sizeof(*GameState));
    GameState->TileMap.Chunks = ArenaPushArray(
        &GameState->WorldArena, tile_hash_entry, GameState->TileMap.MaxChunks);

    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 0, TILE_WALL);
    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 2, TILE_WALL);
    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 1, TILE_WALL);
    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 3, TILE_WALL);

    Memory->Initialized = true;
  }

  if (GameState->JumpTime > 0) {
    GameState->JumpTime--;
  }

  for (size_t c = 0; c < ArrayCount(Input->Controllers); c++) {
    game_controller_input *Controller = &Input->Controllers[c];
    if (Controller->LeftShoulder.HalfTransitionCount > 0 &&
        Controller->LeftShoulder.EndedDown) {
      GameState->Note--;
    }
    if (Controller->RightShoulder.HalfTransitionCount > 0 &&
        Controller->RightShoulder.EndedDown) {
      GameState->Note++;
    }
    if (Controller->ActionUp.HalfTransitionCount > 0 &&
        Controller->ActionUp.EndedDown) {
      GameState->Volume++;
      GameState->Muted = false;
      if (GameState->Volume > GameState->VolumeRange) {
        GameState->Volume = GameState->VolumeRange;
      }
    }
    if (Controller->ActionDown.HalfTransitionCount > 0 &&
        Controller->ActionDown.EndedDown) {
      GameState->Volume--;
      if (GameState->Volume <= -GameState->VolumeRange) {
        GameState->Volume = -GameState->VolumeRange;
      }
    }
    if (Controller->ActionLeft.HalfTransitionCount > 0 &&
        Controller->ActionLeft.EndedDown) {
      GameState->Muted = !GameState->Muted;
    }

    if (GameState->ControllerMap.controllers[c]) {
      if (Controller->Back.EndedDown &&
          Controller->Back.HalfTransitionCount > 0) {

        game_entity *Entity = GameState->ControllerMap.controllers[c];
        SetTileKind(&GameState->WorldArena, &GameState->TileMap, Entity->p.X,
                    Entity->p.Y, TILE_WALL);
      }
    }

    if (Controller->ActionRight.HalfTransitionCount > 0 &&
        Controller->ActionRight.EndedDown) {
      if (!GameState->ControllerMap.controllers[c]) {
        if (GameState->EntityCount < ENTITY_MAX) {
          game_entity NewEntity{};
          NewEntity.active = true;
          int32 RandomX = GameState->EntityCount *
                          (RandomNumbers[GameState->EntityCount] % 3 - 1);
          int32 RandomY = GameState->EntityCount *
                          (RandomNumbers[GameState->EntityCount + 42] % 3 - 1);
          NewEntity.p = tile_position{};
          NewEntity.p.X = 0; // RandomX;
          NewEntity.p.Y = 0; // RandomY;
          NewEntity.v = {0.0f, 0.0f};
          NewEntity.s = {50.0f, 50.0f};
          NewEntity.c = {0.2f, 0.1f * GameState->EntityCount, 0.3f};
          GameState->Entities[GameState->EntityCount] = NewEntity;
          GameState->ControllerMap.controllers[c] =
              &GameState->Entities[GameState->EntityCount];
          GameState->CameraTrack = &GameState->Entities[GameState->EntityCount];
          GameState->EntityCount++;
        }
      } else {
        game_velocity v0 = {};
        GameState->ControllerMap.controllers[c]->v = v0;
        GameState->ControllerMap.controllers[c] = 0;
      }
    }
    game_entity *Entity = GameState->ControllerMap.controllers[c];
    if (Entity) {

      game_velocity NewVelocity = {};
      if (Controller->isAnalog) {
        NewVelocity.x = Controller->AverageStickX;
        NewVelocity.y = -Controller->AverageStickY;
      } else {
        if (Controller->MoveLeft.EndedDown) {
          NewVelocity.x -= 1;
        }
        if (Controller->MoveRight.EndedDown) {
          NewVelocity.x += 1;
        }
        if (Controller->MoveUp.EndedDown) {
          NewVelocity.y -= 1;
        }
        if (Controller->MoveDown.EndedDown) {
          NewVelocity.y += 1;
        }
        if (NewVelocity.x != 0 || NewVelocity.y != 0) {
          real32 Speed = sqrtf(NewVelocity.x * NewVelocity.x +
                               NewVelocity.y * NewVelocity.y);
          NewVelocity.x /= Speed;
          NewVelocity.y /= Speed;
        }
      }

      NewVelocity.x *= 512 * Input->DeltaTime;
      NewVelocity.y *= 512 * Input->DeltaTime;
      Entity->v = NewVelocity;
    }
    if (Controller->Menu.EndedDown) {
      *ShallExit = true;
    }
    if (Controller->Back.EndedDown &&
        Controller->Back.HalfTransitionCount > 0 && GameState->JumpTime == 0) {
      GameState->JumpTime += 15;
    }
  }
  for (int e = 0; e < GameState->EntityCount; e++) {
    game_entity *Entity = &GameState->Entities[e];
    if (!Entity->active) {
      continue;
    }

    bool CurrentWall =
        TILE_WALL == GetTileKind(&GameState->TileMap, Entity->p.X, Entity->p.Y);
    {
      tile_position NewPos = Entity->p;
      NewPos.RelX += Entity->v.x / GameState->TileMap.TileWidth;
      TilePositionNormalize(&NewPos);
      if (CurrentWall ||
          TILE_WALL != GetTileKind(&GameState->TileMap, NewPos.X, NewPos.Y)) {
        Entity->p = NewPos;
      }
    }
    {
      tile_position NewPos = Entity->p;
      NewPos.RelY += Entity->v.y / GameState->TileMap.TileHeight;
      TilePositionNormalize(&NewPos);
      if (CurrentWall ||
          TILE_WALL != GetTileKind(&GameState->TileMap, NewPos.X, NewPos.Y)) {
        Entity->p = NewPos;
      }
    }
  }
  if (GameState->CameraTrack) {
    game_entity *Entity = GameState->CameraTrack;
    tile_distance Distance = TileDistance(GameState->Camera.pos, Entity->p);
    real32 ScreenDistX = Distance.DX * GameState->TileMap.TileWidth +
                         Distance.DRelX * GameState->TileMap.TileWidth;
    real32 ScreenDistY = Distance.DY * GameState->TileMap.TileHeight +
                         Distance.DRelY * GameState->TileMap.TileHeight;
    real32 MaxDistX = ScreenBuffer->Width / 3.0f;
    real32 MaxDistY = ScreenBuffer->Height / 3.0f;
    if (ScreenDistX < -MaxDistX || ScreenDistX > MaxDistX) {
      GameState->Camera.pos.X -= Distance.DX;
      // GameState->Camera.pos.RelX -= Distance.DRelX;
    }
    if (ScreenDistY < -MaxDistY || ScreenDistY > MaxDistY) {
      GameState->Camera.pos.Y -= Distance.DY;
      // GameState->Camera.pos.RelY -= Distance.DRelY;
    }
    TilePositionNormalize(&GameState->Camera.pos);
  }

  int PlayerHeight = 50;
  int PlayerWidth = 10;
  int PlayerColor = 0x00ff44;

  ClearScreen(ScreenBuffer, 0x00000000);
  // RenderGradient(ScreenBuffer, GameState->XPos, GameState->YPos,
  // (int32)GameState->Time);

  real32 CenterX = ScreenBuffer->Width / 2.0f -
                   GameState->Camera.pos.X * GameState->TileMap.TileWidth -
                   GameState->Camera.pos.RelX * GameState->TileMap.TileWidth;
  real32 CenterY = ScreenBuffer->Height / 2.0f -
                   GameState->Camera.pos.Y * GameState->TileMap.TileHeight -
                   GameState->Camera.pos.RelY * GameState->TileMap.TileHeight;
  int32 MinX =
      RoundRealToInt(GameState->Camera.pos.X -
                     ScreenBuffer->Width / 2.0f / GameState->TileMap.TileWidth +
                     GameState->Camera.pos.RelX) -
      1;
  int32 MinY = RoundRealToInt(GameState->Camera.pos.Y -
                              ScreenBuffer->Height / 2.0f /
                                  GameState->TileMap.TileHeight +
                              GameState->Camera.pos.RelY) -
               1;

  int32 MaxX =
      RoundRealToInt(GameState->Camera.pos.X +
                     ScreenBuffer->Width / 2.0f / GameState->TileMap.TileWidth +
                     GameState->Camera.pos.RelX) +
      1;
  int32 MaxY = RoundRealToInt(GameState->Camera.pos.Y +
                              ScreenBuffer->Height / 2.0f /
                                  GameState->TileMap.TileHeight +
                              GameState->Camera.pos.RelY) +
               1;
  for (int32 y = MinY; y <= MaxY; y++) {
    for (int32 x = MinX; x <= MaxX; x++) {
      tile_kind Kind = GetTileKind(&GameState->TileMap, x, y);
      switch (Kind) {
      case TILE_WALL: {

        FillRect(ScreenBuffer,
                 CenterX + (x - 0.5f) * GameState->TileMap.TileWidth,
                 CenterY + (y - 0.5f) * GameState->TileMap.TileHeight,
                 CenterX + (x + 0.5f) * GameState->TileMap.TileWidth,
                 CenterY + (y + 0.5f) * GameState->TileMap.TileHeight,
                 game_color_rgb{1.0f, 1.0f, 1.0f});
      } break;
      case TILE_DOOR: {
        FillRect(ScreenBuffer,
                 CenterX + (x - 0.5f) * GameState->TileMap.TileWidth,
                 CenterY + (y - 0.5f) * GameState->TileMap.TileHeight,
                 CenterX + (x + 0.5f) * GameState->TileMap.TileWidth,
                 CenterY + (y + 0.5f) * GameState->TileMap.TileHeight,
                 game_color_rgb{0.0f, 0.0f, 0.0f});
      } break;
      case TILE_EMPTY: {

        FillRect(ScreenBuffer,
                 CenterX + (x - 0.5f) * GameState->TileMap.TileWidth,
                 CenterY + (y - 0.5f) * GameState->TileMap.TileHeight,
                 CenterX + (x + 0.5f) * GameState->TileMap.TileWidth,
                 CenterY + (y + 0.5f) * GameState->TileMap.TileHeight,
                 game_color_rgb{0.01f * x, y % 2 == 0 ? 0.5f : 0.7f,
                                x % 2 == 0 ? 0.6f : 0.8f});
      } break;
      }
    }
  }

  for (int m = 0; m < ArrayCount(Input->Mouse.Buttons); m++) {
    game_button_state Button = Input->Mouse.Buttons[m];
    RenderRect(ScreenBuffer, Input->Mouse.MouseX + 10 * m, Input->Mouse.MouseY,
               10, 10, Button.EndedDown ? 0x00aaaa : 0xffffff);
  }

  for (int e = 0; e < GameState->EntityCount; e++) {
    game_entity *Entity = &GameState->Entities[e];
    if (!Entity->active) {
      continue;
    }
    ////    FillRect(ScreenBuffer,
    ////             Entity->p.x - Entity->s.x / 2 + ScreenBuffer->Width / 2.0f,
    ////             Entity->p.y - Entity->s.y / 2 + ScreenBuffer->Height
    //// 2.0f, /             Entity->p.x + Entity->s.x / 2 + ScreenBuffer->Width
    //// 2.0f, /             Entity->p.y + Entity->s.x / 2 +
    /// ScreenBuffer->Height / 2.0f, /             Entity->c);

    real32 CX =
        Entity->p.X - GameState->Camera.pos.X - GameState->Camera.pos.RelX;
    real32 CY =
        Entity->p.Y - GameState->Camera.pos.Y - GameState->Camera.pos.RelY;
    real32 CXR = Entity->p.RelX;
    real32 CYR = Entity->p.RelY;
    real32 X =
        CX * GameState->TileMap.TileWidth + CXR * GameState->TileMap.TileWidth;
    real32 Y = CY * GameState->TileMap.TileHeight +
               CYR * GameState->TileMap.TileHeight;

    FillRect(
        ScreenBuffer,
        (CX - 0.2f) * GameState->TileMap.TileWidth + ScreenBuffer->Width / 2.0f,
        (CY - 0.2f) * GameState->TileMap.TileHeight +
            ScreenBuffer->Height / 2.0f,
        (CX + 0.2f) * GameState->TileMap.TileWidth + ScreenBuffer->Width / 2.0f,
        (CY + 0.2f) * GameState->TileMap.TileHeight +
            ScreenBuffer->Height / 2.0f,
        game_color_rgb{0.0f, 0.0f, 0.0f});

    FillRectTexture(
        ScreenBuffer, X - Entity->s.x / 2 + ScreenBuffer->Width / 2.0f,
        Y - Entity->s.y / 2 + ScreenBuffer->Height / 2.0f,
        X + Entity->s.x / 2 + ScreenBuffer->Width / 2.0f,
        Y + Entity->s.x / 2 + ScreenBuffer->Height / 2.0f, &GameState->Logo);
  }

  int BX = 0;
  int BY = 0;

  GameState->Time++;
}
