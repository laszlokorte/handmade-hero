#if !defined(HANDMADE_ASSETS_H)

#include "handmade_types.h"

struct handmade_assets {};

struct bitmap_id {
  uint32 Value;
};
struct sound_id {
  uint32 Value;
};

struct loaded_bitmap {
  size_t Width;
  size_t Height;
  uint32 *Memory;
};

struct loaded_sound {
  size_t SampleCount;
  size_t ChannelCount;
  uint16 *Memory;
};

enum asset_state {
  AssetState_Unloaded,
  AssetState_Locked,
  AssetState_Queued,
  AssetState_Loaded,
};

struct asset_slot {
  asset_state State;
  union {
    loaded_sound Sound;
    loaded_bitmap Bitmap;
  };
};

#define HANDMADE_ASSETS_H
#endif
