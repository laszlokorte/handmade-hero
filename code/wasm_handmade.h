#include "handmade.h"

struct wasm_game {
  game_update_and_render *GameUpdateAndRender;
  game_get_sound_samples *GameGetSoundSamples;
};

struct wasm_state {
  bool Running;
  bool Configured;
  float WindowWidth;
  float WindowHeight;
  struct wasm_game Game;

  game_input GameInputs[2];
  size_t CurrentGameInputIndex;
  game_memory GameMemory;

  size_t TotalMemorySize;
  void *GameMemoryBlock;
  render_buffer RenderBuffer;
};

struct wasm_audio_buffer {
  size_t Size;
  int16_t *Memory;

  uint32 ReadHead;
  uint32 WriteHead;
};
