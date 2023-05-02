#version 310 es

shared uint sh_atomic_failed;
uint tint_workgroupUniformLoad_sh_atomic_failed() {
  barrier();
  uint result = sh_atomic_failed;
  barrier();
  return result;
}

int tint_ftoi(float v) {
  return ((v < 2147483520.0f) ? ((v < -2147483648.0f) ? (-2147483647 - 1) : int(v)) : 2147483647);
}

struct Config {
  uint width_in_tiles;
  uint height_in_tiles;
  uint target_width;
  uint target_height;
  uint base_color;
  uint n_drawobj;
  uint n_path;
  uint n_clip;
  uint bin_data_start;
  uint pathtag_base;
  uint pathdata_base;
  uint drawtag_base;
  uint drawdata_base;
  uint transform_base;
  uint linewidth_base;
  uint binning_size;
  uint tiles_size;
  uint segments_size;
  uint ptcl_size;
  uint pad;
};

struct BumpAllocators {
  uint failed;
  uint binning;
  uint ptcl;
  uint tile;
  uint segments;
  uint blend;
};

struct DrawMonoid {
  uint path_ix;
  uint clip_ix;
  uint scene_offset;
  uint info_offset;
};

struct Path {
  uvec4 bbox;
  uint tiles;
  uint pad_1;
  uint pad_2;
  uint pad_3;
};

struct Tile {
  int backdrop;
  uint segments;
};

layout(binding = 0, std140) uniform config_block_ubo {
  Config inner;
} config;

layout(binding = 1, std430) buffer scene_block_ssbo {
  uint inner[];
} scene;

layout(binding = 2, std430) buffer draw_bboxes_block_ssbo {
  vec4 inner[];
} draw_bboxes;

layout(binding = 3, std430) buffer bump_block_ssbo {
  BumpAllocators inner;
} bump;

layout(binding = 4, std430) buffer paths_block_ssbo {
  Path inner[];
} paths;

layout(binding = 5, std430) buffer tiles_block_ssbo {
  Tile inner[];
} tiles;

shared uint sh_tile_count[256];
void assign_and_preserve_padding_paths_X(uint dest[1], Path value) {
  paths.inner[dest[0]].bbox = value.bbox;
  paths.inner[dest[0]].tiles = value.tiles;
}

void tint_symbol(uvec3 global_id, uvec3 local_id, uint local_invocation_index) {
  if ((local_invocation_index < 1u)) {
    sh_atomic_failed = 0u;
  }
  {
    uint i_1 = local_invocation_index;
    sh_tile_count[i_1] = 0u;
  }
  barrier();
  if ((local_id.x == 0u)) {
    sh_atomic_failed = atomicOr(bump.inner.failed, 0u);
  }
  uint failed = tint_workgroupUniformLoad_sh_atomic_failed();
  if (((failed & 1u) != 0u)) {
    return;
  }
  float SX = 0.0625f;
  float SY = 0.0625f;
  uint drawobj_ix = global_id.x;
  uint drawtag = 0u;
  if ((drawobj_ix < config.inner.n_drawobj)) {
    drawtag = scene.inner[(config.inner.drawtag_base + drawobj_ix)];
  }
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  bool tint_tmp = (drawtag != 0u);
  if (tint_tmp) {
    tint_tmp = (drawtag != 33u);
  }
  if ((tint_tmp)) {
    vec4 bbox = draw_bboxes.inner[drawobj_ix];
    x0 = tint_ftoi(floor((bbox.x * SX)));
    y0 = tint_ftoi(floor((bbox.y * SY)));
    x1 = tint_ftoi(ceil((bbox.z * SX)));
    y1 = tint_ftoi(ceil((bbox.w * SY)));
  }
  uint ux0 = uint(clamp(x0, 0, int(config.inner.width_in_tiles)));
  uint uy0 = uint(clamp(y0, 0, int(config.inner.height_in_tiles)));
  uint ux1 = uint(clamp(x1, 0, int(config.inner.width_in_tiles)));
  uint uy1 = uint(clamp(y1, 0, int(config.inner.height_in_tiles)));
  uint tile_count = ((ux1 - ux0) * (uy1 - uy0));
  uint total_tile_count = tile_count;
  sh_tile_count[local_id.x] = tile_count;
  {
    for(uint i = 0u; (i < 8u); i = (i + 1u)) {
      barrier();
      if ((local_id.x >= (1u << (i & 31u)))) {
        total_tile_count = (total_tile_count + sh_tile_count[(local_id.x - (1u << (i & 31u)))]);
      }
      barrier();
      sh_tile_count[local_id.x] = total_tile_count;
    }
  }
  if ((local_id.x == 255u)) {
    uint count = sh_tile_count[255u];
    uint offset = atomicAdd(bump.inner.tile, count);
    if (((offset + count) > config.inner.tiles_size)) {
      offset = 0u;
      atomicOr(bump.inner.failed, 2u);
    }
    paths.inner[drawobj_ix].tiles = offset;
  }
  { barrier(); memoryBarrierBuffer(); };
  uint tile_offset = paths.inner[(drawobj_ix | 255u)].tiles;
  { barrier(); memoryBarrierBuffer(); };
  if ((drawobj_ix < config.inner.n_drawobj)) {
    uint tile_subix = ((local_id.x > 0u) ? sh_tile_count[(local_id.x - 1u)] : 0u);
    uvec4 bbox = uvec4(ux0, uy0, ux1, uy1);
    Path path = Path(bbox, (tile_offset + tile_subix), 0u, 0u, 0u);
    uint tint_symbol_1[1] = uint[1](drawobj_ix);
    assign_and_preserve_padding_paths_X(tint_symbol_1, path);
  }
  uint total_count = sh_tile_count[255u];
  {
    for(uint i = local_id.x; (i < total_count); i = (i + 256u)) {
      Tile tint_symbol_2 = Tile(0, 0u);
      tiles.inner[(tile_offset + i)] = tint_symbol_2;
    }
  }
}

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
void main() {
  tint_symbol(gl_GlobalInvocationID, gl_LocalInvocationID, gl_LocalInvocationIndex);
  return;
}
