// flags: --overrides o=0
override o : bool;

@compute @workgroup_size(1)
fn main() {
    if o {
        _ = 1;
    }
}
