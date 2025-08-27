
#include <AppKit/AppKit.h>
#include <stdio.h>
#include <dlfcn.h>
#include "handmade.h"

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
  MacOsResizeScreenBuffer(&_State->ScreenBuffer,
                          Window.contentView.frame.size.width,
                          Window.contentView.frame.size.height);

  MacOsPaint(&_State->ScreenBuffer);
  MacOsSwapWindowBuffer(_State->Window, &_State->ScreenBuffer);
}

- (void)windowWillStartLiveResize:(NSNotification *)notification {
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
}
@end

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
                          NSWindowStyleMaskResizable |NSWindowStyleMaskFullSizeContentView
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

  while (MacOsState.Running) {
    @autoreleasepool {
      NSEvent *Event;
      NSBitmapImageRep *newRep = [[[NSBitmapImageRep alloc]
          initWithBitmapDataPlanes:(uint8_t **)&MacOsState.ScreenBuffer.Memory
                        pixelsWide:MacOsState.ScreenBuffer.Width
                        pixelsHigh:MacOsState.ScreenBuffer.Height
                     bitsPerSample:8
                   samplesPerPixel:MacOsState.ScreenBuffer.BytesPerPixel
                          hasAlpha:YES
                          isPlanar:NO
                    colorSpaceName:NSDeviceRGBColorSpace
                       bytesPerRow:MacOsState.ScreenBuffer.Pitch
                      bitsPerPixel:32] autorelease];
      NSImage *image = [[[NSImage alloc]
          initWithSize:NSMakeSize(MacOsState.ScreenBuffer.Width,
                                  MacOsState.ScreenBuffer.Height)] autorelease];
      [image addRepresentation:newRep];
      MacOsState.Window.contentView.wantsLayer = YES;
      MacOsState.Window.contentView.layer.contents = image;
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
        MacOsPaint(&MacOsState.ScreenBuffer);
        MacOsSwapWindowBuffer(MacOsState.Window, &MacOsState.ScreenBuffer);
      } while (Event != nil);
    }
  }
  printf("Finisehd!\n");
  return 0;
}
