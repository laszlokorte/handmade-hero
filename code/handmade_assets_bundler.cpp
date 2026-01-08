#include "handmade_assets.h"
#include <fcntl.h>
#include <stdio.h>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
internal loaded_bitmap_file LoadBMP(char const *FileName);

int main() {
  assets_file_header Header = {};

  asset_tag tags[7] = {};
  tags[0].Id = 0xff;
  tags[0].Value = 0;
  tags[1].Id = 0xff;
  tags[1].Value = 0;
  tags[2].Id = 0xff;
  tags[2].Value = 0;
  tags[3].Id = 0xff;
  tags[3].Value = 0;
  tags[4].Id = 0xff;
  tags[4].Value = 0;
  asset_type types[1] = {};
  asset assets[1] = {};
  const char *asset_file_names[1] = {};
  asset_file_names[0] = "./data/logo.bmp";
  assets[0].FirstTagIndex = 0;
  assets[0].PastLastTagIndex = 7;
  types[0].FirstAssetIndex = 0;
  types[0].PastLastAssetIndex = 1;
  types[0].TypeId = 0;
  uint32 TagCount = 7;
  uint32 TypeCount = 13;
  uint32 AssetCount = 1;

  uint64 TagOffset = sizeof(Header);
  uint64 TypeOffset = TagOffset + sizeof(asset_tag) * TagCount;
  uint64 AssetsOffset = TypeOffset + sizeof(asset_type) * TypeCount;
  uint64 DataOffset = AssetsOffset + sizeof(asset) * AssetCount;

  Header.MagicValue = HANDMADE_ASSET_MAGIC;
  Header.Version = HANDMADE_ASSET_VERSION;
  Header.TagCount = 0;
  Header.AssetCount = 1;
  Header.AssetTypeCount = 0;
  Header.Tags = TagOffset;
  Header.AssetTypes = TypeOffset;
  Header.Assets = AssetsOffset;

  FILE *Out = fopen("assets.hma", "wb");
  fwrite(&Header, sizeof(Header), 1, Out);
  fwrite(tags, sizeof(asset_tag), TagCount, Out);
  fwrite(types, sizeof(types), TypeCount, Out);
  for (uint32 a = 0; a < AssetCount; a++) {

    fseek(Out, AssetsOffset + sizeof(asset) * a, SEEK_SET);
    fwrite(&assets[a], sizeof(asset), 1, Out);
    loaded_bitmap_file BitmapFile = LoadBMP(asset_file_names[a]);

    fseek(Out, DataOffset, SEEK_SET);
    fwrite(BitmapFile.Bitmap.Memory,
           BitmapFile.Bitmap.Width * BitmapFile.Bitmap.Height * 4 *
               sizeof(char),
           1, Out);
    DataOffset = ftell(Out);

    FileFree(BitmapFile.File.Contents);
  }

  fclose(Out);
}

void FileFree(void *Memory) {
  if (Memory) {
    free(Memory); // On Linux, we can just free
  }
}
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

internal loaded_bitmap_file LoadBMP(char const *FileName) {

  loaded_bitmap_file Result = {};
  read_file_result ReadResult = ReadEntireFile(FileName);
  Result.File = ReadResult;

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

      Result.Bitmap.Memory = Pixels;
      Result.Bitmap.Width = Header->Width;
      Result.Bitmap.Height = Header->Height;
    }
  }

  return Result;
}

read_file_result ReadEntireFile(char const *Filename) {
  read_file_result Result = {0};
  int fd = open(Filename, O_RDONLY);
  if (fd != -1) {
    struct stat st;
    if (fstat(fd, &st) == 0) {
      size_t FileSize = (size_t)st.st_size;
      void *Memory = malloc(FileSize);
      if (Memory) {
        ssize_t BytesRead = read(fd, Memory, FileSize);
        if (BytesRead == (ssize_t)FileSize) {
          Result.Contents = Memory;
          Result.ContentSize = FileSize;
        } else {
          FileFree(Memory);
        }
      }
    }
    close(fd);
  }
  return Result;
}
