#pragma once

// Native Metal for the initial macOS target. Introduce sokol-shdc when adding
// other graphics backends so shader source and reflection stay in sync.
static const char *cube_shader_source =
    "#include <metal_stdlib>\n"
    "using namespace metal;\n"
    "struct Input { float3 position [[attribute(0)]]; float3 color [[attribute(1)]]; };\n"
    "struct Output { float4 position [[position]]; float3 color; };\n"
    "struct Params { float angle; float aspect; float2 padding; };\n"
    "vertex Output cube_vs(Input in [[stage_in]], constant Params& u [[buffer(0)]]) {\n"
    "  float cy = cos(u.angle), sy = sin(u.angle);\n"
    "  float cx = cos(u.angle * 0.6), sx = sin(u.angle * 0.6);\n"
    "  float3 p = in.position;\n"
    "  p = float3(cy*p.x + sy*p.z, p.y, -sy*p.x + cy*p.z);\n"
    "  p = float3(p.x, cx*p.y - sx*p.z, sx*p.y + cx*p.z);\n"
    "  p.z -= 5.0;\n"
    // Right-handed perspective, Metal depth range [0, 1], near .1, far 100.
    "  Output out;\n"
    "  out.position = float4(1.7320508*p.x/u.aspect, 1.7320508*p.y,\n"
    "                        (-100.0/99.9)*p.z - 10.0/99.9, -p.z);\n"
    "  out.color = in.color;\n"
    "  return out;\n"
    "}\n"
    "fragment float4 cube_fs(Output in [[stage_in]]) { return float4(in.color, 1.0); }\n";
