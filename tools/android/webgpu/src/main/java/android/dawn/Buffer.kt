package android.dawn

class Buffer(val handle: Long) {

    external fun destroy(): Unit
    val mapState: Int
        external get
    val size: Long
        external get
    val usage: Int
        external get

    external fun mapAsync(mode: Int, offset: Long, size: Long, callback: BufferMapCallback): Unit
    external fun setLabel(label: String?): Unit
    external fun unmap(): Unit
}
