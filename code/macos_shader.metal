using namespace metal;

struct VSOut {
  float4 pos [[position]];
  float3 col;
};

vertex VSOut v_main(uint vid [[vertex_id]],
                    const device float2 *pos [[buffer(0)]]) {
  float3 cols[6] = {float3(1, 0, 0), float3(0, 1, 0), float3(0, 0, 1), float3(1, 0, 0), float3(0, 1, 0), float3(1, 0, 1), };
  VSOut o;
  o.pos = float4(pos[vid], 0, 1);
  o.col = cols[vid];
  return o;
}

fragment float4 f_main(VSOut in [[stage_in]]) { return float4(in.col, 1); };
