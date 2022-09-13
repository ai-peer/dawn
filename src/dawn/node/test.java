package com.google.dawn;

public class Dawn {
    static {
        System.loadLibrary("dawn");
    }

    public static void main(String[] args) {
        new HelloWorldJNI().sayHello();
    }

    // Declare a native method sayHello() that receives no arguments and returns void
    private native void sayHello();
}
