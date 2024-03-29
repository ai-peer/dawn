package android.dawn

fun interface CompilationInfoCallback {
    fun callback(
        status: Int,
        compilationInfo: Long
    );
}
