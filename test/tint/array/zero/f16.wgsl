enable f16;

var<private> a = array<f16, 2>();

fn func() {
  // Should not duplicate
  var a = array<f16, 2>();

  // Not defined above, should get own method
  var z = array<f16, 156>();
}

