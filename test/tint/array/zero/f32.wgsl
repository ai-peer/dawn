var<private> a = array<f32, 2>();

fn func() {
  // Defined above should not duplicate
  var a = array<f32, 2>();

  // Not defined above, should get own method
  var z = array<f32, 156>();
}

