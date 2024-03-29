package android.dawn

fun interface ErrorCallback {
    fun callback(
        type: Int,
        message: String
    );
}
