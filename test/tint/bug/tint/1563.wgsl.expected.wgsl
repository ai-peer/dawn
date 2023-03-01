fn foo() -> f32 {
  let oob = 99;
  let index = oob;
  let predicate = (u32(index) <= 3u);
  var predicated_load : f32;
  if (predicate) {
    predicated_load = vec4<f32>()[index];
  }
  let b = predicated_load;
  var v : vec4<f32>;
  let index_1 = oob;
  let predicate_1 = (u32(index_1) <= 3u);
  if (predicate_1) {
    v[index_1] = b;
  }
  return b;
}
