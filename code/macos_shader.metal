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
};

struct VSOut {
  float4 pos [[position]];
  float4 col;
};

vertex VSOut v_main(uint vid [[vertex_id]],
                    const device Vertex* verts [[buffer(0)]],
                    constant Uniforms& uni [[buffer(1)]]) {
  VSOut o;
  o.pos = float4(verts[vid].pos * float2(uni.scaleX, uni.scaleY) + float2(uni.transX, uni.transY), 0, 1);
  o.col = verts[vid].col;
  return o;
}

fragment float4 f_main(VSOut in [[stage_in]]) { return float4(in.col); };
