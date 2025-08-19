#include "./handmade.h"
#include "math.h"

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

internal void GameOutputSound(bool muted, int32 Note, int32 Volume,
                              game_sound_output_buffer *SoundBuffer) {

  local_persist real32 tSin = 0;
  real32 ToneVolumne =
      muted ? 0 : (3000.0f * powf(2.0f, (real32)Volume / 5.0f));
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
    real32 Alpha = (((Fg>>24) & 0xff) / 255.0f);
    return lerpColor(Alpha, Bg, Fg);
}

internal uint32 SampleBitmapBilinear(loaded_bitmap *Bitmap, real32 x, real32 y) {
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
    GameOutputSound(GameState->Muted, GameState->Note, GameState->Volume,
                    SoundBuffer);
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
    GameState->Volume = 5;
    GameState->XPlayer = 0;
    GameState->YPlayer = 0;
    GameState->XPos = 0;
    GameState->YPos = 0;
    GameState->Muted = true;
    GameState->JumpTime = 0;
    char FileName[] = "./data/logo.bmp";
    GameState->Logo =
        DEBUGLoadBMP(Context, Memory->DebugPlatformReadEntireFile, FileName);
    InitializeArena(&GameState->WorldArena,
                    Memory->PermanentStorageSize - sizeof(*GameState),
                    Memory->PermanentStorage + sizeof(*GameState));
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
    }
    if (Controller->ActionDown.HalfTransitionCount > 0 &&
        Controller->ActionDown.EndedDown) {
      GameState->Volume--;
    }
    if (Controller->ActionLeft.HalfTransitionCount > 0 &&
        Controller->ActionLeft.EndedDown) {
      GameState->Muted = !GameState->Muted;
    }

    if (Controller->ActionRight.HalfTransitionCount > 0 &&
        Controller->ActionRight.EndedDown) {
      if (!GameState->ControllerMap.controllers[c]) {
        if (GameState->EntityCount < ENTITY_MAX) {
          game_entity NewEntity{};
          NewEntity.active = true;
          NewEntity.p = {0.0f, 10.0f};
          NewEntity.v = {0.0f, 0.0f};
          NewEntity.s = {50.0f, 50.0f};
          NewEntity.c = {0.2f, 0.1f * GameState->EntityCount, 0.3f};
          GameState->Entities[GameState->EntityCount] = NewEntity;
          GameState->ControllerMap.controllers[c] =
              &GameState->Entities[GameState->EntityCount];
          GameState->EntityCount++;
        }
      } else {
        //GameState->ControllerMap.controllers[c]->active = false;
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
      GameState->YPlayer -= 10;
    }
  }
  for (int e = 0; e < GameState->EntityCount; e++) {
    game_entity *Entity = &GameState->Entities[e];
    if (!Entity->active) {
      continue;
    }
    Entity->p.x += Entity->v.x;
    Entity->p.y += Entity->v.y;
  }
  int PlayerHeight = 50;
  int PlayerWidth = 10;
  int PlayerColor = 0x00ff44;

  RenderGradient(ScreenBuffer, GameState->XPos, GameState->YPos,
                 (int32)GameState->Time);
  RenderRect(
      ScreenBuffer,
      GameState->XPlayer + ScreenBuffer->Width / 2 - PlayerWidth / 2,
      GameState->YPlayer -
          (int32)(100.0f * sinf((real32)GameState->JumpTime / 15.0f * Pi32)) +
          ScreenBuffer->Height / 2 - PlayerHeight / 2,
      PlayerWidth, PlayerHeight, PlayerColor);

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
////             Entity->p.y - Entity->s.y / 2 + ScreenBuffer->Height / 2.0f,
////             Entity->p.x + Entity->s.x / 2 + ScreenBuffer->Width / 2.0f,
////             Entity->p.y + Entity->s.x / 2 + ScreenBuffer->Height / 2.0f,
////             Entity->c);
    FillRectTexture(ScreenBuffer,
                    Entity->p.x - Entity->s.x / 2 + ScreenBuffer->Width / 2.0f,
                    Entity->p.y - Entity->s.y / 2 + ScreenBuffer->Height / 2.0f,
                    Entity->p.x + Entity->s.x / 2 + ScreenBuffer->Width / 2.0f,
                    Entity->p.y + Entity->s.x / 2 + ScreenBuffer->Height / 2.0f,
                    &GameState->Logo);
  }

  int BX = 0;
  int BY = 0;

  GameState->Time++;
}
