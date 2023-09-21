@group(0) @binding(0) var s : sampler;
@group(0) @binding(1) var t : texture_external;

@fragment fn main(@builtin(position) FragCoord : vec4f)
                         -> @location(0) vec4f {
    return textureSampleBaseClampToEdge(t, s, FragCoord.xy / vec2f(4.0, 4.0));
}
