var<private> a = array<array<i32, 2>, 3>();
var<private> b = array<array<array<array<f32, 4>, 3>, 2>, 1>();

fn func() {
  // Same as above, should not duplciate
  var c = array<array<i32, 2>, 3>();

  // Not defined above, should get own method
  var z = array<array<f32, 4>, 156>();
}

