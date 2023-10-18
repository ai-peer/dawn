alias Mat = mat3x3<f32>;

struct S {
  m : Mat,
}

alias Type = S;

@group(0) @binding(0) var<storage, read_write> buffer : Type;

@group(0) @binding(1) var<storage, read_write> buffer2 : array<Type, 4>;

@compute @workgroup_size(1)
fn main() {
  var m : Mat;
  for(var c = 0u; (c < 3); c++) {
    m[c] = vec3(f32(((c * 3) + 1)), f32(((c * 3) + 2)), f32(((c * 3) + 3)));
  }
  buffer = Type(m);
  buffer2 = array<Type, 4>(Type(m), Type((m * 2)), Type((m * 3)), Type((m * 4)));
}
