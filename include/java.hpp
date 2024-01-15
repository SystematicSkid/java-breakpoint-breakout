#pragma once
#include <jni.h>
#include <memory>
#include <string>
#include <cstdint>

/* Macros */
#include <utility/java_macros.hpp>

/* Classes */
#include <classes/bytecode.hpp>
#include <classes/string.hpp>
#include <classes/klass.hpp>
#include <classes/method.hpp>
#include <classes/mirror.hpp>
#include <classes/frame.hpp>

class JavaInterop
{
private:
    JavaVM* vm;
    JNIEnv* env;
public:
    JavaInterop( );
    ~JavaInterop( );

    jclass find_class( const char* name );
    jmethodID find_method( jclass clazz, const char* name, const char* sig );
    jmethodID find_static_method( jclass clazz, const char* name, const char* sig );
    jfieldID find_field( jclass clazz, const char* name, const char* sig );

    java::InstanceKlass* get_instance_class( jclass klasas );
};

extern std::unique_ptr<JavaInterop> java_interop;