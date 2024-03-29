package android.dawn

open class MultisampleState(
    var count: Int = 1,
    var mask: Int = -0x7FFFFFFF,
    var alphaToCoverageEnabled: Boolean = false
) {
}
