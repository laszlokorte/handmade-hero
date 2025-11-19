#ifndef RENDERER_H

#include "handmade_types.h"

typedef struct loaded_bitmap loaded_bitmap;
typedef struct render_viewport render_viewport;
typedef struct render_color_rgba render_color_rgba;
typedef struct render_command render_command;
typedef struct render_command_rect render_command_rect;
typedef struct render_command_triangle render_command_triangle;
typedef struct render_buffer render_buffer;


struct loaded_bitmap {
  size_t Width;
  size_t Height;
  uint32 *Memory;
};

struct render_viewport {
  uint32 Width;
  uint32 Height;
};

typedef enum render_command_type {
  RenderCommandRect,
  RenderCommandTriangle,
} render_command_type;

struct render_color_rgba {
  real32 Red;
  real32 Green;
  real32 Blue;
  real32 Alpha;
};

struct render_command_rect {
  real32 MinX;
  real32 MinY;
  real32 MaxX;
  real32 MaxY;
  render_color_rgba Color;
  loaded_bitmap *Image;
};

struct render_command_triangle {
  real32 AX;
  real32 AY;
  real32 BX;
  real32 BY;
  real32 CX;
  real32 CY;
  render_color_rgba Color;
};

struct render_command {
  render_command_type Type;
  union {
    render_command_rect Rect;
    render_command_triangle Triangle;
  };
};

struct render_buffer {
  render_viewport Viewport;
  memory_index Size;
  memory_index Count;
  render_command *Base;
};

internal inline void InitializeRenderBuffer(render_buffer *Buffer,
                                            memory_index Size,
                                            render_command *Base) {
  Buffer->Viewport = (render_viewport){0};
  Buffer->Size = Size;
  Buffer->Count = 0;
  Buffer->Base = Base;
}
internal inline void ClearRenderBuffer(render_buffer *Buffer,
                                       uint32 ViewportWidth,
                                       uint32 ViewportHeight) {

  Buffer->Count = 0;
  Buffer->Viewport.Width = ViewportWidth;
  Buffer->Viewport.Height = ViewportHeight;
}
internal void PushTriangle(render_buffer *Buffer, real32 AX, real32 AY,
                           real32 BX, real32 BY, real32 CX, real32 CY,
                           render_color_rgba Color);
internal void PushRect(render_buffer *Buffer, real32 MinX, real32 MinY,
                       real32 MaxX, real32 MaxY, render_color_rgba Color);

#define RENDERER_H
#endif
