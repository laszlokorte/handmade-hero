using namespace metal;

struct Vertex {
    packed_float2 pos;
    packed_float3 col;
};

struct VSOut {
  float4 pos [[position]];
  float3 col;
};

vertex VSOut v_main(uint vid [[vertex_id]],
                    const device Vertex* verts [[buffer(0)]]) {
  VSOut o;
  o.pos = float4(verts[vid].pos, 0, 1);
  o.col = verts[vid].col;
  return o;
}

fragment float4 f_main(VSOut in [[stage_in]]) { return float4(in.col, 1); };
