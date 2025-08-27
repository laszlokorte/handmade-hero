
#include <AppKit/AppKit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <stdio.h>
#include <dlfcn.h>
#include "handmade.h"
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <mach-o/dyld.h>

#define USE_METAL

static id<MTLDevice> gDevice;
static id<MTLCommandQueue> gQueue;
static id<MTLLibrary> gLib;
static id<MTLRenderPipelineState> gPSO;
static CAMetalLayer *gLayer;
static id<MTLBuffer> gVtx;

// Tiny shaders as source string (no .metal file needed)

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

  gPSO = [gDevice newRenderPipelineStateWithDescriptor:p error:&err];
  if (!gPSO) {
    NSLog(@"PSO failed: %@", err);
    exit(1);
  }

  // Triangle in clip space (x,y)
  float verts[12] = {1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
                    1.0f, 1.0f, -1.0f, -1.0f, -1.0f, 1.0f};
  gVtx = [gDevice newBufferWithBytes:verts
                              length:sizeof(verts)
                             options:MTLResourceStorageModeManaged];
}

static void DrawFrame() {
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

    id<MTLCommandBuffer> cb = [gQueue commandBuffer];
    id<MTLRenderCommandEncoder> enc =
        [cb renderCommandEncoderWithDescriptor:rp];
    [enc setRenderPipelineState:gPSO];
    [enc setVertexBuffer:gVtx offset:0 atIndex:0];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
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

struct macos_state {
  bool Running;
  float WindowWidth;
  float WindowHeight;

  NSWindow *Window;
  macos_screen_buffer ScreenBuffer;
  macos_game Game;
};

void MacOsLoadGame(macos_game *Game) {
  Game->GameDLL = dlopen("handmade_game", RTLD_NOW);
  if (Game->GameDLL) {
    Game->GameUpdateAndRender =
        (game_update_and_render *)dlsym(Game->GameDLL, "GameUpdateAndRender");
    Game->GameGetSoundSamples =
        (game_get_sound_samples *)dlsym(Game->GameDLL, "GameGetSoundSamples");
    Game->IsValid = true;
  }
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
  // AudioData *data = (AudioData*)inRefCon;

  Float32 *out = (Float32 *)ioData->mBuffers[0].mData;
  for (UInt32 i = 0; i < inNumberFrames; i++) {
    out[i * 2 + 0] = 0.2 * sinf(0.02 * 3.141 * (inTimeStamp->mSampleTime + i));
    out[i * 2 + 1] = 0.2 * sinf(0.02 * 3.141 * (inTimeStamp->mSampleTime + i));
  }
  return noErr;
}

void MacInitAudio() {
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
  cb.inputProcRefCon = NULL;
  AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                       kAudioUnitScope_Input, 0, &cb, sizeof(cb));

  AudioUnitInitialize(audioUnit);
  AudioOutputUnitStart(audioUnit);
}

int main(void) {
  macos_state MacOsState = {};
  MacOsState.Running = true;
  MacOsState.WindowWidth = 1200;
  MacOsState.WindowHeight = 800;
  MacOsState.ScreenBuffer.BytesPerPixel = 4;
  MacOsLoadGame(&MacOsState.Game);
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

  MacOsResizeScreenBuffer(&MacOsState.ScreenBuffer, MacOsState.WindowWidth,
                          MacOsState.WindowHeight);

  [MacOsState.Window setBackgroundColor:NSColor.darkGrayColor];
  [MacOsState.Window setTitle:@"Handmade hero"];
  [MacOsState.Window setDelegate:mainWindowDelegate];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp activateIgnoringOtherApps:true];
  [MacOsState.Window makeKeyAndOrderFront:nil];
  MacOsState.Window.contentView.wantsLayer = YES;

  MacInitAudio();
#ifdef USE_METAL
  CreateMetal(MacOsState.Window.contentView, initialFrame.size);
#endif

  while (MacOsState.Running) {
    @autoreleasepool {
      NSEvent *Event;

      do {

        Event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                   untilDate:nil
                                      inMode:NSDefaultRunLoopMode
                                     dequeue:YES];

        if (!Event) {
          break;
        };

        switch ([Event type]) {
        case NSEventTypeKeyDown: {

          if ((Event.modifierFlags & NSEventModifierFlagCommand) &&
              [[Event charactersIgnoringModifiers] isEqualToString:@"q"]) {
            [NSApp terminate:nil]; // or custom handling
            MacOsState.Running = false;
          }
          NSString *chars = [Event charactersIgnoringModifiers];
          if (chars.length > 0 &&
              [chars characterAtIndex:0] == 0x1B) { // 0x1B = ESC
            MacOsState.Running = false;
          }
        } break;
        default:
          [NSApp sendEvent:Event];
        }
#ifdef USE_METAL

        DrawFrame();
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

      } while (Event != nil);
    }
  }
  printf("Finished!\n");
  return 0;
}
