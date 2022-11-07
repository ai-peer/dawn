statements/discard/atomic_in_for_loop_continuing.wgsl:14:19 warning: 'textureSample' must only be called from uniform control flow
    result += i32(textureSample(t, s, coord).x);
                  ^^^^^^^^^^^^^

statements/discard/atomic_in_for_loop_continuing.wgsl:13:3 note: control flow depends on non-uniform value
  for (var i = 0; i < 10; i = atomicAdd(&a, 1)) {
  ^^^

statements/discard/atomic_in_for_loop_continuing.wgsl:13:42 note: reading from read_write storage buffer 'a' may result in a non-uniform value
  for (var i = 0; i < 10; i = atomicAdd(&a, 1)) {
                                         ^

@group(0) @binding(0) var t : texture_2d<f32>;

@group(0) @binding(1) var s : sampler;

@group(0) @binding(2) var<storage, read_write> a : atomic<i32>;

@fragment
fn foo(@location(0) in : f32, @location(1) coord : vec2<f32>) -> @location(0) i32 {
  if ((in == 0.0)) {
    discard;
  }
  var result = 0;
  for(var i = 0; (i < 10); i = atomicAdd(&(a), 1)) {
    result += i32(textureSample(t, s, coord).x);
  }
  return result;
}
