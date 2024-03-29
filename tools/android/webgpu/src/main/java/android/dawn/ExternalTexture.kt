package android.dawn

class ExternalTexture(val handle: Long) {

    external fun destroy(): Unit
    external fun expire(): Unit
    external fun refresh(): Unit
    external fun setLabel(label: String?): Unit
}
