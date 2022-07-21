// flags: --overrides o=0
override o : bool = true;

@compute @workgroup_size(1)
fn main() {
    if o {
        _ = o;
    }
}
