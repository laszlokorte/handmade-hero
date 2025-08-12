#include "./handmade.h"
#include "math.h"

global_variable game_state global_game_state = {};

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
    GameState->Time = 0;
    GameState->Note = 0;
    GameState->Volume = 5;
    GameState->XPlayer= 0;
    GameState->YPlayer= 0;
    GameState->XPos = 0;
    GameState->YPos = 0;
    GameState->Muted = true;
    Memory->Initialized = true;
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
    if (Controller->isAnalog) {

    } else {
      if (Controller->MoveLeft.EndedDown) {
        GameState->XPos -= 10;
        GameState->XPlayer -= 5;
      }
      if (Controller->MoveRight.EndedDown) {
        GameState->XPos += 10;
        GameState->XPlayer += 5;
      }
      if (Controller->MoveUp.EndedDown) {
        GameState->YPos -= 10;
        GameState->YPlayer -= 5;
      }
      if (Controller->MoveDown.EndedDown) {
        GameState->YPos += 10;
        GameState->YPlayer += 5;
      }
    }

    if (Controller->Menu.EndedDown) {
      *ShallExit = true;
    }
  }

  RenderGradient(ScreenBuffer, GameState->XPos, GameState->YPos,
                 (int32)GameState->Time);
  RenderRect(ScreenBuffer, GameState->XPlayer + ScreenBuffer->Width/2-20, GameState->YPlayer + ScreenBuffer->Height/2-30, 40, 60, 0xffffff);
  GameState->Time++;
}
