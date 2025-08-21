#ifndef RENDERER_H

#include "handmade_types.h"

struct render_viewport {
  uint32 Width;
  uint32 Height;
};

enum render_command_type {
  RenderCommandRect,
};

struct render_command_rect {
  real32 MinX;
  real32 MinY;
  real32 MaxX;
  real32 MaxY;
};

struct render_command {
  render_command_type Type;
  union {
    render_command_rect Rect;
  };
};

struct render_buffer {
  render_viewport Viewport;
  memory_index Size;
  memory_index Count;
  render_command *Base;
};

internal inline void InitializeRenderBuffer(render_buffer *Buffer,
                                            memory_index Size, render_command *Base) {
  Buffer->Viewport = {0,0};
  Buffer->Size = Size;
  Buffer->Count = 0;
  Buffer->Base = Base;
}
internal inline void ClearRenderBuffer(render_buffer* Buffer, uint32 ViewportWidth, uint32 ViewportHeight) {

    Buffer->Count = 0;
    Buffer->Viewport.Width = ViewportWidth;
    Buffer->Viewport.Width = ViewportWidth;

}
internal void PushRect(render_buffer *Buffer, real32 MinX, real32 MinY,
                       real32 MaxX, real32 MaxY);

#define RENDERER_H
#endif
