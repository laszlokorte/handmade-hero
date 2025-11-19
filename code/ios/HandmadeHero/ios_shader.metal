
#include <metal_stdlib>
using namespace metal;

struct Uniforms {
    float scaleX;
    float scaleY;
    float transX;
    float transY;
};

struct Vertex {
    packed_float2 pos;
    packed_float4 col;
    packed_float3 tex;
};

struct VSOut {
  float4 pos [[position]];
  float4 col;
  float3 tex;
};

vertex VSOut v_main(uint vid [[vertex_id]],
                    const device Vertex* verts [[buffer(0)]],
                    constant Uniforms& uni [[buffer(1)]]) {
  VSOut o;
  o.pos = float4(verts[vid].pos * float2(uni.scaleX, uni.scaleY) + float2(uni.transX, uni.transY), 0, 1);
  o.col = verts[vid].col;
  o.tex = verts[vid].tex;
  return o;
}

fragment float4 f_main(VSOut in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler samp [[sampler(0)]]) {
// sample the texture
    float4 texColor = tex.sample(samp, in.tex.xy);
    return mix(in.col, texColor, in.tex.z);
};
