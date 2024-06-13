package android.dawn.helper

fun Long.roundDown(boundary: Int) = this / boundary * boundary
fun Int.roundDown(boundary: Int) = this / boundary * boundary
fun Long.roundUp(boundary: Int) = (this + boundary - 1) / boundary * boundary
fun Int.roundUp(boundary: Int) = (this + boundary - 1) / boundary * boundary
