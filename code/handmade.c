#include "./handmade.h"
#include "handmade_types.h"
#include "entropy.h"
#include "renderer.h"
#include "handmade_math.h"
#include "tilemap.h"
#include "renderer.cpp"

global_variable game_state global_game_state = {0};
typedef struct bit_scan_result {
  bool Found;
  uint32 Index;
} bit_scan_result;
internal bit_scan_result FindLowestBit(uint32 Value) {
  bit_scan_result Result = {0};
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
typedef struct bitmap_header {
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
} bitmap_header;
#pragma pack(pop)

internal
    loaded_bitmap DEBUGLoadBMP(thread_context *Context,
                               debug_platform_read_entire_file *ReadEntireFile,
                               char *FileName) {

  loaded_bitmap Result = {0};
  debug_read_file_result ReadResult = ReadEntireFile(Context, FileName);

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

real32 SoundEnvelope(int32 Progress, int32 Duration) {
  int Attack = Duration / 4;
  int Decay = 2 * Duration / 4;
  int Sustain = 3 * Duration / 4;
  if (Progress < Attack) {
    return (real32)Progress / (real32)Attack;
  }
  if (Progress < Decay) {
    return 1.0f - 0.5f * (real32)(Progress - Attack) / (real32)(Decay - Attack);
  }
  if (Progress < Sustain) {
    return 0.5f;
  }
  return 0.5f -
         0.5f * (real32)(Progress - Sustain) / (real32)(Duration - Sustain);
}

internal void GameOutputSound(bool muted, game_sound_state *SoundState,
                              real32 BaseVolume,
                              game_sound_output_buffer *SoundBuffer) {

  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
       ++SampleIndex) {
    game_sound_synth **CurrentSoundPointer = &SoundState->PlayingSound;
    float SampleValue = 0;
    real32 toneHz = 440;
    real32 ToneVolume = muted ? 0 : (2.0f * powf(2.0f, 8 + 4 * BaseVolume));
    while (*CurrentSoundPointer) {
      game_sound_synth *CurrentSound = *CurrentSoundPointer;
      if (CurrentSound->Duration >= CurrentSound->Progress) {
        if (CurrentSound->Progress >= 0) {
          real32 f = toneHz / (real32)SoundBuffer->SamplesPerSecond *
                     powf(2.0f, NOTE_HALFTONE * (real32)CurrentSound->Note);
          real32 ThisSampleValue =
              (sinf(2.0f * Pi32 * CurrentSound->GeneratorTimeInRadians) *
               ToneVolume * CurrentSound->ToneBaseVolume *
               SoundEnvelope(CurrentSound->Progress, CurrentSound->Duration));
          SampleValue += ThisSampleValue;

          CurrentSound->GeneratorTimeInRadians =
              fmodf(CurrentSound->GeneratorTimeInRadians + f, 1.0);
        }

        CurrentSound->Progress += 1;
        CurrentSoundPointer = &CurrentSound->NextSound;
      } else {
        game_sound_synth *Next = CurrentSound->NextSound;
        *CurrentSoundPointer = Next;
        CurrentSound->NextSound = SoundState->FreeSound;
        SoundState->FreeSound = CurrentSound;
      }
    }

    int16 finalSample = (int16)SampleValue;
    *SampleOut++ = finalSample;
    *SampleOut++ = finalSample;
  }
}

void GameGetSoundSamples(thread_context *Context, game_memory *Memory,
                         game_sound_output_buffer *SoundBuffer) {
  Assert(Memory->PermanentStorageSize > sizeof(game_state));

  game_state *GameState = (game_state *)Memory->PermanentStorage;

  if (Memory->Initialized) {
    GameOutputSound(GameState->Muted, &GameState->SoundState,
                    ((real32)GameState->Volume) / GameState->VolumeRange,
                    SoundBuffer);
  }
}

internal tile_chunk *GetOrCreateTileChunk(memory_arena *Arena, tile_map *Map,
                                          int ChunkX, int ChunkY) {
  for (memory_index i = 0; i < Map->MaxChunks; i++) {
    memory_index Hash = (ChunkX * 7 + ChunkY * 13 + i * 11) % Map->MaxChunks;

    tile_hash_entry *Entry = &Map->Chunks[Hash];
    if (Entry->Removed || Entry->Chunk == 0) {
      tile_chunk *NewChunk = ArenaPushStruct(Arena, tile_chunk);
      if (!NewChunk) {
        break;
      }
      Entry->Chunk = NewChunk;
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
  for (memory_index i = 0; i < Map->MaxChunks; i++) {
    memory_index Hash = (ChunkX * 7 + ChunkY * 13 + i * 11) % Map->MaxChunks;

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
  chunk_tile_position Result = {0};
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

void GamePlaySound(game_sound_state *SoundState, int32 Note, int32 Duration,
                   real32 Volume, int32 Delay) {
  if (!SoundState->FreeSound) {
    game_sound_synth *FreeSound =
        ArenaPushStruct(&SoundState->SoundArena, game_sound_synth);
    if (!FreeSound) {
      return;
    }
    FreeSound->NextSound = SoundState->FreeSound;
    SoundState->FreeSound = FreeSound;
  }
  game_sound_synth *PlayingSound = SoundState->FreeSound;
  SoundState->FreeSound = PlayingSound->NextSound;
  PlayingSound->Duration = Duration;
  PlayingSound->Progress = -Delay;
  PlayingSound->ToneBaseVolume = Volume;
  PlayingSound->Note = Note;
  PlayingSound->GeneratorTimeInRadians = 0;
  PlayingSound->NextSound = SoundState->PlayingSound;
  SoundState->PlayingSound = PlayingSound;
}

void TestTask(void *Data) {
  game_state *GameState = (game_state *)Data;

  GamePlaySound(&GameState->SoundState, 4, 5000, 1.5, 0);
  GamePlaySound(&GameState->SoundState, 6, 5000, 1.5, 3000);
}
void TestTask2(void *Data) {
  game_state *GameState = (game_state *)Data;

  GamePlaySound(&GameState->SoundState, 1, 5000, 1.5, 0);
  GamePlaySound(&GameState->SoundState, -1, 5000, 1.5, 3000);
}

bool GameUpdateAndRender(thread_context *Context, game_memory *Memory,
                         game_input *Input, render_buffer *RenderBuffer) {
  Assert(Memory->PermanentStorageSize > sizeof(game_state));

  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->Initialized) {
    game_state NewGameState = {0};
    *GameState = NewGameState;
    GameState->Time = 0;
    game_sound_state SoundState = {};
    GameState->SoundState = SoundState;
    GameState->VolumeRange = 5;
    GameState->Volume = 4;
    GameState->Muted = false;
    GameState->JumpTime = 0;
    GameState->Camera.pos.X = 0;
    GameState->Camera.pos.Y = 0;
    GameState->Camera.pos.RelX = 0;
    GameState->Camera.pos.RelY = 0;
    GameState->Camera.ZoomLevel = 0;
    GameState->TileMap.TileWidth = 128;
    GameState->TileMap.TileHeight = 128;
    GameState->TileMap.ChunkHeight = 64;
    GameState->TileMap.ChunkWidth = 64;
    GameState->TileMap.MaxChunks = 256;
    char FileName[] = "./data/logo.bmp";

    GameState->Logo =
        DEBUGLoadBMP(Context, Memory->DebugPlatformReadEntireFile, FileName);

    InitializeArena(&GameState->WorldArena,
                    Memory->PermanentStorageSize - sizeof(*GameState) -
                        sizeof(game_sound_synth) * 100,
                    Memory->PermanentStorage + sizeof(*GameState) +
                        sizeof(game_sound_synth) * 100);
    InitializeArena(&GameState->SoundState.SoundArena,
                    sizeof(game_sound_synth) * 100,
                    Memory->PermanentStorage + sizeof(*GameState));
    GameState->TileMap.Chunks = ArenaPushArray(
        &GameState->WorldArena, tile_hash_entry, GameState->TileMap.MaxChunks);

    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 0, TILE_WALL);
    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 2, TILE_WALL);
    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 1, TILE_WALL);
    SetTileKind(&GameState->WorldArena, &GameState->TileMap, 2, 3, TILE_WALL);

    Memory->Initialized = true;
  }

  {
    real32 OldZoomFactor = powf(2.0f, GameState->Camera.ZoomLevel);
    tile_position MouseTilePos = {0};
    MouseTilePos.RelX =
        (Input->Mouse.MouseX - RenderBuffer->Viewport.Width / 2.0f) /
            OldZoomFactor / GameState->TileMap.TileWidth +
        GameState->Camera.pos.X + GameState->Camera.pos.RelX;
    MouseTilePos.RelY =
        (Input->Mouse.MouseY - RenderBuffer->Viewport.Height / 2.0f) /
            OldZoomFactor / GameState->TileMap.TileHeight +
        GameState->Camera.pos.Y + GameState->Camera.pos.RelY;

    TilePositionNormalize(&MouseTilePos);
    TilePositionNormalize(&GameState->Camera.pos);
    if (GameState->JumpTime > 0) {
      GameState->JumpTime--;
    }
    if (Input->Controllers[0].Menu.EndedDown) {

      float oldZoomLevel = GameState->Camera.ZoomLevel;
      GameState->Camera.ZoomLevel -= Input->Mouse.WheelY / 512.0f;
      float MaxZoom = 1;
      float MinZoom = -3;
      if (GameState->Camera.ZoomLevel > MaxZoom) {
        GameState->Camera.ZoomLevel = MaxZoom;
      }
      if (GameState->Camera.ZoomLevel < MinZoom) {
        GameState->Camera.ZoomLevel = MinZoom;
      }

      if (Input->Mouse.InRange) {

        float zoomDelta = GameState->Camera.ZoomLevel - oldZoomLevel;
        float k = powf(2.0f, zoomDelta);

        float zoomPanX =
            (1.0f - 1.0f / k) * (MouseTilePos.X - GameState->Camera.pos.X);
        float zoomPanY =
            (1.0f - 1.0f / k) * (MouseTilePos.Y - GameState->Camera.pos.Y);
        float zoomPanXRel =
            (1 - 1 / k) * (MouseTilePos.RelX - GameState->Camera.pos.RelX);
        float zoomPanYRel =
            (1 - 1 / k) * (MouseTilePos.RelY - GameState->Camera.pos.RelY);
        GameState->Camera.pos.RelX += zoomPanXRel + zoomPanX;
        GameState->Camera.pos.RelY += zoomPanYRel + zoomPanY;
        TilePositionNormalize(&GameState->Camera.pos);
      }
    } else {
      GameState->Camera.pos.RelX +=
          Input->Mouse.WheelX / 128.0f / OldZoomFactor;
      GameState->Camera.pos.RelY +=
          Input->Mouse.WheelY / 128.0f / OldZoomFactor;

      TilePositionNormalize(&GameState->Camera.pos);
    }
    if (Input->Mouse.Buttons[1].EndedDown) {
      GameState->Camera.pos.RelX -=
          Input->Mouse.DeltaX / 128.0f / OldZoomFactor;
      GameState->Camera.pos.RelY -=
          Input->Mouse.DeltaY / 128.0f / OldZoomFactor;

      TilePositionNormalize(&GameState->Camera.pos);
    }
  }
  float ZoomFactor = powf(2.0f, GameState->Camera.ZoomLevel);
  for (size_t c = 0; c < ArrayCount(Input->Controllers); c++) {
    game_controller_input *Controller = &Input->Controllers[c];

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

        GamePlaySound(&GameState->SoundState, -15, 7000, 1.5, 0);
        GamePlaySound(&GameState->SoundState, -13, 7000, 1.5, 6000);
      }
    }

    if (Controller->ActionRight.HalfTransitionCount > 0 &&
        Controller->ActionRight.EndedDown) {
      if (!GameState->ControllerMap.controllers[c]) {
        if (GameState->EntityCount < ENTITY_MAX) {
          game_entity NewEntity = {0};
          NewEntity.active = true;
          int32 RandomX = GameState->EntityCount *
                          (RandomNumbers[GameState->EntityCount] % 3 - 1);
          int32 RandomY = GameState->EntityCount *
                          (RandomNumbers[GameState->EntityCount + 42] % 3 - 1);
          tile_position pos = {0};
          NewEntity.p.X = 0;
          NewEntity.p.Y = 0;
          NewEntity.p.RelX = 0;
          NewEntity.p.RelY = 0;

          game_velocity NewVelocity = {0};
          game_color_rgb NewColor = {0.2f, 0.1f * GameState->EntityCount, 0.3f};
          game_size NewSize = {128.0f, 128.0f};
          NewEntity.v = NewVelocity;
          NewEntity.s = NewSize;
          NewEntity.c = NewColor;
          GameState->Entities[GameState->EntityCount] = NewEntity;
          GameState->ControllerMap.controllers[c] =
              &GameState->Entities[GameState->EntityCount];
          GameState->CameraTrack = &GameState->Entities[GameState->EntityCount];
          GameState->EntityCount++;
          Memory->PlatformPushTaskToQueue(Memory->TaskQueue, TestTask,
                                          GameState);
          GameState->Camera.pos = NewEntity.p;
        }
      } else {
        game_velocity v0 = {0};
        GameState->ControllerMap.controllers[c]->v = v0;
        GameState->ControllerMap.controllers[c] = 0;
        GameState->CameraTrack = 0;
        Memory->PlatformPushTaskToQueue(Memory->TaskQueue, TestTask2,
                                        GameState);
      }
      Memory->PlatformWaitForQueueToFinish(Memory->TaskQueue);
    }

    game_entity *Entity = GameState->ControllerMap.controllers[c];
    if (Entity) {

      game_velocity NewVelocity = {0};
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

      NewVelocity.x *= 768 * Input->DeltaTime;
      NewVelocity.y *= 768 * Input->DeltaTime;
      Entity->v = NewVelocity;
    }
    if (Controller->Menu.EndedDown) {
      // return false;
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

    tile_position OldPos = Entity->p;
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

    if (OldPos.X < Entity->p.X) {
      GamePlaySound(&GameState->SoundState, -1, 7000, 1, 0);
    }
    if (OldPos.Y < Entity->p.Y) {
      GamePlaySound(&GameState->SoundState, -2, 7000, 1, 0);
    }

    if (OldPos.X > Entity->p.X) {
      GamePlaySound(&GameState->SoundState, 1, 7000, 1, 0);
    }
    if (OldPos.Y > Entity->p.Y) {
      GamePlaySound(&GameState->SoundState, 2, 7000, 1, 0);
    }

    if (Entity->v.x > 0 && Entity->v.y > 0) {
      Entity->o = GameDirectionSouthEast;
    }
    if (Entity->v.x < 0 && Entity->v.y > 0) {
      Entity->o = GameDirectionSouthWest;
    }
    if (Entity->v.x > 0 && Entity->v.y < 0) {
      Entity->o = GameDirectionNorthEast;
    }
    if (Entity->v.x < 0 && Entity->v.y < 0) {
      Entity->o = GameDirectionNorthWest;
    }
    if (Entity->v.x == 0 && Entity->v.y > 0) {
      Entity->o = GameDirectionJustSouth;
    }
    if (Entity->v.x == 0 && Entity->v.y < 0) {
      Entity->o = GameDirectionJustNorth;
    }
    if (Entity->v.x > 0 && Entity->v.y == 0) {
      Entity->o = GameDirectionJustEast;
    }
    if (Entity->v.x < 0 && Entity->v.y == 0) {
      Entity->o = GameDirectionJustWest;
    }
  }
  if (GameState->CameraTrack) {
    game_entity *Entity = GameState->CameraTrack;
    tile_distance Distance = TileDistance(Entity->p, GameState->Camera.pos);
    real32 ScreenDistX = Distance.DX * GameState->TileMap.TileWidth +
                         Distance.DRelX * GameState->TileMap.TileWidth;
    real32 ScreenDistY = Distance.DY * GameState->TileMap.TileHeight +
                         Distance.DRelY * GameState->TileMap.TileHeight;
    real32 MaxDistX = RenderBuffer->Viewport.Width / 3.0f / ZoomFactor;
    real32 MaxDistY = RenderBuffer->Viewport.Height / 3.0f / ZoomFactor;
    if (ScreenDistX < -MaxDistX) {
      GameState->Camera.pos.RelX +=
          (ScreenDistX + MaxDistX) / GameState->TileMap.TileWidth;
    } else if (ScreenDistX > MaxDistX) {
      GameState->Camera.pos.RelX +=
          (ScreenDistX - MaxDistX) / GameState->TileMap.TileWidth;
    }
    if (ScreenDistY < -MaxDistY) {
      GameState->Camera.pos.RelY +=
          (ScreenDistY + MaxDistY) / GameState->TileMap.TileHeight;
    } else if (ScreenDistY > MaxDistY) {
      GameState->Camera.pos.RelY +=
          (ScreenDistY - MaxDistY) / GameState->TileMap.TileHeight;
    }
    TilePositionNormalize(&GameState->Camera.pos);
  }

  int PlayerHeight = 50;
  int PlayerWidth = 10;
  int PlayerColor = 0x00ff44;

  {

    real32 CenterX =
        (RenderBuffer->Viewport.Width / ZoomFactor / 2.0f -
         GameState->Camera.pos.X * GameState->TileMap.TileWidth -
         GameState->Camera.pos.RelX * GameState->TileMap.TileWidth) *
        ZoomFactor;
    real32 CenterY =
        (RenderBuffer->Viewport.Height / ZoomFactor / 2.0f -
         GameState->Camera.pos.Y * GameState->TileMap.TileHeight -
         GameState->Camera.pos.RelY * GameState->TileMap.TileHeight) *
        ZoomFactor;
    int32 MinX = RoundRealToInt((GameState->Camera.pos.X -
                                 RenderBuffer->Viewport.Width / 2.0f /
                                     GameState->TileMap.TileWidth / ZoomFactor +
                                 GameState->Camera.pos.RelX) -
                                1);
    int32 MinY =
        RoundRealToInt((GameState->Camera.pos.Y -
                        RenderBuffer->Viewport.Height / 2.0f /
                            GameState->TileMap.TileHeight / ZoomFactor +
                        GameState->Camera.pos.RelY) -
                       1);

    int32 MaxX = RoundRealToInt((GameState->Camera.pos.X +
                                 RenderBuffer->Viewport.Width / 2.0f /
                                     GameState->TileMap.TileWidth / ZoomFactor +
                                 GameState->Camera.pos.RelX) +
                                1);
    int32 MaxY =
        RoundRealToInt((GameState->Camera.pos.Y +
                        RenderBuffer->Viewport.Height / 2.0f /
                            GameState->TileMap.TileHeight / ZoomFactor +
                        GameState->Camera.pos.RelY) +
                       1);

    // printf("  x: %d -  %d = %d", MinX, MaxX, MaxX - MinX);
    // printf("  y: %d - %d = %d\n", MinY, MaxY, MaxY - MinY);
    for (int32 y = MinY; y <= MaxY; y++) {
      for (int32 x = MinX; x <= MaxX; x++) {
        tile_kind Kind = GetTileKind(&GameState->TileMap, x, y);
        switch (Kind) {
        case TILE_WALL: {
          render_color_rgba RectColor = {1.0f, 1.0f, 1.0f, 1.0f};
          PushRect(
              RenderBuffer,
              CenterX + (x - 0.5f) * ZoomFactor * GameState->TileMap.TileWidth,
              CenterY + (y - 0.5f) * ZoomFactor * GameState->TileMap.TileHeight,
              CenterX + (x + 0.5f) * ZoomFactor * GameState->TileMap.TileWidth,
              CenterY + (y + 0.5f) * ZoomFactor * GameState->TileMap.TileHeight,
              RectColor);

        } break;
        case TILE_DOOR: {
          render_color_rgba RectColor = {0.0f, 0.0f, 0.0f, 1.0f};
          PushRect(
              RenderBuffer,
              CenterX + (x - 0.5f) * ZoomFactor * GameState->TileMap.TileWidth,
              CenterY + (y - 0.5f) * ZoomFactor * GameState->TileMap.TileHeight,
              CenterX + (x + 0.5f) * ZoomFactor * GameState->TileMap.TileWidth,
              CenterY + (y + 0.5f) * ZoomFactor * GameState->TileMap.TileHeight,
              RectColor);

        } break;
        case TILE_EMPTY: {

          render_color_rgba RectColor = {0.01f * abs(x),
                                         abs(y) % 2 == 0 ? 0.5f : 0.7f,
                                         x % 2 == 0 ? 0.6f : 0.8f, 1.0f};
          PushRect(
              RenderBuffer,
              CenterX + (x - 0.5f) * ZoomFactor * GameState->TileMap.TileWidth,
              CenterY + (y - 0.5f) * ZoomFactor * GameState->TileMap.TileHeight,
              CenterX + (x + 0.5f) * ZoomFactor * GameState->TileMap.TileWidth,
              CenterY + (y + 0.5f) * ZoomFactor * GameState->TileMap.TileHeight,
              RectColor);

        } break;
        }
      }
    }
  }
  for (size_t c = 0; c < ArrayCount(GameState->ControllerMap.controllers);
       c++) {
    game_entity *Entity = GameState->ControllerMap.controllers[c];
    if (!Entity || !Entity->active) {
      continue;
    }
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
    render_color_rgba RectColor = {0.5f, 0.1f, 0.5f, 1.0f};
    PushRect(RenderBuffer,
             (CX - 0.2f) * ZoomFactor * GameState->TileMap.TileWidth +
                 RenderBuffer->Viewport.Width / 2.0f,
             (CY - 0.2f) * ZoomFactor * GameState->TileMap.TileHeight +
                 RenderBuffer->Viewport.Height / 2.0f,
             (CX + 0.2f) * ZoomFactor * GameState->TileMap.TileWidth +
                 RenderBuffer->Viewport.Width / 2.0f,
             (CY + 0.2f) * ZoomFactor * GameState->TileMap.TileHeight +
                 RenderBuffer->Viewport.Height / 2.0f,
             RectColor);
  }

  for (int e = 0; e < GameState->EntityCount; e++) {

    game_entity *Entity = &GameState->Entities[e];
    if (!Entity->active) {
      continue;
    }

    real32 CX =
        Entity->p.X - GameState->Camera.pos.X - GameState->Camera.pos.RelX;
    real32 CY =
        Entity->p.Y - GameState->Camera.pos.Y - GameState->Camera.pos.RelY;
    real32 CXR = Entity->p.RelX;
    real32 CYR = Entity->p.RelY;
    real32 X = (CX * GameState->TileMap.TileWidth +
                CXR * GameState->TileMap.TileWidth) *
               ZoomFactor;
    real32 Y = (CY * GameState->TileMap.TileHeight +
                CYR * GameState->TileMap.TileHeight) *
               ZoomFactor;

    float arrow[6] = {0};
    switch (Entity->o) {

    case GameDirectionJustNorth: {
      arrow[0] = 0.0f;
      arrow[1] = -1.5f;
      arrow[2] = -1.0f;
      arrow[3] = -1.0f;
      arrow[4] = 1.0f;
      arrow[5] = -1.0f;
    } break;
    case GameDirectionJustSouth: {
      arrow[0] = 0.0f;
      arrow[1] = 1.5f;
      arrow[2] = -1.0f;
      arrow[3] = 1.0f;
      arrow[4] = 1.0f;
      arrow[5] = 1.0f;
    } break;
    case GameDirectionJustEast: {
      arrow[0] = 1.5f;
      arrow[1] = 0.0f;
      arrow[2] = 1.0f;
      arrow[3] = -1.0f;
      arrow[4] = 1.0f;
      arrow[5] = 1.0f;
    } break;
    case GameDirectionJustWest: {
      arrow[0] = -1.5f;
      arrow[1] = 0.0f;
      arrow[2] = -1.0f;
      arrow[3] = -1.0f;
      arrow[4] = -1.0f;
      arrow[5] = 1.0f;
    } break;
    case GameDirectionNorthWest: {
      arrow[0] = -1.0f;
      arrow[1] = 0.0f;
      arrow[2] = 0.0f;
      arrow[3] = -1.0f;
      arrow[4] = -1.0f;
      arrow[5] = -1.0f;
    } break;
    case GameDirectionSouthWest: {
      arrow[0] = -1.0f;
      arrow[1] = 0.0f;
      arrow[2] = 0.0f;
      arrow[3] = 1.0f;
      arrow[4] = -1.0f;
      arrow[5] = 1.0f;
    } break;
    case GameDirectionNorthEast: {
      arrow[0] = 1.0f;
      arrow[1] = 0.0f;
      arrow[2] = 0.0f;
      arrow[3] = -1.0f;
      arrow[4] = 1.0f;
      arrow[5] = -1.0f;
    } break;
    case GameDirectionSouthEast: {
      arrow[0] = 1.0f;
      arrow[1] = 0.0f;
      arrow[2] = 0.0f;
      arrow[3] = 1.0f;
      arrow[4] = 1.0f;
      arrow[5] = 1.0f;
    } break;
    }
    render_color_rgba RectColor = {0.6f, 0.0f, 0.8f, 1.0f};
    PushTriangle(RenderBuffer,
                 X + (arrow[0] * Entity->s.x / 2.0f) * ZoomFactor +
                     RenderBuffer->Viewport.Width / 2.0f,
                 Y + (arrow[1] * Entity->s.y / 2.0f) * ZoomFactor +
                     RenderBuffer->Viewport.Height / 2.0f,
                 X + (arrow[2] * Entity->s.x / 2.0f) * ZoomFactor +
                     RenderBuffer->Viewport.Width / 2.0f,
                 Y + (arrow[3] * Entity->s.y / 2.0f) * ZoomFactor +
                     RenderBuffer->Viewport.Height / 2.0f,
                 X + (arrow[4] * Entity->s.x / 2.0f) * ZoomFactor +
                     RenderBuffer->Viewport.Width / 2.0f,
                 Y + (arrow[5] * Entity->s.y / 2.0f) * ZoomFactor +
                     RenderBuffer->Viewport.Height / 2.0f,
                 RectColor);
    if (GameState->Logo.Width) {
      PushRectImage(RenderBuffer,
                    X - Entity->s.x / 2 * ZoomFactor +
                        RenderBuffer->Viewport.Width / 2.0f,
                    Y - Entity->s.y / 2 * ZoomFactor +
                        RenderBuffer->Viewport.Height / 2.0f,
                    X + Entity->s.x / 2 * ZoomFactor +
                        RenderBuffer->Viewport.Width / 2.0f,
                    Y + Entity->s.x / 2 * ZoomFactor +
                        RenderBuffer->Viewport.Height / 2.0f,
                    &GameState->Logo);
    } else {

      render_color_rgba RectColor = {0.0f, 0.0f, 0.0f, 0.5f};
      PushRect(RenderBuffer,
               X - Entity->s.x / 2 * ZoomFactor +
                   RenderBuffer->Viewport.Width / 2.0f,
               Y - Entity->s.y / 2 * ZoomFactor +
                   RenderBuffer->Viewport.Height / 2.0f,
               X + Entity->s.x / 2 * ZoomFactor +
                   RenderBuffer->Viewport.Width / 2.0f,
               Y + Entity->s.x / 2 * ZoomFactor +
                   RenderBuffer->Viewport.Height / 2.0f,
               RectColor);
    }
  }

  real32 PaddingH = (real32)min(10, RenderBuffer->Viewport.Width / 2);
  real32 PaddingV = (real32)min(10, RenderBuffer->Viewport.Height / 2);
  render_color_rgba RectColor = {0.0f, 0.0f, 0.0f, 0.5f};
  PushRect(RenderBuffer, RenderBuffer->Viewport.Inset.Left,
           RenderBuffer->Viewport.Inset.Top,
           (real32)RenderBuffer->Viewport.Width -
               RenderBuffer->Viewport.Inset.Right,
           (real32)min(RenderBuffer->Viewport.Inset.Top + 64,
                       (int32)RenderBuffer->Viewport.Height -
                           RenderBuffer->Viewport.Inset.Bottom),
           RectColor);

  for (size_t h = 0; h < ArrayCount(Input->Hands); h++) {
    for (size_t f = 0; f < ArrayCount(Input->Hands[h].Fingers); f++) {
      game_finger_input Finger = Input->Hands[h].Fingers[f];
      if (!Finger.Touches) {
        continue;
      }
      render_color_rgba Col1 = {1.0f, 0.8f, 0.2f, 1.0f};
      render_color_rgba Col2 = {1.0f, 0.0f, 0.9f, 1.0f};
      PushRect(RenderBuffer, (real32)Finger.TipX - 20.0f,
               (real32)Finger.TipY - 20.0f, (real32)Finger.TipX + 20.0f,
               (real32)Finger.TipY + 20.0f, Col1);
    }
  }
  {
    for (size_t h = 0; h < ArrayCount(Input->Hands); h++) {
      for (size_t f = 0; f < ArrayCount(Input->Hands[h].Fingers); f++) {
        game_finger_input Finger = Input->Hands[h].Fingers[f];
        if (!Finger.Touches) {
          continue;
        }
        tile_position FingerTilePos = {0};
        FingerTilePos.RelX =
            (Finger.TipX - RenderBuffer->Viewport.Width / 2.0f) / ZoomFactor /
                GameState->TileMap.TileWidth +
            GameState->Camera.pos.X + GameState->Camera.pos.RelX;
        FingerTilePos.RelY =
            (Finger.TipY - RenderBuffer->Viewport.Height / 2.0f) / ZoomFactor /
                GameState->TileMap.TileHeight +
            GameState->Camera.pos.Y + GameState->Camera.pos.RelY;

        TilePositionNormalize(&FingerTilePos);
        SetTileKind(&GameState->WorldArena, &GameState->TileMap,
                    FingerTilePos.X, FingerTilePos.Y, TILE_WALL);
      }
    }
  }

  if (Input->Mouse.InRange) {
    tile_position MouseTilePos = {0};
    MouseTilePos.RelX =
        (Input->Mouse.MouseX - RenderBuffer->Viewport.Width / 2.0f) /
            ZoomFactor / GameState->TileMap.TileWidth +
        GameState->Camera.pos.X + GameState->Camera.pos.RelX;
    MouseTilePos.RelY =
        (Input->Mouse.MouseY - RenderBuffer->Viewport.Height / 2.0f) /
            ZoomFactor / GameState->TileMap.TileHeight +
        GameState->Camera.pos.Y + GameState->Camera.pos.RelY;

    TilePositionNormalize(&MouseTilePos);
    bool AnyMouseDown = false;

    for (size_t m = 0; m < ArrayCount(Input->Mouse.Buttons); m++) {

      game_button_state Button = Input->Mouse.Buttons[m];
      render_color_rgba Col1 = {1.0f, 0.8f, 0.2f, 1.0f};
      render_color_rgba Col2 = {1.0f, 0.0f, 0.9f, 1.0f};
      render_color_rgba Col3 = {1.0f, 1.0f, 1.0f, 0.8f};
      PushRect(RenderBuffer, (real32)Input->Mouse.MouseX + 10.0f * m,
               (real32)Input->Mouse.MouseY,
               (real32)Input->Mouse.MouseX + 10.0f * m + 10.0f,
               (real32)Input->Mouse.MouseY + 10.0f +
                   10.0f * Button.HalfTransitionCount,
               Button.EndedDown ? (Button.HalfTransitionCount > 0 ? Col1 : Col2)
                                : Col3);
      if (m == 1) {
        continue;
      }
      if (Button.EndedDown) {
        AnyMouseDown = true;
      }
      if (Button.HalfTransitionCount > 0 && Button.EndedDown) {
        GamePlaySound(&GameState->SoundState, (int)m - 10, 7000, 1, 0);
      }
    }
    if (AnyMouseDown) {
      real32 CenterX =
          (RenderBuffer->Viewport.Width / ZoomFactor / 2.0f -
           GameState->Camera.pos.X * GameState->TileMap.TileWidth -
           GameState->Camera.pos.RelX * GameState->TileMap.TileWidth) *
          ZoomFactor;
      real32 CenterY =
          (RenderBuffer->Viewport.Height / ZoomFactor / 2.0f -
           GameState->Camera.pos.Y * GameState->TileMap.TileHeight -
           GameState->Camera.pos.RelY * GameState->TileMap.TileHeight) *
          ZoomFactor;
      if (Input->Mouse.Buttons[0].EndedDown) {
        SetTileKind(&GameState->WorldArena, &GameState->TileMap, MouseTilePos.X,
                    MouseTilePos.Y, TILE_WALL);
      } else if (Input->Mouse.Buttons[2].EndedDown) {

        SetTileKind(&GameState->WorldArena, &GameState->TileMap, MouseTilePos.X,
                    MouseTilePos.Y, TILE_EMPTY);
      }

      {

        render_color_rgba RectColor = {0.7f, 0.3f, 0.3f, 0.6f};
        PushRect(RenderBuffer,
                 CenterX + (MouseTilePos.X - 0.5f) * ZoomFactor *
                               GameState->TileMap.TileWidth,
                 CenterY + (MouseTilePos.Y - 0.5f) * ZoomFactor *
                               GameState->TileMap.TileHeight,
                 CenterX + (MouseTilePos.X + 0.5f) * ZoomFactor *
                               GameState->TileMap.TileWidth,
                 CenterY + (MouseTilePos.Y + 0.5f) * ZoomFactor *
                               GameState->TileMap.TileHeight,
                 RectColor);
      }
    }
  }
  Memory->PlatformWaitForQueueToFinish(Memory->TaskQueue);
  GameState->Time++;
  return true;
}
