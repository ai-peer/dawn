package android.dawn

fun interface PopErrorScopeCallback {
    fun callback(
        status: Int,
        type: Int,
        message: String
    );
}
