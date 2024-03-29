package android.dawn

class BindGroupDescriptor(
    var label: String? = null,
    var layout: BindGroupLayout? = null,
    var entries: Array<BindGroupEntry> = arrayOf()

) {
}
