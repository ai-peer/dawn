package android.dawn

object ErrorType {
    const val NoError = 0x00000000;
    const val Validation = 0x00000001;
    const val OutOfMemory = 0x00000002;
    const val Internal = 0x00000003;
    const val Unknown = 0x00000004;
    const val DeviceLost = 0x00000005;
}
