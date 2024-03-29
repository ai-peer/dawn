package android.dawn

fun interface LoggingCallback {
    fun callback(
        type: Int,
        message: String
    );
}
