struct vec4f_1 {
  i : i32,
}

type vec2f_1 = f32;

type vec2i_1 = bool;

@vertex
fn main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4<f32> {
  let s = vec4f_1(1);
  let f : f32 = vec2f_1(s.i);
  let b : bool = vec2i_1(f);
  return select(vec4<f32>(), vec4<f32>(1), b);
}
