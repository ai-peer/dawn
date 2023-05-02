groupshared uint sh_atomic_failed;

uint tint_workgroupUniformLoad_sh_atomic_failed() {
  GroupMemoryBarrierWithGroupSync();
  const uint result = sh_atomic_failed;
  GroupMemoryBarrierWithGroupSync();
  return result;
}

int tint_ftoi(float v) {
  return ((v < 2147483520.0f) ? ((v < -2147483648.0f) ? -2147483648 : int(v)) : 2147483647);
}

int tint_clamp(int e, int low, int high) {
  return min(max(e, low), high);
}

struct DrawMonoid {
  uint path_ix;
  uint clip_ix;
  uint scene_offset;
  uint info_offset;
};

DrawMonoid draw_monoid_identity() {
  const DrawMonoid tint_symbol_2 = (DrawMonoid)0;
  return tint_symbol_2;
}

DrawMonoid combine_draw_monoid(DrawMonoid a, DrawMonoid b) {
  DrawMonoid c = (DrawMonoid)0;
  c.path_ix = (a.path_ix + b.path_ix);
  c.clip_ix = (a.clip_ix + b.clip_ix);
  c.scene_offset = (a.scene_offset + b.scene_offset);
  c.info_offset = (a.info_offset + b.info_offset);
  return c;
}

DrawMonoid map_draw_tag(uint tag_word) {
  DrawMonoid c = (DrawMonoid)0;
  c.path_ix = uint((tag_word != 0u));
  c.clip_ix = (tag_word & 1u);
  c.scene_offset = ((tag_word >> 2u) & 7u);
  c.info_offset = ((tag_word >> 6u) & 15u);
  return c;
}

struct Path {
  uint4 bbox;
  uint tiles;
};
struct Tile {
  int backdrop;
  uint segments;
};

cbuffer cbuffer_config : register(b0) {
  uint4 config[5];
};
ByteAddressBuffer scene : register(t1);
ByteAddressBuffer draw_bboxes : register(t2);
RWByteAddressBuffer bump : register(u3);
RWByteAddressBuffer paths : register(u4);
RWByteAddressBuffer tiles : register(u5);

groupshared uint sh_tile_count[256];
groupshared uint sh_tile_offset;

struct tint_symbol_1 {
  uint3 local_id : SV_GroupThreadID;
  uint local_invocation_index : SV_GroupIndex;
  uint3 global_id : SV_DispatchThreadID;
};

uint bumpatomicLoad(uint offset) {
  uint value = 0;
  bump.InterlockedOr(offset, 0, value);
  return value;
}


uint bumpatomicAdd(uint offset, uint value) {
  uint original_value = 0;
  bump.InterlockedAdd(offset, value, original_value);
  return original_value;
}


uint bumpatomicOr(uint offset, uint value) {
  uint original_value = 0;
  bump.InterlockedOr(offset, value, original_value);
  return original_value;
}


void paths_store_1(uint offset, Path value) {
  paths.Store4((offset + 0u), asuint(value.bbox));
  paths.Store((offset + 16u), asuint(value.tiles));
}

void tiles_store(uint offset, Tile value) {
  tiles.Store((offset + 0u), asuint(value.backdrop));
  tiles.Store((offset + 4u), asuint(value.segments));
}

void main_inner(uint3 global_id, uint3 local_id, uint local_invocation_index) {
  if ((local_invocation_index < 1u)) {
    sh_atomic_failed = 0u;
  }
  {
    const uint i_1 = local_invocation_index;
    sh_tile_count[i_1] = 0u;
  }
  GroupMemoryBarrierWithGroupSync();
  if ((local_id.x == 0u)) {
    sh_atomic_failed = bumpatomicLoad(0u);
  }
  const uint failed = tint_workgroupUniformLoad_sh_atomic_failed();
  if (((failed & 1u) != 0u)) {
    return;
  }
  const float SX = 0.0625f;
  const float SY = 0.0625f;
  const uint drawobj_ix = global_id.x;
  uint drawtag = 0u;
  if ((drawobj_ix < config[1].y)) {
    drawtag = scene.Load((4u * (config[2].w + drawobj_ix)));
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
    const float4 bbox = asfloat(draw_bboxes.Load4((16u * drawobj_ix)));
    x0 = tint_ftoi(floor((bbox.x * SX)));
    y0 = tint_ftoi(floor((bbox.y * SY)));
    x1 = tint_ftoi(ceil((bbox.z * SX)));
    y1 = tint_ftoi(ceil((bbox.w * SY)));
  }
  const uint ux0 = uint(tint_clamp(x0, 0, int(config[0].x)));
  const uint uy0 = uint(tint_clamp(y0, 0, int(config[0].y)));
  const uint ux1 = uint(tint_clamp(x1, 0, int(config[0].x)));
  const uint uy1 = uint(tint_clamp(y1, 0, int(config[0].y)));
  const uint tile_count = ((ux1 - ux0) * (uy1 - uy0));
  uint total_tile_count = tile_count;
  sh_tile_count[local_id.x] = tile_count;
  {
    for(uint i = 0u; (i < 8u); i = (i + 1u)) {
      GroupMemoryBarrierWithGroupSync();
      if ((local_id.x >= (1u << (i & 31u)))) {
        total_tile_count = (total_tile_count + sh_tile_count[(local_id.x - (1u << (i & 31u)))]);
      }
      GroupMemoryBarrierWithGroupSync();
      sh_tile_count[local_id.x] = total_tile_count;
    }
  }
  if ((local_id.x == 255u)) {
    const uint count = sh_tile_count[255u];
    uint offset = bumpatomicAdd(12u, count);
    if (((offset + count) > config[4].x)) {
      offset = 0u;
      bumpatomicOr(0u, 2u);
    }
    paths.Store(((32u * drawobj_ix) + 16u), asuint(offset));
  }
  DeviceMemoryBarrierWithGroupSync();
  const uint tile_offset = paths.Load(((32u * (drawobj_ix | 255u)) + 16u));
  DeviceMemoryBarrierWithGroupSync();
  if ((drawobj_ix < config[1].y)) {
    const uint tile_subix = ((local_id.x > 0u) ? sh_tile_count[(local_id.x - 1u)] : 0u);
    const uint4 bbox = uint4(ux0, uy0, ux1, uy1);
    const Path path = {bbox, (tile_offset + tile_subix)};
    paths_store_1((32u * drawobj_ix), path);
  }
  const uint total_count = sh_tile_count[255u];
  {
    for(uint i = local_id.x; (i < total_count); i = (i + 256u)) {
      const Tile tint_symbol_3 = (Tile)0;
      tiles_store((8u * (tile_offset + i)), tint_symbol_3);
    }
  }
}

[numthreads(256, 1, 1)]
void main(tint_symbol_1 tint_symbol) {
  main_inner(tint_symbol.global_id, tint_symbol.local_id, tint_symbol.local_invocation_index);
  return;
}
