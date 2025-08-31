
#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include <dlfcn.h>
#include "handmade.h"
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <mach-o/dyld.h>
#include <Carbon/Carbon.h>

#define USE_METAL
// #define USE_AUDIO

static id<MTLDevice> gDevice;
static id<MTLCommandQueue> gQueue;
static id<MTLLibrary> gLib;
static id<MTLRenderPipelineState> gPSO;
static CAMetalLayer *gLayer;
static id<MTLBuffer> gVtx;

DEBUG_PLATFORM_FREE_FILE_MEMORY(PlatformFreeFileNoop) {}
DEBUG_PLATFORM_READ_ENTIRE_FILE(PlatformReadEntireFileNoop) {
  debug_read_file_result Result = {};

  return Result;
}
DEBUG_PLATFORM_WRITE_ENTIRE_FILE(PlatformWriteEntireFileNoop) { return false; }


PUSH_TASK_TO_QUEUE(PushTaskToQueueNoop) {

}

WAIT_FOR_QUEUE_TO_FINISH(WaitForQueueToFinishNoop) {

}

// Tiny shaders as source string (no .metal file needed)
typedef struct {
  float pos[2];
  float col[4];
} MetalVertex;

struct MetalVertices {
  size_t Capacity;
  size_t Count;
  MetalVertex *Buffer;
};

static void CreateMetal(NSView *view, CGSize size) {
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

  gVtx = [gDevice newBufferWithLength:sizeof(MetalVertex) * 1000
                              options:MTLResourceStorageModeManaged];
}

static void MetalPushQuad(MetalVertices *Vertices, float x0, float y0, float x1,
                          float y1, float r, float g, float b, float a) {
  if (Vertices->Capacity < Vertices->Count + 6) {
    return;
  }
  Vertices->Buffer[(Vertices->Count)++] = {{x0, y0}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x1, y0}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x0, y1}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x1, y0}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x1, y1}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x0, y1}, {r, g, b, a}};
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
    [enc drawPrimitives:MTLPrimitiveTypeTriangle
            vertexStart:0
            vertexCount:Vertices->Count];
    [enc endEncoding];
    [cb presentDrawable:drawable];
    [cb commit];
  }
}

struct macos_screen_buffer {
  uint32_t Width;
  uint32_t Height;
  uint32_t Pitch;
  uint32_t BytesPerPixel;
  void *Memory;
};

struct macos_game {
  bool IsValid;
  void *GameDLL;
  game_update_and_render *GameUpdateAndRender;
  game_get_sound_samples *GameGetSoundSamples;
};

struct macos_work_queue_task {
  work_queue_callback *Callback;
  void *Data;
};

struct work_queue {
  size_t Size;
  macos_work_queue_task *Base;

  uint32 volatile NextWrite;
  uint32 volatile NextRead;

  size_t volatile CompletionGoal;
  size_t volatile CompletionCount;

  void *SemaphoreHandle;
};

struct macos_thread_info {
  int32 LogicalThreadIndex;
  uint32 ThreadId;
  work_queue *Queue;
};

struct macos_thread_pool {
  size_t Count;
  macos_thread_info *Threads;
};

struct macos_state {
  bool Running;
  float WindowWidth;
  float WindowHeight;

  NSWindow *Window;
  macos_screen_buffer ScreenBuffer;
  macos_game Game;

  size_t TotalMemorySize;
  void *GameMemoryBlock;
  render_buffer RenderBuffer;
  work_queue WorkQueue;
  macos_thread_pool ThreadPool;
};

void MacOsLoadGame(macos_game *Game) {
  char exePath[PATH_MAX];
  uint32_t exeSize = sizeof(exePath);
  _NSGetExecutablePath(exePath, &exeSize); // gives path inside .app or binary
  NSString *exeDir = [[NSString stringWithUTF8String:exePath]
      stringByDeletingLastPathComponent];
  NSString *dllPath =
      [exeDir stringByAppendingPathComponent:@"./handmade_game"];
  Game->GameDLL = dlopen([dllPath UTF8String], RTLD_NOW);
  if (Game->GameDLL) {
    Game->GameUpdateAndRender =
        (game_update_and_render *)dlsym(Game->GameDLL, "GameUpdateAndRender");
    Game->GameGetSoundSamples =
        (game_get_sound_samples *)dlsym(Game->GameDLL, "GameGetSoundSamples");
    Game->IsValid = true;
  }
}

void MacOsSetupGameMemory(macos_state *MacState, game_memory *GameMemory) {
  uint32 RenderBufferLength = 10000;
  memory_index RenderBufferSize = RenderBufferLength * sizeof(render_command);
  uint32 WorkQueueLength = 128;
  size_t WorkQueueSize = WorkQueueLength * sizeof(macos_work_queue_task);
  GameMemory->Initialized = false;
  GameMemory->PermanentStorageSize = Megabytes(10);
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
  // InitializeWorkQueue(
  //     &MacState->WorkQueue, WorkQueueLength,
  //     (win32_work_queue_task *)(GameMemory->PermanentStorage +
  //                               GameMemory->PermanentStorageSize +
  //                               GameMemory->TransientStorageSize +
  //                               RenderBufferSize));

  GameMemory->TaskQueue = &MacState->WorkQueue;
  GameMemory->PlatformPushTaskToQueue = PushTaskToQueueNoop; //&PushTaskToQueue;
  GameMemory->PlatformWaitForQueueToFinish =
      WaitForQueueToFinishNoop; //&WaitForQueueToFinish;
  GameMemory->DebugPlatformReadEntireFile =
      PlatformReadEntireFileNoop; //&DEBUGPlatformReadEntireFile;
  GameMemory->DebugPlatformFreeFileMemory =
      PlatformFreeFileNoop; //&DEBUGPlatformFreeFileMemory;
  GameMemory->DebugPlatformWriteEntireFile =
      PlatformWriteEntireFileNoop; //&DEBUGPlatformWriteEntireFile;
}

void MacOsPaint(macos_screen_buffer *ScreenBuffer) {
  uint8_t *Row = (uint8_t *)ScreenBuffer->Memory;
  for (int y = 0; y < ScreenBuffer->Height; y++) {
    uint8_t *Column = Row;
    for (int x = 0; x < ScreenBuffer->Width; x++) {
      uint32_t *Pixel = (uint32_t *)Column;
      (*Pixel) =
          (x / 100 % 2 == 0) ^ (y / 100 % 2 == 0) ? 0xffffeebb : 0xffbbeeff;
      Column += ScreenBuffer->BytesPerPixel;
    }
    Row += ScreenBuffer->Pitch;
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

  MacOsPaint(&_State->ScreenBuffer);
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

struct mac_audio_buffer {
  size_t Size;
  int16_t *Memory;

  uint32 ReadHead;
  uint32 WriteHead;
};

OSStatus MacAudioCallback(void *inRefCon,
                          AudioUnitRenderActionFlags *ioActionFlags,
                          const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                          UInt32 inNumberFrames, AudioBufferList *ioData) {
  mac_audio_buffer *data = (mac_audio_buffer *)inRefCon;

  data->WriteHead = (data->WriteHead + inNumberFrames) % data->Size;
  Float32 *out = (Float32 *)ioData->mBuffers[0].mData;
  uint16 *source = (uint16_t *)data->Memory;
  for (UInt32 i = 0; i < inNumberFrames; i++) {
    // printf("%f\n", source[2 * i]/ 655360.0f);
    out[i * 2 + 0] =
        source[(2 * (data->ReadHead + i)) % data->Size] / (float)(1 << 24);
    out[i * 2 + 1] =
        source[(2 * (data->ReadHead + i) + 1) % data->Size] / (float)(1 << 24);
  }
  data->ReadHead = (data->ReadHead + inNumberFrames) % data->Size;
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

int main(void) {
  macos_state MacOsState = {};
  MacOsState.Running = true;
  MacOsState.WindowWidth = 1200;
  MacOsState.WindowHeight = 800;
  MacOsState.ScreenBuffer.BytesPerPixel = 4;
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
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable
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
  AudioBuffer.Size = f * 2;
  AudioBuffer.Memory =
      (int16_t *)mmap(NULL, AudioBuffer.Size * sizeof(int16_t),
                      PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  int note = 1;
  for (int i = 0; i < f; i++) { // i = frame index
    float sample =
        6000 * sinf(440 * pow(2, note * 1.0 / 12.0) * Pi32 * i / (float)f);
    AudioBuffer.Memory[2 * i + 0] = (int16_t)sample; // left
    AudioBuffer.Memory[2 * i + 1] = (int16_t)sample; // right
  }
#ifdef USE_AUDIO
  MacInitAudio(&AudioBuffer);
#endif
#ifdef USE_METAL
  CreateMetal(MacOsState.Window.contentView, initialFrame.size);
#endif
  game_input GameInputs[2] = {};
  game_input *LastInput = &GameInputs[0];
  game_input *CurrentInput = &GameInputs[1];
  game_memory GameMemory = {};
  MacOsSetupGameMemory(&MacOsState, &GameMemory);
  while (MacOsState.Running) {
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
              [NSApp terminate:nil]; // or custom handling
              MacOsState.Running = false;
            }
            NSString *chars = [Event charactersIgnoringModifiers];
            if (chars.length > 0 &&
                [chars characterAtIndex:0] == 0x1B) { // 0x1B = ESC
              MacOsState.Running = false;
            }
          }
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
      NSRect content = MacOsState.Window.frame;
      if (NSPointInRect([NSEvent mouseLocation], content)) {
        CurrentInput->Mouse.MouseX = (int)posInWindow.x;
        CurrentInput->Mouse.MouseY =
            (int)(MacOsState.WindowHeight - posInWindow.y);
      }
      for (int m = 0; m < ArrayCount(CurrentInput->Mouse.Buttons); m++) {
        bool NewDown = NSEvent.pressedMouseButtons & (1 << m);
        bool toggled = CurrentInput->Mouse.Buttons[m].EndedDown != NewDown;
        CurrentInput->Mouse.Buttons[m].HalfTransitionCount += (NewDown ? 1 : 0);
        CurrentInput->Mouse.Buttons[m].EndedDown = NewDown;
      }
      thread_context Context = {};
      game_sound_output_buffer SoundBuffer = {};

      CurrentInput->DeltaTime = 0.016;
      MacOsState.Game.GameGetSoundSamples(&Context, &GameMemory, &SoundBuffer);
      ClearRenderBuffer(&MacOsState.RenderBuffer, MacOsState.WindowWidth,
                        MacOsState.WindowHeight);
      MacOsState.Game.GameUpdateAndRender(&Context, &GameMemory, CurrentInput,
                                          &MacOsState.RenderBuffer);

#ifdef USE_METAL
      MetalVertices Vertices = {};
      Vertices.Capacity = 6000;
      Vertices.Buffer = (MetalVertex *)mmap(
          NULL, Vertices.Capacity * sizeof(*Vertices.Buffer),
          PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
      MetalPushQuad(&Vertices, -1, -1, 1, 1, 0.5, 0, 0.7, 1);
      for (uint32 ri = 0; ri < MacOsState.RenderBuffer.Count; ri += 1) {

        render_command *RCmd = &MacOsState.RenderBuffer.Base[ri];
        switch (RCmd->Type) {
        case RenderCommandRect: {
          // glColor4f(0.0f,0.0f,0.0f,0.0f);
          if (RCmd->Rect.Image) {
            MetalPushQuad(&Vertices, RCmd->Rect.MinX, RCmd->Rect.MinY,
                          RCmd->Rect.MaxX, RCmd->Rect.MaxY,
                          RCmd->Rect.Color.Red, RCmd->Rect.Color.Green,
                          RCmd->Rect.Color.Blue, 0.3f);
          } else {
            MetalPushQuad(&Vertices, RCmd->Rect.MinX, RCmd->Rect.MinY,
                          RCmd->Rect.MaxX, RCmd->Rect.MaxY,
                          RCmd->Rect.Color.Red, RCmd->Rect.Color.Green,
                          RCmd->Rect.Color.Blue, 1.0f);
            // glColor4f(RCmd->Rect.Color.Red, RCmd->Rect.Color.Green,
            //           RCmd->Rect.Color.Blue, RCmd->Rect.Color.Alpha);
          }

          // glBegin(GL_TRIANGLES);
          // glTexCoord2f(0.0f, 1.0f);
          // glVertex2f(RCmd->Rect.MinX, RCmd->Rect.MinY);

          // glTexCoord2f(1.0f, 1.0f);
          // glVertex2f(RCmd->Rect.MaxX, RCmd->Rect.MinY);

          // glTexCoord2f(1.0f, 0.0f);
          // glVertex2f(RCmd->Rect.MaxX, RCmd->Rect.MaxY);

          // glTexCoord2f(1.0f, 0.0f);
          // glVertex2f(RCmd->Rect.MaxX, RCmd->Rect.MaxY);

          // glTexCoord2f(0.0f, 0.0f);
          // glVertex2f(RCmd->Rect.MinX, RCmd->Rect.MaxY);

          // glTexCoord2f(0.0f, 1.0f);
          // glVertex2f(RCmd->Rect.MinX, RCmd->Rect.MinY);
          // glEnd();
          // glDisable(GL_TEXTURE_2D);
        } break;
        case RenderCommandTriangle: {
          // glColor4f(RCmd->Triangle.Color.Red, RCmd->Triangle.Color.Green,
          //           RCmd->Triangle.Color.Blue, RCmd->Triangle.Color.Alpha);

          // glBegin(GL_TRIANGLES);
          // glVertex2f(RCmd->Triangle.AX, RCmd->Triangle.AY);

          // glVertex2f(RCmd->Triangle.BX, RCmd->Triangle.BY);

          // glVertex2f(RCmd->Triangle.CX, RCmd->Triangle.CY);
          // glEnd();
          // glDisable(GL_TEXTURE_2D);
        } break;
        default: {
          break;
        } break;
        }
      }
      MetalDraw(&Vertices, 2.0f / MacOsState.WindowWidth,
                -2.0f / MacOsState.WindowHeight, -1.f, 1.f);
#else
      MacOsPaint(&MacOsState.ScreenBuffer);
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
  }
  printf("Finished!\n");
  return 0;
}
