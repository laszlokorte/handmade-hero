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
    struct loaded_sound Sound;
    struct loaded_bitmap Bitmap;
  };
};

struct asset_tag {
  uint32 Id;
  real32 Value;
};

struct asset_type {
  uint32 TypeId;
  uint32 FirstAssetIndex;
  uint32 PastLastAssetIndex;
};

struct asset {
  uint64 DataOffset;
  uint32 FirstTagIndex;
  uint32 PastLastTagIndex;
};

#define MAX_TAGS 1024

#define MAGIC(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
struct assets_file_header {
#define HANDMADE_ASSET_MAGIC MAGIC('h', 'm', 'h', 'a')
  uint32 MagicValue;
#define HANDMADE_ASSET_VERSION 0;
  uint32 Version;

  uint32 TagCount;
  uint32 AssetTypeCount;
  uint32 AssetCount;

  uint64 Tags;
  uint64 AssetTypes;
  uint64 Assets;
};

typedef struct read_file_result {
  uint32 ContentSize;
  void *Contents;
} read_file_result;

struct loaded_bitmap_file {
  struct read_file_result File;
  struct loaded_bitmap Bitmap;
};

void FileFree(void *Memory);
read_file_result ReadEntireFile(char const *Filename);

#define HANDMADE_ASSETS_H
#endif
