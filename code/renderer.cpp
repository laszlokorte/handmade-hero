#include "renderer.h"

internal render_command *RenderPushCommand(render_buffer *Buffer) {
  render_command NewCommand = {};
  render_command *Result = &Buffer->Base[Buffer->Count];
  *Result = NewCommand;
  Buffer->Count += 1;

  return Result;
}

internal void PushRect(render_buffer *Buffer, real32 MinX, real32 MinY,
                       real32 MaxX, real32 MaxY, render_color_rgba Color) {
  render_command *Cmd = RenderPushCommand(Buffer);
  Cmd->Type = RenderCommandRect;
  render_command_rect *Rect = &Cmd->Rect;
  Rect->MinX = MinX;
  Rect->MinY = MinY;
  Rect->MaxX = MaxX;
  Rect->MaxY = MaxY;
  Rect->Color = Color;
}
