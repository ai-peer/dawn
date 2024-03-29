package android.dawn

object WaitStatus {
    const val Success = 0x00000000;
    const val TimedOut = 0x00000001;
    const val UnsupportedTimeout = 0x00000002;
    const val UnsupportedCount = 0x00000003;
    const val UnsupportedMixedSources = 0x00000004;
    const val Unknown = 0x00000005;
}
