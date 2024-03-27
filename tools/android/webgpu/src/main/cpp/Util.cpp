#include <jni.h>

#include <android/native_window_jni.h>

extern "C" JNIEXPORT jlong JNICALL Java_android_dawn_Util_windowFromSurface(JNIEnv* env,
                                                                            jclass thiz,
                                                                            jobject surface) {
    return reinterpret_cast<jlong>(ANativeWindow_fromSurface(env, surface));
}
