package android.dawn

class CompilationMessage(
    var message: String? = null,
    var type: Int = 0,
    var lineNum: Long = 0,
    var linePos: Long = 0,
    var offset: Long = 0,
    var length: Long = 0,
    var utf16LinePos: Long = 0,
    var utf16Offset: Long = 0,
    var utf16Length: Long = 0
) {
}
