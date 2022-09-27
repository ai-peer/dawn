var<private> a = array<mat2x3<f32>, 3>();

fn func() {
  // These should all re-use the methods defined above
  var g = array<mat2x3<f32>, 3>();

  // Not defined above, should get own method
  var z = array<mat3x2<f32>, 156>();
}

