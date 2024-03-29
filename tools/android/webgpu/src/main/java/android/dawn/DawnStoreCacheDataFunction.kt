package android.dawn

fun interface DawnStoreCacheDataFunction {
    fun callback(
        key: ByteArray,
        keySize: Long,
        value: ByteArray,
        valueSize: Long
    );
}
