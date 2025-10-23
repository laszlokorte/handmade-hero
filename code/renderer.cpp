#include "renderer.h"

internal render_command *RenderPushCommand(render_buffer *Buffer) {
  Assert(Buffer->Count < Buffer->Size);
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
  render_command_rect FreshRect = {};
  render_command_rect *Rect = &Cmd->Rect;
  *Rect = FreshRect;
  Rect->MinX = MinX;
  Rect->MinY = MinY;
  Rect->MaxX = MaxX;
  Rect->MaxY = MaxY;
  Rect->Color = Color;
  Rect->Image = 0;
}
internal void PushRectImage(render_buffer *Buffer, real32 MinX, real32 MinY,
                            real32 MaxX, real32 MaxY, loaded_bitmap *Image) {
  render_command *Cmd = RenderPushCommand(Buffer);
  Cmd->Type = RenderCommandRect;
  render_command_rect FreshRect = {};
  render_command_rect *Rect = &Cmd->Rect;
  *Rect = FreshRect;
  Rect->MinX = MinX;
  Rect->MinY = MinY;
  Rect->MaxX = MaxX;
  Rect->MaxY = MaxY;
  Rect->Image = Image;
}

internal void PushTriangle(render_buffer *Buffer, real32 AX, real32 AY,
                           real32 BX, real32 BY, real32 CX, real32 CY,
                           render_color_rgba Color) {
  render_command *Cmd = RenderPushCommand(Buffer);
  Cmd->Type = RenderCommandTriangle;
  render_command_triangle *Triangle = &Cmd->Triangle;
  Triangle->AX = AX;
  Triangle->AY = AY;
  Triangle->BX = BX;
  Triangle->BY = BY;
  Triangle->CX = CX;
  Triangle->CY = CY;
  Triangle->Color = Color;
}
