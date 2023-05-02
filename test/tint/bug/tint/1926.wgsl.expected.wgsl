struct Config {
  width_in_tiles : u32,
  height_in_tiles : u32,
  target_width : u32,
  target_height : u32,
  base_color : u32,
  n_drawobj : u32,
  n_path : u32,
  n_clip : u32,
  bin_data_start : u32,
  pathtag_base : u32,
  pathdata_base : u32,
  drawtag_base : u32,
  drawdata_base : u32,
  transform_base : u32,
  linewidth_base : u32,
  binning_size : u32,
  tiles_size : u32,
  segments_size : u32,
  ptcl_size : u32,
}

const TILE_WIDTH = 16u;

const TILE_HEIGHT = 16u;

const N_TILE_X = 16u;

const N_TILE_Y = 16u;

const N_TILE = 256u;

const BLEND_STACK_SPLIT = 4u;

const STAGE_BINNING : u32 = 1u;

const STAGE_TILE_ALLOC : u32 = 2u;

const STAGE_PATH_COARSE : u32 = 4u;

const STAGE_COARSE : u32 = 8u;

struct BumpAllocators {
  failed : atomic<u32>,
  binning : atomic<u32>,
  ptcl : atomic<u32>,
  tile : atomic<u32>,
  segments : atomic<u32>,
  blend : atomic<u32>,
}

struct DrawMonoid {
  path_ix : u32,
  clip_ix : u32,
  scene_offset : u32,
  info_offset : u32,
}

const DRAWTAG_NOP = 0u;

const DRAWTAG_FILL_COLOR = 68u;

const DRAWTAG_FILL_LIN_GRADIENT = 276u;

const DRAWTAG_FILL_RAD_GRADIENT = 732u;

const DRAWTAG_FILL_IMAGE = 584u;

const DRAWTAG_BEGIN_CLIP = 9u;

const DRAWTAG_END_CLIP = 33u;

fn draw_monoid_identity() -> DrawMonoid {
  return DrawMonoid();
}

fn combine_draw_monoid(a : DrawMonoid, b : DrawMonoid) -> DrawMonoid {
  var c : DrawMonoid;
  c.path_ix = (a.path_ix + b.path_ix);
  c.clip_ix = (a.clip_ix + b.clip_ix);
  c.scene_offset = (a.scene_offset + b.scene_offset);
  c.info_offset = (a.info_offset + b.info_offset);
  return c;
}

fn map_draw_tag(tag_word : u32) -> DrawMonoid {
  var c : DrawMonoid;
  c.path_ix = u32((tag_word != DRAWTAG_NOP));
  c.clip_ix = (tag_word & 1u);
  c.scene_offset = ((tag_word >> 2u) & 7u);
  c.info_offset = ((tag_word >> 6u) & 15u);
  return c;
}

struct Path {
  bbox : vec4<u32>,
  tiles : u32,
}

struct Tile {
  backdrop : i32,
  segments : u32,
}

@group(0) @binding(0) var<uniform> config : Config;

@group(0) @binding(1) var<storage> scene : array<u32>;

@group(0) @binding(2) var<storage> draw_bboxes : array<vec4<f32>>;

@group(0) @binding(3) var<storage, read_write> bump : BumpAllocators;

@group(0) @binding(4) var<storage, read_write> paths : array<Path>;

@group(0) @binding(5) var<storage, read_write> tiles : array<Tile>;

const WG_SIZE = 256u;

var<workgroup> sh_tile_count : array<u32, WG_SIZE>;

var<workgroup> sh_tile_offset : u32;

var<workgroup> sh_atomic_failed : u32;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) global_id : vec3<u32>, @builtin(local_invocation_id) local_id : vec3<u32>) {
  if ((local_id.x == 0u)) {
    sh_atomic_failed = atomicLoad(&(bump.failed));
  }
  let failed = workgroupUniformLoad(&(sh_atomic_failed));
  if (((failed & STAGE_BINNING) != 0u)) {
    return;
  }
  let SX = (1.0 / f32(TILE_WIDTH));
  let SY = (1.0 / f32(TILE_HEIGHT));
  let drawobj_ix = global_id.x;
  var drawtag = DRAWTAG_NOP;
  if ((drawobj_ix < config.n_drawobj)) {
    drawtag = scene[(config.drawtag_base + drawobj_ix)];
  }
  var x0 = 0;
  var y0 = 0;
  var x1 = 0;
  var y1 = 0;
  if (((drawtag != DRAWTAG_NOP) && (drawtag != DRAWTAG_END_CLIP))) {
    let bbox = draw_bboxes[drawobj_ix];
    x0 = i32(floor((bbox.x * SX)));
    y0 = i32(floor((bbox.y * SY)));
    x1 = i32(ceil((bbox.z * SX)));
    y1 = i32(ceil((bbox.w * SY)));
  }
  let ux0 = u32(clamp(x0, 0, i32(config.width_in_tiles)));
  let uy0 = u32(clamp(y0, 0, i32(config.height_in_tiles)));
  let ux1 = u32(clamp(x1, 0, i32(config.width_in_tiles)));
  let uy1 = u32(clamp(y1, 0, i32(config.height_in_tiles)));
  let tile_count = ((ux1 - ux0) * (uy1 - uy0));
  var total_tile_count = tile_count;
  sh_tile_count[local_id.x] = tile_count;
  for(var i = 0u; (i < firstTrailingBit(WG_SIZE)); i += 1u) {
    workgroupBarrier();
    if ((local_id.x >= (1u << i))) {
      total_tile_count += sh_tile_count[(local_id.x - (1u << i))];
    }
    workgroupBarrier();
    sh_tile_count[local_id.x] = total_tile_count;
  }
  if ((local_id.x == (WG_SIZE - 1u))) {
    let count = sh_tile_count[(WG_SIZE - 1u)];
    var offset = atomicAdd(&(bump.tile), count);
    if (((offset + count) > config.tiles_size)) {
      offset = 0u;
      atomicOr(&(bump.failed), STAGE_TILE_ALLOC);
    }
    paths[drawobj_ix].tiles = offset;
  }
  storageBarrier();
  let tile_offset = paths[(drawobj_ix | (WG_SIZE - 1u))].tiles;
  storageBarrier();
  if ((drawobj_ix < config.n_drawobj)) {
    let tile_subix = select(0u, sh_tile_count[(local_id.x - 1u)], (local_id.x > 0u));
    let bbox = vec4(ux0, uy0, ux1, uy1);
    let path = Path(bbox, (tile_offset + tile_subix));
    paths[drawobj_ix] = path;
  }
  let total_count = sh_tile_count[(WG_SIZE - 1u)];
  for(var i = local_id.x; (i < total_count); i += WG_SIZE) {
    tiles[(tile_offset + i)] = Tile(0, 0u);
  }
}
