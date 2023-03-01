struct GammaTransferParams {
  G : f32,
  A : f32,
  B : f32,
  C : f32,
  D : f32,
  E : f32,
  F : f32,
  padding : u32,
}

struct ExternalTextureParams {
  numPlanes : u32,
  doYuvToRgbConversionOnly : u32,
  yuvToRgbConversionMatrix : mat3x4<f32>,
  gammaDecodeParams : GammaTransferParams,
  gammaEncodeParams : GammaTransferParams,
  gamutConversionMatrix : mat3x3<f32>,
  coordTransformationMatrix : mat3x2<f32>,
}

@group(0) @binding(2) var ext_tex_plane_1 : texture_2d<f32>;

@group(0) @binding(3) var<uniform> ext_tex_params : ExternalTextureParams;

@group(0) @binding(0) var t : texture_2d<f32>;

@group(0) @binding(1) var outImage : texture_storage_2d<rgba8unorm, write>;

fn gammaCorrection(v : vec3<f32>, params : GammaTransferParams) -> vec3<f32> {
  let cond = (abs(v) < vec3<f32>(params.D));
  let t = (sign(v) * ((params.C * abs(v)) + params.F));
  let f = (sign(v) * (pow(((params.A * abs(v)) + params.B), vec3<f32>(params.G)) + params.E));
  return select(f, t, cond);
}

fn textureLoadExternal(plane0 : texture_2d<f32>, plane1 : texture_2d<f32>, coord : vec2<i32>, params : ExternalTextureParams) -> vec4<f32> {
  let coord1 = (coord >> vec2<u32>(1));
  var color : vec3<f32>;
  if ((params.numPlanes == 1)) {
    color = textureLoad(plane0, coord, 0).rgb;
  } else {
    color = (vec4<f32>(textureLoad(plane0, coord, 0).r, textureLoad(plane1, coord1, 0).rg, 1) * params.yuvToRgbConversionMatrix);
  }
  if ((params.doYuvToRgbConversionOnly == 0)) {
    color = gammaCorrection(color, params.gammaDecodeParams);
    color = (params.gamutConversionMatrix * color);
    color = gammaCorrection(color, params.gammaEncodeParams);
  }
  return vec4<f32>(color, 1);
}

@compute @workgroup_size(1)
fn main() {
  let coords = vec2<i32>(10, 10);
  var texture_load : vec4<f32>;
  if (all((vec2<u32>(coords) < textureDimensions(t)))) {
    texture_load = textureLoadExternal(t, ext_tex_plane_1, coords, ext_tex_params);
  }
  var red : vec4<f32> = texture_load;
  let coords_1 = vec2<i32>(0, 0);
  if (all((vec2<u32>(coords_1) < textureDimensions(outImage)))) {
    textureStore(outImage, coords_1, red);
  }
  let coords_2 = vec2<i32>(70, 118);
  var texture_load_1 : vec4<f32>;
  if (all((vec2<u32>(coords_2) < textureDimensions(t)))) {
    texture_load_1 = textureLoadExternal(t, ext_tex_plane_1, coords_2, ext_tex_params);
  }
  var green : vec4<f32> = texture_load_1;
  let coords_3 = vec2<i32>(1, 0);
  if (all((vec2<u32>(coords_3) < textureDimensions(outImage)))) {
    textureStore(outImage, coords_3, green);
  }
  return;
}
