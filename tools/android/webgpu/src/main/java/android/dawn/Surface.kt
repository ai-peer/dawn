package android.dawn

class Surface(val handle: Long) {

    external fun getPreferredFormat(adapter: Adapter): Int
}
