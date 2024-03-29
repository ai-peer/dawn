package android.dawn

class QuerySet(val handle: Long) {

    external fun destroy(): Unit
    val count: Int
        external get
    val type: Int
        external get

    external fun setLabel(label: String?): Unit
}
