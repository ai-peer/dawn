var<private> v_2 = array<vec2<f32>, 3>();
var<private> v_3 = array<vec3<f32>, 3>();
var<private> v_4 = array<vec4<f32>, 3>();

fn func() {
  var v_2 = array<vec2<f32>, 3>();
  var v_3 = array<vec3<f32>, 3>();
  var v_4 = array<vec4<f32>, 3>();

  // Not defined above, should get own method
  var z = array<vec2<f32>, 156>();
}

