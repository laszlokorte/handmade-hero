#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include <dlfcn.h>
#include "macos_handmade.h"
#include "renderer.cpp"
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <mach-o/dyld.h>
#include <sys/stat.h>
#include "macos_work_queue.mm"

#define USE_METAL
#define USE_AUDIO

static id<MTLDevice> gDevice;
static id<MTLCommandQueue> gQueue;
static id<MTLLibrary> gLib;
static id<MTLRenderPipelineState> gPSO;
static CAMetalLayer *gLayer;
static id<MTLBuffer> gVtx;
static id<MTLSamplerState> sampler;
static id<MTLTexture> texture = 0;
DEBUG_PLATFORM_FREE_FILE_MEMORY(PlatformFreeFileNoop) {}
DEBUG_PLATFORM_READ_ENTIRE_FILE(PlatformReadEntireFileNoop) {
  debug_read_file_result Result = {};

  return Result;
}
DEBUG_PLATFORM_WRITE_ENTIRE_FILE(PlatformWriteEntireFileNoop) { return false; }

PUSH_TASK_TO_QUEUE(PushTaskToQueueNoop) {}

WAIT_FOR_QUEUE_TO_FINISH(WaitForQueueToFinishNoop) {}

GAME_UPDATE_AND_RENDER(GameUpdateAndRenderNoop) { return false; }

GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesNoop) {}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
  if (Memory) {
    free(Memory); // On Linux, we can just free
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  debug_read_file_result Result = {0};
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
          DEBUGPlatformFreeFileMemory(Context, Memory);
        }
      }
    }
    close(fd);
  }
  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
  bool Result = false;
  int fd = open(Filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd != -1) {
    ssize_t BytesWritten = write(fd, Memory, MemorySize);
    if (BytesWritten == (ssize_t)MemorySize) {
      Result = true;
    }
    close(fd);
  }
  return Result;
}

// Tiny shaders as source string (no .metal file needed)
typedef struct {
  float pos[2];
  float col[4];
  float tex[3];
} MetalVertex;

struct MetalVertices {
  size_t Capacity;
  size_t Count;
  MetalVertex *Buffer;
};

static void CreateMetal(NSView *view, CGSize size, MetalVertices *Vertices) {
  gDevice = MTLCreateSystemDefaultDevice();
  gQueue = [gDevice newCommandQueue];

  gLayer = [CAMetalLayer layer];
  gLayer.device = gDevice;
  gLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  gLayer.frame = view.bounds;
  gLayer.drawableSize = size;
  gLayer.contentsScale = NSScreen.mainScreen.backingScaleFactor;
  [view setWantsLayer:YES];
  view.layer = gLayer;

  char exePath[PATH_MAX];
  uint32_t exeSize = sizeof(exePath);
  _NSGetExecutablePath(exePath, &exeSize); // gives path inside .app or binary
  NSString *exeDir = [[NSString stringWithUTF8String:exePath]
      stringByDeletingLastPathComponent];
  NSURL *libURL = [NSURL
      fileURLWithPath:
          [exeDir stringByAppendingPathComponent:@"./macos_shader.metallib"]];
  NSError *err = nil;
  gLib = [gDevice newLibraryWithURL:libURL error:&err];
  if (!gLib) {
    NSLog(@"Shader compile failed: %@", err);
    exit(1);
  }

  id<MTLFunction> vs = [gLib newFunctionWithName:@"v_main"];
  id<MTLFunction> fs = [gLib newFunctionWithName:@"f_main"];

  MTLRenderPipelineDescriptor *p = [MTLRenderPipelineDescriptor new];
  p.vertexFunction = vs;
  p.fragmentFunction = fs;
  p.colorAttachments[0].pixelFormat = gLayer.pixelFormat;

  MTLRenderPipelineColorAttachmentDescriptor *cAtt = p.colorAttachments[0];
  cAtt.blendingEnabled = YES;
  cAtt.rgbBlendOperation = MTLBlendOperationAdd;
  cAtt.alphaBlendOperation = MTLBlendOperationAdd;
  cAtt.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
  cAtt.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
  cAtt.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  cAtt.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

  gPSO = [gDevice newRenderPipelineStateWithDescriptor:p error:&err];
  if (!gPSO) {
    NSLog(@"PSO failed: %@", err);
    exit(1);
  }
  MTLSamplerDescriptor *samplerDesc = [[MTLSamplerDescriptor alloc] init];
  samplerDesc.minFilter = MTLSamplerMinMagFilterLinear;
  samplerDesc.magFilter = MTLSamplerMinMagFilterLinear;
  samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
  samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;

  sampler = [gDevice newSamplerStateWithDescriptor:samplerDesc];
  gVtx = [gDevice newBufferWithLength:sizeof(MetalVertex) * Vertices->Capacity
                              options:MTLResourceStorageModeManaged];
}
static void MetalPushTri(MetalVertices *Vertices, float x0, float y0, float x1,
                         float y1, float x2, float y2, float r, float g,
                         float b, float a) {
  if (Vertices->Capacity < Vertices->Count + 3) {
    // printf("%lu\n", Vertices->Count);
    // printf("%lu\n", Vertices->Capacity);
    return;
  }
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x0, y0}, .col = {r, g, b, a}, .tex = {0.0, 0.0, 0.0}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x1, y1}, .col = {r, g, b, a}, .tex = {0.0, 0.0, 0.0}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x2, y2}, .col = {r, g, b, a}, .tex = {0.0, 0.0, 0.0}};
}

static void MetalPushQuad(MetalVertices *Vertices, float x0, float y0, float x1,
                          float y1, float r, float g, float b, float a,
                          bool tex) {
  if (Vertices->Capacity < Vertices->Count + 6) {
    printf("Metal Vertex Buffer full\n");
    // printf("%lu\n", Vertices->Count);
    // printf("%lu\n", Vertices->Capacity);
    return;
  }
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x0, y0},
      .col = {r, g, b, a},
      .tex = {0.0f, 1.0f, tex ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x1, y0},
      .col = {r, g, b, a},
      .tex = {1.0f, 1.0f, tex ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x0, y1},
      .col = {r, g, b, a},
      .tex = {0.0f, 0.0f, tex ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x1, y0},
      .col = {r, g, b, a},
      .tex = {1.0f, 1.0f, tex ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x1, y1},
      .col = {r, g, b, a},
      .tex = {1.0f, 0.0f, tex ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x0, y1},
      .col = {r, g, b, a},
      .tex = {0.0f, 0.0f, tex ? 1.0f : 0.0f}};
}

typedef struct {
  float scaleX;
  float scaleY;
  float transX;
  float transY;
} MetalUniforms;

static void MetalDraw(MetalVertices *Vertices, float scaleX, float scaleY,
                      float transX, float transY) {
  @autoreleasepool {
    id<CAMetalDrawable> drawable = [gLayer nextDrawable];
    // if (!drawable)
    //   return;
    //

    MTLRenderPassDescriptor *rp =
        [MTLRenderPassDescriptor renderPassDescriptor];
    rp.colorAttachments[0].texture = drawable.texture;
    rp.colorAttachments[0].loadAction = MTLLoadActionClear;
    rp.colorAttachments[0].storeAction = MTLStoreActionStore;
    rp.colorAttachments[0].clearColor =
        MTLClearColorMake(0.10, 0.12, 0.14, 1.0);

    // Triangle in clip space (x,y)
    MetalUniforms uni = {
        scaleX,
        scaleY,
        transX,
        transY,
    };
    memcpy(gVtx.contents, Vertices->Buffer,
           Vertices->Count * sizeof(MetalVertex));

    [gVtx didModifyRange:NSMakeRange(0, Vertices->Count * sizeof(MetalVertex))];
    id<MTLCommandBuffer> cb = [gQueue commandBuffer];
    id<MTLRenderCommandEncoder> enc =
        [cb renderCommandEncoderWithDescriptor:rp];
    [enc setRenderPipelineState:gPSO];
    [enc setVertexBuffer:gVtx offset:0 atIndex:0];
    [enc setVertexBytes:&uni length:sizeof(uni) atIndex:1];
    [enc setFragmentTexture:texture atIndex:0];
    [enc setFragmentSamplerState:sampler atIndex:0];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle
            vertexStart:0
            vertexCount:Vertices->Count];
    [enc endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
  }
}

timespec MacOsGetLastWriteTime(const char *filename) {
  timespec lastWrite = {0, 0};

  struct stat st;
  if (stat(filename, &st) == 0) {
    lastWrite = st.st_mtimespec; // macOS has st_mtimespec
  }

  return lastWrite;
}

void MacOsUnloadGame(macos_game *Game) {
  if (Game->GameDLL) {
    dlclose(Game->GameDLL);
    free(Game->FullDllPath);
    Game->GameDLL = NULL;
  }
  Game->GameUpdateAndRender = GameUpdateAndRenderNoop;
  Game->GameGetSoundSamples = GameGetSoundSamplesNoop;
  Game->IsValid = false;
}

void MacOsLoadGame(macos_game *Game) {
  char exePath[PATH_MAX];
  uint32_t exeSize = sizeof(exePath);
  _NSGetExecutablePath(exePath, &exeSize); // gives path inside .app or binary
  NSString *exeDir = [[NSString stringWithUTF8String:exePath]
      stringByDeletingLastPathComponent];
  NSString *dllPath =
      [exeDir stringByAppendingPathComponent:@"./handmade_game"];
  Game->GameDLL = dlopen([dllPath UTF8String], RTLD_NOW);

  timespec LatestWriteTime = MacOsGetLastWriteTime([dllPath UTF8String]);
  if (Game->GameDLL) {
    Game->GameUpdateAndRender =
        (game_update_and_render *)dlsym(Game->GameDLL, "GameUpdateAndRender");
    Game->GameGetSoundSamples =
        (game_get_sound_samples *)dlsym(Game->GameDLL, "GameGetSoundSamples");
    Game->IsValid = true;
    Game->FullDllPath = strdup([dllPath UTF8String]);
    Game->LatestWriteTime = LatestWriteTime;
  }
}

void MacOsSetupGameMemory(macos_state *MacState, game_memory *GameMemory) {
  uint32 RenderBufferLength = 240000;
  memory_index RenderBufferSize = RenderBufferLength * sizeof(render_command);
  uint32 WorkQueueLength = 128;
  size_t WorkQueueSize = WorkQueueLength * sizeof(macos_work_queue_task);
  GameMemory->Initialized = false;
  GameMemory->PermanentStorageSize = Megabytes(100);
  GameMemory->TransientStorageSize = Megabytes(100);
  MacState->TotalMemorySize = GameMemory->TransientStorageSize +
                              GameMemory->PermanentStorageSize +
                              RenderBufferSize + WorkQueueSize;
  MacState->GameMemoryBlock =
      mmap(NULL, MacState->TotalMemorySize, PROT_READ | PROT_WRITE,
           MAP_ANON | MAP_PRIVATE, -1, 0);
  GameMemory->PermanentStorage = (uint8 *)MacState->GameMemoryBlock;
  GameMemory->TransientStorage =
      (uint8 *)GameMemory->PermanentStorage + GameMemory->PermanentStorageSize;
  InitializeRenderBuffer(&MacState->RenderBuffer, RenderBufferLength,
                         (render_command *)(GameMemory->PermanentStorage +
                                            GameMemory->PermanentStorageSize +
                                            GameMemory->TransientStorageSize));
  InitializeWorkQueue(
      &MacState->WorkQueue, WorkQueueLength,
      (macos_work_queue_task *)(GameMemory->PermanentStorage +
                                GameMemory->PermanentStorageSize +
                                GameMemory->TransientStorageSize +
                                RenderBufferSize));

  GameMemory->TaskQueue = &MacState->WorkQueue;
  GameMemory->PlatformPushTaskToQueue = PushTaskToQueue;
  GameMemory->PlatformWaitForQueueToFinish = WaitForQueueToFinish;
  GameMemory->DebugPlatformReadEntireFile = &DEBUGPlatformReadEntireFile;
  GameMemory->DebugPlatformFreeFileMemory = &DEBUGPlatformFreeFileMemory;
  GameMemory->DebugPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;
}

inline uint32 lerpColor(real32 t, uint32 c1, uint32 c2) {
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

inline uint32 Blend(uint32 Bg, uint32 Fg) {
  real32 Alpha = (((Fg >> 24) & 0xff) / 255.0f);
  return lerpColor(Alpha, Bg, Fg);
}

void MacOsPaintRect(macos_screen_buffer *ScreenBuffer, real32 MinX, real32 MinY,
                    real32 MaxX, real32 MaxY, real32 r, real32 g, real32 b,
                    real32 a) {
  if (MaxX < MinX) {
    real32 tmp = MinX;
    MinX = MaxX;
    MaxX = tmp;
  }
  if (MaxY < MinY) {
    real32 tmp = MinY;
    MinY = MaxY;
    MaxY = tmp;
  }
  if (MaxX < 0)
    MaxX = 0;
  if (MaxY < 0)
    MaxY = 0;
  if (MinX < 0)
    MinX = 0;
  if (MinY < 0)
    MinY = 0;
  if (MinX >= ScreenBuffer->Width)
    MinX = ScreenBuffer->Width - 1;
  if (MinY >= ScreenBuffer->Height)
    MinY = ScreenBuffer->Height - 1;
  if (MaxX >= ScreenBuffer->Width)
    MaxX = ScreenBuffer->Width - 1;
  if (MaxY >= ScreenBuffer->Height)
    MaxY = ScreenBuffer->Height - 1;

  int Color = ((uint8_t)(a * 255) << 24) | ((uint8_t)(b * 255) << 16) |
              ((uint8_t)(g * 255) << 8) | ((uint8_t)(r * 255) << 0);

  uint8_t *Row =
      (uint8_t *)ScreenBuffer->Memory + (ScreenBuffer->Pitch * (int)MinY);
  for (int y = MinY; y < MaxY; y++) {
    uint8_t *Column = Row + (ScreenBuffer->BytesPerPixel * (int)MinX);
    for (int x = MinX; x < MaxX; x++) {
      uint32_t *Pixel = (uint32_t *)Column;
      (*Pixel) = Blend(*Pixel, Color);
      Column += ScreenBuffer->BytesPerPixel;
    }
    Row += ScreenBuffer->Pitch;
  }
}

void MacOsPaint(macos_screen_buffer *ScreenBuffer,
                render_buffer *RenderBuffer) {
  uint8_t *Row = (uint8_t *)ScreenBuffer->Memory;
  for (int y = 0; y < ScreenBuffer->Height; y++) {
    uint8_t *Column = Row;
    for (int x = 0; x < ScreenBuffer->Width; x++) {
      uint32_t *Pixel = (uint32_t *)Column;
      (*Pixel) = 0xff000000;
      Column += ScreenBuffer->BytesPerPixel;
    }
    Row += ScreenBuffer->Pitch;
  }
  for (uint32 ri = 0; ri < RenderBuffer->Count; ri += 1) {

    render_command *RCmd = &RenderBuffer->Base[ri];
    switch (RCmd->Type) {
    case RenderCommandRect: {
      // glColor4f(0.0f,0.0f,0.0f,0.0f);
      if (RCmd->Rect.Image) {
        MacOsPaintRect(ScreenBuffer, RCmd->Rect.MinX, RCmd->Rect.MinY,
                       RCmd->Rect.MaxX, RCmd->Rect.MaxY, 0, 0, 0, 0.3f);
      } else {
        MacOsPaintRect(ScreenBuffer, RCmd->Rect.MinX, RCmd->Rect.MinY,
                       RCmd->Rect.MaxX, RCmd->Rect.MaxY, RCmd->Rect.Color.Red,
                       RCmd->Rect.Color.Green, RCmd->Rect.Color.Blue,
                       RCmd->Rect.Color.Alpha);
      }

    } break;
    case RenderCommandTriangle: {

    } break;
    default: {
      break;
    } break;
    }
  }
}

void MacOsResizeScreenBuffer(macos_screen_buffer *ScreenBuffer, uint32 NewWidth,
                             uint32 NewHeight) {
  if (ScreenBuffer->Memory) {
    uint32_t OldSize = ScreenBuffer->Pitch * ScreenBuffer->Height;
    munmap(&ScreenBuffer->Memory, OldSize);
  }

  uint32_t NewPitch = NewWidth * ScreenBuffer->BytesPerPixel;
  uint32_t NewSize = NewPitch * NewHeight;
  void *NewMem = mmap(NULL, NewSize, PROT_READ | PROT_WRITE,
                      MAP_ANON | MAP_PRIVATE, -1, 0);
  if (NewMem == MAP_FAILED) {
    // handle error
  }
  ScreenBuffer->Memory = NewMem;
  ScreenBuffer->Width = NewWidth;
  ScreenBuffer->Height = NewHeight;
  ScreenBuffer->Pitch = NewPitch;
}

void MacOsSwapWindowBuffer(NSWindow *Window,
                           macos_screen_buffer *ScreenBuffer) {

  @autoreleasepool {
    NSBitmapImageRep *newRep = [[[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:(uint8_t **)&ScreenBuffer->Memory
                      pixelsWide:ScreenBuffer->Width
                      pixelsHigh:ScreenBuffer->Height
                   bitsPerSample:8
                 samplesPerPixel:ScreenBuffer->BytesPerPixel
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:ScreenBuffer->Pitch
                    bitsPerPixel:32] autorelease];
    NSImage *image = [[[NSImage alloc]
        initWithSize:NSMakeSize(ScreenBuffer->Width, ScreenBuffer->Height)]
        autorelease];
    [image addRepresentation:newRep];
    Window.contentView.wantsLayer = YES;
    Window.contentView.layer.contents = image;
  }
}

@interface HandmadeMainWindowDelegate : NSObject <NSWindowDelegate>
@property macos_state *State;
@end

@implementation HandmadeMainWindowDelegate
- (id)initWithState:(macos_state *)state {
  _State = state;
  return self;
}

- (void)windowWillClose:(NSNotification *)notification {
  _State->Running = false;
}
- (void)windowDidResize:(NSNotification *)notification {
  NSWindow *Window = (NSWindow *)notification.object;
#ifdef USE_METAL

#else
  MacOsResizeScreenBuffer(&_State->ScreenBuffer,
                          Window.contentView.frame.size.width,
                          Window.contentView.frame.size.height);

  MacOsPaint(&_State->ScreenBuffer, &_State->RenderBuffer);
  MacOsSwapWindowBuffer(_State->Window, &_State->ScreenBuffer);
#endif
  _State->WindowWidth = Window.contentView.frame.size.width;
  _State->WindowHeight = Window.contentView.frame.size.height;
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
}
@end

OSStatus MacAudioCallback(void *inRefCon,
                          AudioUnitRenderActionFlags *ioActionFlags,
                          const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                          UInt32 inNumberFrames, AudioBufferList *ioData) {
  mac_audio_buffer *data = (mac_audio_buffer *)inRefCon;

  Float32 *out = (Float32 *)ioData->mBuffers[0].mData;
  int16 *source = (int16_t *)data->Memory;
  // printf("read: %d\n", data->ReadHead);
  // printf("data-size: %lu\n", data->Size);
  for (UInt32 i = 0; i < inNumberFrames; i++) {
    uint32_t frameIndex = data->ReadHead % data->Size;
    if (data->ReadHead == data->WriteHead) {
      out[i * 2 + 0] = 0.0f;
      out[i * 2 + 1] = 0.0f;
      continue;
    }
    // printf("%f\n", source[2 * i]/ 655360.0f);
    out[i * 2 + 0] = source[frameIndex * 2 + 0] / (float)(1 << 15);
    out[i * 2 + 1] = source[frameIndex * 2 + 1] / (float)(1 << 15);
    data->ReadHead = (data->ReadHead + 1) % data->Size;
  }
  return noErr;
}

void MacInitAudio(mac_audio_buffer *AudioBuffer) {
  AudioComponentDescription desc = {0};
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;

  AudioComponent comp = AudioComponentFindNext(NULL, &desc);
  AudioUnit audioUnit;
  AudioComponentInstanceNew(comp, &audioUnit);

  // Configure format
  AudioStreamBasicDescription format = {0};
  format.mSampleRate = 44100;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  format.mChannelsPerFrame = 2;
  format.mBytesPerFrame = 2 * sizeof(Float32);
  format.mBytesPerPacket = 2 * sizeof(Float32);
  format.mBitsPerChannel = 8 * sizeof(Float32);
  format.mFramesPerPacket = 1;

  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                       kAudioUnitScope_Input, 0, &format, sizeof(format));

  // Set callback
  AURenderCallbackStruct cb;
  cb.inputProc = MacAudioCallback;
  cb.inputProcRefCon = AudioBuffer;
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                       kAudioUnitScope_Input, 0, &cb, sizeof(cb));

  AudioUnitInitialize(audioUnit);
  AudioOutputUnitStart(audioUnit);
}

void MacOsHandKeyInput(game_button_state *Button, uint32 keyCode,
                       uint32 virtualKey, bool NewDown) {
  bool isKey = keyCode == virtualKey;
  if (isKey) {
    if (Button->EndedDown != NewDown) {
      Button->HalfTransitionCount += 1;
    }
    Button->EndedDown = NewDown;
  }
}

struct macos_frame_measures {
  int64 DeltaTimeMS;
  int32 SkippedFrames;
};

int main(void) {
  macos_state MacOsState = {};
  MacOsState.Running = true;
  MacOsState.WindowWidth = 1200;
  MacOsState.WindowHeight = 800;
  MacOsState.ScreenBuffer.BytesPerPixel = 4;
  macos_frame_measures FrameMeasures = {};
  int64_t LastCounter;

  mach_timebase_info(&GlobalMachTimebase);
  LastCounter = mach_absolute_time();

  MacOsLoadGame(&MacOsState.Game);
  // printf("%p\n%p\n", MacOsState.Game.GameGetSoundSamples,
  //        MacOsState.Game.GameUpdateAndRender);
  // printf("%p\n%d\n", MacOsState.Game.GameDLL, MacOsState.Game.IsValid);
  HandmadeMainWindowDelegate *mainWindowDelegate =
      [[HandmadeMainWindowDelegate alloc] initWithState:&MacOsState];

  NSRect screenRect = [[NSScreen mainScreen] frame];
  NSRect initialFrame =
      NSMakeRect((screenRect.size.width - MacOsState.WindowWidth) / 2,
                 (screenRect.size.height - MacOsState.WindowHeight) / 2,
                 MacOsState.WindowWidth, MacOsState.WindowHeight);

  MacOsState.Window = [[NSWindow alloc]
      initWithContentRect:initialFrame
                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable |
                           NSWindowStyleMaskResizable)
                  backing:NSBackingStoreBuffered
                    defer:YES];
#ifdef USE_METAL
#else
  MacOsResizeScreenBuffer(&MacOsState.ScreenBuffer, MacOsState.WindowWidth,
                          MacOsState.WindowHeight);
#endif

  [MacOsState.Window setBackgroundColor:NSColor.darkGrayColor];
  [MacOsState.Window setTitle:@"Handmade hero"];
  [MacOsState.Window setDelegate:mainWindowDelegate];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp activateIgnoringOtherApps:true];
  [MacOsState.Window makeKeyAndOrderFront:nil];
  MacOsState.Window.contentView.wantsLayer = YES;

  mac_audio_buffer AudioBuffer = {};
  uint32 f = 44100;
  AudioBuffer.Size = f / 8;
  AudioBuffer.WriteHead = 0;
  AudioBuffer.ReadHead = 0;
  AudioBuffer.Memory =
      (int16_t *)mmap(NULL, AudioBuffer.Size * 2 * sizeof(int16_t),
                      PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);

  int16_t *LinearAudioSamples =
      (int16_t *)mmap(NULL, f * 2 * sizeof(int16_t), PROT_READ | PROT_WRITE,
                      MAP_ANON | MAP_PRIVATE, -1, 0);
#ifdef USE_AUDIO
  MacInitAudio(&AudioBuffer);
#endif
#ifdef USE_METAL
  MetalVertices Vertices = {};
  Vertices.Capacity = 128000;
  Vertices.Buffer = (MetalVertex *)mmap(
      NULL, Vertices.Capacity * sizeof(*Vertices.Buffer),
      PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  CreateMetal(MacOsState.Window.contentView, initialFrame.size, &Vertices);
#endif
  game_input GameInputs[2] = {};
  game_input *LastInput = &GameInputs[0];
  game_input *CurrentInput = &GameInputs[1];
  game_memory GameMemory = {};
  MacOsSetupGameMemory(&MacOsState, &GameMemory);
  while (MacOsState.Running) {

    timespec LatestWriteTime =
        MacOsGetLastWriteTime(MacOsState.Game.FullDllPath);
    if (LatestWriteTime.tv_nsec != MacOsState.Game.LatestWriteTime.tv_nsec ||
        LatestWriteTime.tv_sec != MacOsState.Game.LatestWriteTime.tv_sec) {
      MacOsUnloadGame(&MacOsState.Game);
      MacOsLoadGame(&MacOsState.Game);
    }

    game_input *SwapInput = CurrentInput;
    CurrentInput = LastInput;
    LastInput = SwapInput;
    game_input InputReset = {};
    *CurrentInput = InputReset;
    CurrentInput->Mouse.MouseX = LastInput->Mouse.MouseX;
    CurrentInput->Mouse.MouseY = LastInput->Mouse.MouseY;

    CurrentInput->Controllers[0].isAnalog = false;
    for (int b = 0; b < ArrayCount(CurrentInput->Controllers[0].Buttons); b++) {
      CurrentInput->Controllers[0].Buttons[b].EndedDown =
          LastInput->Controllers[0].Buttons[b].EndedDown;
    }
    for (int b = 0; b < ArrayCount(CurrentInput->Mouse.Buttons); b++) {
      CurrentInput->Mouse.Buttons[b].EndedDown =
          LastInput->Mouse.Buttons[b].EndedDown;
    }

    @autoreleasepool {
      NSEvent *Event;

      do {

        Event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                   untilDate:[NSDate distantPast]
                                      inMode:NSEventTrackingRunLoopMode
                                     dequeue:YES];
        if (!Event) {
          break;
        };

        switch ([Event type]) {

        case NSEventTypeKeyDown:
        case NSEventTypeKeyUp: {
          // printf("0x%04x\n", Event.keyCode);
          bool IsDown = Event.type == NSEventTypeKeyDown;
          bool IsShift = Event.modifierFlags & NSEventModifierFlagShift;
          bool IsAlt = Event.modifierFlags & NSEventModifierFlagOption;
          bool IsCmd = Event.modifierFlags & NSEventModifierFlagCommand;
          MacOsHandKeyInput(&CurrentInput->Controllers[0].LeftShoulder,
                            Event.keyCode, kVK_ANSI_Q, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].LeftShoulder,
                            Event.keyCode, kVK_ANSI_E, IsDown);

          MacOsHandKeyInput(&CurrentInput->Controllers[0].MoveUp, Event.keyCode,
                            kVK_ANSI_W, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].MoveDown,
                            Event.keyCode, kVK_ANSI_S, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].MoveLeft,
                            Event.keyCode, kVK_ANSI_A, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].MoveRight,
                            Event.keyCode, kVK_ANSI_D, IsDown);

          MacOsHandKeyInput(&CurrentInput->Controllers[0].ActionUp,
                            Event.keyCode, kVK_UpArrow, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].ActionDown,
                            Event.keyCode, kVK_DownArrow, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].ActionLeft,
                            Event.keyCode, kVK_LeftArrow, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].ActionRight,
                            Event.keyCode, kVK_RightArrow, IsDown);

          MacOsHandKeyInput(&CurrentInput->Controllers[0].Menu, Event.keyCode,
                            kVK_Space, IsDown);
          MacOsHandKeyInput(&CurrentInput->Controllers[0].Back, Event.keyCode,
                            kVK_Delete, IsDown);
          if (IsDown) {
            if ((IsCmd) &&
                [[Event charactersIgnoringModifiers] isEqualToString:@"q"]) {
              MacOsState.Running = false;
              [NSApp terminate:nil]; // or custom handling
            }
            if ((IsCmd) &&
                [[Event charactersIgnoringModifiers] isEqualToString:@"r"]) {
              MacOsUnloadGame(&MacOsState.Game);
              MacOsLoadGame(&MacOsState.Game);
            }
            if ([[Event charactersIgnoringModifiers] isEqualToString:@"9"]) {
              MacOsState.DebugSoundWave = !MacOsState.DebugSoundWave;
            }
            NSString *chars = [Event charactersIgnoringModifiers];
            if (chars.length > 0 &&
                [chars characterAtIndex:0] == 0x1B) { // 0x1B = ESC
              MacOsState.Running = false;
            }
          }
        } break;

        case NSEventTypeScrollWheel: {
          CGFloat dx = Event.scrollingDeltaX;
          CGFloat dy = Event.scrollingDeltaY;
          BOOL precise = Event.hasPreciseScrollingDeltas;
          if (precise) {
            // Typically high-resolution touchpad scroll
          }
          CurrentInput->Mouse.WheelX += dx;
          CurrentInput->Mouse.WheelY += dy;
        } break;

        default: {
          [NSApp sendEvent:Event];
        } break;
        }

      } while (Event != nil);

      NSPoint mousePos = [NSEvent mouseLocation];
      NSPoint posInWindow =
          NSMakePoint(mousePos.x - MacOsState.Window.frame.origin.x,
                      mousePos.y - MacOsState.Window.frame.origin.y);
      NSRect content = [MacOsState.Window
          convertRectToScreen:MacOsState.Window.contentView.frame];
      CurrentInput->Mouse.DeltaX =
          (int)posInWindow.x - CurrentInput->Mouse.MouseX;
      CurrentInput->Mouse.DeltaY =
          (int)(MacOsState.WindowHeight - posInWindow.y) -
          CurrentInput->Mouse.MouseY;
      CurrentInput->Mouse.MouseX = (int)posInWindow.x;
      CurrentInput->Mouse.MouseY =
          (int)(MacOsState.WindowHeight - posInWindow.y);
      if (NSPointInRect(mousePos, content) && [NSApp isActive]) {

        CurrentInput->Mouse.InRange = true;
        for (int m = 0; m < ArrayCount(CurrentInput->Mouse.Buttons); m++) {
          memory_index b = m;
          // swap Mouse button indices to make right mouse button index 2 and
          // middle mouse index 1
          if (m == 1 || m == 2) {
            b = 3 - m;
          }
          bool NewDown = NSEvent.pressedMouseButtons & (1 << m);
          bool toggled = CurrentInput->Mouse.Buttons[b].EndedDown != NewDown;
          CurrentInput->Mouse.Buttons[b].HalfTransitionCount +=
              (NewDown != LastInput->Mouse.Buttons[b].EndedDown ? 1 : 0);
          CurrentInput->Mouse.Buttons[b].EndedDown = NewDown;
        }
      } else {
        CurrentInput->Mouse.InRange = false;
      }
      thread_context Context = {};

#ifdef USE_AUDIO
      uint32_t filled =
          (AudioBuffer.WriteHead + AudioBuffer.Size - AudioBuffer.ReadHead) %
          AudioBuffer.Size;
      // printf("filled: %d\n", filled);
      uint32_t available = AudioBuffer.Size - filled - 1;
      // printf("available: %d\n", available);
      //

      // printf("read,write: %d, %d\n", AudioBuffer.ReadHead,
      //  AudioBuffer.WriteHead);
      game_sound_output_buffer SoundBuffer = {};
      SoundBuffer.SamplesPerSecond = 44100;
      SoundBuffer.SampleCount = available;
      SoundBuffer.Samples = LinearAudioSamples;

      MacOsState.Game.GameGetSoundSamples(&Context, &GameMemory, &SoundBuffer);

      uint32_t firstChunk = AudioBuffer.Size - AudioBuffer.WriteHead;
      if (available < firstChunk) {
        firstChunk = available;
      }
      memcpy(&AudioBuffer.Memory[AudioBuffer.WriteHead * 2], LinearAudioSamples,
             firstChunk * 2 * sizeof(int16_t));
      memcpy(&AudioBuffer.Memory[0], &LinearAudioSamples[firstChunk * 2],
             (available - firstChunk) * 2 * sizeof(int16_t));
      AudioBuffer.WriteHead =
          (AudioBuffer.WriteHead + available) % AudioBuffer.Size;
#endif
      ClearRenderBuffer(&MacOsState.RenderBuffer, MacOsState.WindowWidth,
                        MacOsState.WindowHeight);
      CurrentInput->DeltaTime = FrameMeasures.DeltaTimeMS / 1000.0f;
      // printf("%f\n", FrameMeasures.DeltaTimeMS / 1000.0f);
      MacOsState.Game.GameUpdateAndRender(&Context, &GameMemory, CurrentInput,
                                          &MacOsState.RenderBuffer);
      if (MacOsState.DebugSoundWave) {
        int padding = 10;
        int skip = 2;
        float barWidthHalf = 0.5;
        int screenWidth =
            MacOsState.RenderBuffer.Viewport.Width - (2 * padding);
        float c1Base = 100;
        float c2Base = 200;
        float height = 128.0;
        render_color_rgba c1Color = render_color_rgba{0.0f, 1.0f, 0.0f, 1.0f};
        render_color_rgba c2Color = render_color_rgba{1.0f, 0.0f, 0.0f, 1.0f};
        for (int s = 0; s < screenWidth; s += skip) {
          size_t d = s * AudioBuffer.Size / screenWidth;
          float c1 = AudioBuffer.Memory[d * 2] / (float)(1 << 15);
          float c2 = AudioBuffer.Memory[d * 2] / (float)(1 << 15);
          float x =
              padding + (float)(d * (MacOsState.RenderBuffer.Viewport.Width -
                                     (2 * padding))) /
                            AudioBuffer.Size;
          PushRect(&MacOsState.RenderBuffer, x - barWidthHalf, c1Base - 1,
                   x + barWidthHalf, c1Base + c1 * height, c1Color);

          PushRect(&MacOsState.RenderBuffer, x - barWidthHalf, c2Base - 1,
                   x + barWidthHalf, c2Base + c2 * height, c2Color);
        }

        {

          float x = padding + (float)(AudioBuffer.ReadHead *
                                      (MacOsState.RenderBuffer.Viewport.Width -
                                       (2 * padding))) /
                                  AudioBuffer.Size;
          PushRect(&MacOsState.RenderBuffer, x, 100 - 10, x + 5, 100 + 100.0,
                   render_color_rgba{0.0f, 0.0f, 1.0f, 1.0f});
        }
        {

          float x = padding + (float)(AudioBuffer.WriteHead *
                                      (MacOsState.RenderBuffer.Viewport.Width -
                                       (2 * padding))) /
                                  AudioBuffer.Size;
          PushRect(&MacOsState.RenderBuffer, x, 100 - 10, x + 5, 100 + 100.0,
                   render_color_rgba{1.0f, 0.0f, 0.0f, 1.0f});
        }
      }
#ifdef USE_METAL
      Vertices.Count = 0;
      MetalPushQuad(&Vertices, -1, -1, 1, 1, 0.5, 0, 0.7, 1, false);
      for (uint32 ri = 0; ri < MacOsState.RenderBuffer.Count; ri += 1) {

        render_command *RCmd = &MacOsState.RenderBuffer.Base[ri];
        switch (RCmd->Type) {
        case RenderCommandRect: {
          // glColor4f(0.0f,0.0f,0.0f,0.0f);

          if (RCmd->Rect.Image) {
            MetalPushQuad(&Vertices, RCmd->Rect.MinX, RCmd->Rect.MinY,
                          RCmd->Rect.MaxX, RCmd->Rect.MaxY,
                          RCmd->Rect.Color.Red, RCmd->Rect.Color.Green,
                          RCmd->Rect.Color.Blue, 0.3f, true);
            if (!texture) {
              // 1. Describe the texture
              MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
              desc.pixelFormat = MTLPixelFormatBGRA8Unorm;
              desc.width = RCmd->Rect.Image->Width;
              desc.height = RCmd->Rect.Image->Height;
              desc.usage = MTLTextureUsageShaderRead;

              // 2. Create texture on the GPU
              texture = [gDevice newTextureWithDescriptor:desc];

              // 3. Upload your bitmap data
              MTLRegion region = {
                  {0, 0, 0},                   // origin
                  {desc.width, desc.height, 1} // size
              };

              NSUInteger bytesPerPixel = 4;
              NSUInteger bytesPerRow = bytesPerPixel * desc.width;

              [texture replaceRegion:region
                         mipmapLevel:0
                           withBytes:RCmd->Rect.Image->Memory
                         bytesPerRow:bytesPerRow];
            }
          } else {
            MetalPushQuad(&Vertices, RCmd->Rect.MinX, RCmd->Rect.MinY,
                          RCmd->Rect.MaxX, RCmd->Rect.MaxY,
                          RCmd->Rect.Color.Red, RCmd->Rect.Color.Green,
                          RCmd->Rect.Color.Blue, RCmd->Rect.Color.Alpha, false);
          }

        } break;
        case RenderCommandTriangle: {
          MetalPushTri(&Vertices, RCmd->Triangle.AX, RCmd->Triangle.AY,
                       RCmd->Triangle.BX, RCmd->Triangle.BY, RCmd->Triangle.CX,
                       RCmd->Triangle.CY, RCmd->Triangle.Color.Red,
                       RCmd->Triangle.Color.Green, RCmd->Triangle.Color.Blue,
                       RCmd->Triangle.Color.Alpha);
        } break;
        default: {
          break;
        } break;
        }
      }
      MetalDraw(&Vertices, 2.0f / MacOsState.WindowWidth,
                -2.0f / MacOsState.WindowHeight, -1.f, 1.f);
#else
      MacOsPaint(&MacOsState.ScreenBuffer, &MacOsState.RenderBuffer);
      MacOsSwapWindowBuffer(MacOsState.Window, &MacOsState.ScreenBuffer);
      if (gLayer) {
        NSView *v = MacOsState.Window.contentView;
        gLayer.frame = v.bounds;
        CGFloat scale = MacOsState.Window.screen.backingScaleFactor ?: 1.0;
        gLayer.contentsScale = scale;
        gLayer.drawableSize = CGSizeMake(v.bounds.size.width * scale,
                                         v.bounds.size.height * scale);
      }
#endif
    }

    uint64_t EndCounter = mach_absolute_time();
    uint64_t DeltaTime = EndCounter - LastCounter;
    uint64_t DeltaTimeNS =
        DeltaTime * GlobalMachTimebase.numer / GlobalMachTimebase.denom;
    uint64_t DeltaTimeMS = DeltaTimeNS / 1e6;
    FrameMeasures.DeltaTimeMS = DeltaTimeMS;
    char printBuffer[256];

    LastCounter = EndCounter;
  }
  printf("Finished!\n");
  return 0;
}
