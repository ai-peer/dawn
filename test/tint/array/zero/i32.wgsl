var<private> a = array<i32, 2>();

fn func() {
  // Defined above should not duplicate
  var a = array<i32, 2>();

  // Not defined above, should get own method
  var z = array<i32, 156>();
}

