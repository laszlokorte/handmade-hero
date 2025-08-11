#include "./handmade.h"
#include "math.h"

global_variable game_state global_game_state = {};

internal void GameOutputSound(int32 note, int32 volume,
                              game_sound_output_buffer *SoundBuffer) {
  local_persist real32 tSin = 0;
  real32 ToneVolumne = (3000.0f * powf(2.0f, (real32)volume / 5.0f));
  real32 toneHz = 440;
  real32 f = toneHz / (real32)SoundBuffer->SamplesPerSecond *
             powf(2.0f, NOTE_HALFTONE * (real32)note);
  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
       ++SampleIndex) {
    int16 SampleValue = (int16)(sinf(2.0f * Pi32 * tSin) * ToneVolumne);
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

    tSin = fmodf(tSin + f, 1.0);
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

internal void GameGetSoundSamples(game_memory *Memory,
                                  game_sound_output_buffer *SoundBuffer) {
  Assert(Memory->PermanentStorageSize > sizeof(game_state));

  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->Initialized) {
    GameState->time = 0;
    GameState->note = 0;
    GameState->volume = 5;
    GameState->xpos = 0;
    GameState->ypos = 0;
    Memory->Initialized = true;
  }

  GameOutputSound(GameState->note, GameState->volume, SoundBuffer);
}
internal void GameUpdateAndRender(game_memory *Memory, game_input *Input,
                                  game_offscreen_buffer *ScreenBuffer,
                                  bool *ShallExit) {
  Assert(Memory->PermanentStorageSize > sizeof(game_state));

  game_state *GameState = (game_state *)Memory->PermanentStorage;
  if (!Memory->Initialized) {
    GameState->time = 0;
    GameState->note = 0;
    GameState->volume = 5;
    GameState->xpos = 0;
    GameState->ypos = 0;
    Memory->Initialized = true;
  }

  for (size_t c = 0; c < ArrayCount(Input->Controllers); c++) {
    game_controller_input *Controller = &Input->Controllers[c];
    if (Controller->LeftShoulder.HalfTransitionCount > 0 &&
        Controller->LeftShoulder.EndedDown) {
      GameState->note--;
    }
    if (Controller->RightShoulder.HalfTransitionCount > 0 &&
        Controller->RightShoulder.EndedDown) {
      GameState->note++;
    }
    if (Controller->ActionUp.HalfTransitionCount > 0 &&
        Controller->ActionUp.EndedDown) {
      GameState->volume++;
    }
    if (Controller->ActionDown.HalfTransitionCount > 0 &&
        Controller->ActionDown.EndedDown) {
      GameState->volume--;
    }
    if (Controller->isAnalog) {

    } else {
      if (Controller->MoveLeft.EndedDown) {
        GameState->xpos -= 10;
      }
      if (Controller->MoveRight.EndedDown) {
        GameState->xpos += 10;
      }
      if (Controller->MoveUp.EndedDown) {
        GameState->ypos += 10;
      }
      if (Controller->MoveDown.EndedDown) {
        GameState->ypos -= 10;
      }
    }

    if (Controller->Menu.EndedDown) {
      *ShallExit = true;
    }
  }

  RenderGradient(ScreenBuffer, GameState->xpos, GameState->ypos,
                 (int32)GameState->time);
  GameState->time++;
}
