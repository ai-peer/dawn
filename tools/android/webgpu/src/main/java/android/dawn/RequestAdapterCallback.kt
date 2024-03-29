package android.dawn

fun interface RequestAdapterCallback {
    fun callback(
        status: Int,
        adapter: Adapter?,
        message: String?
    );
}
