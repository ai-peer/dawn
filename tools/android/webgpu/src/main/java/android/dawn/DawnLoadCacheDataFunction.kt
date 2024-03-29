package android.dawn

fun interface DawnLoadCacheDataFunction {
    fun callback(
        key: ByteArray,
        keySize: Long,
        value: ByteArray,
        valueSize: Long
    );
}
