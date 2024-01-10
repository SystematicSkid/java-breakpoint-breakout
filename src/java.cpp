#include <java.hpp>
#include <stdexcept>
#include <iostream>

std::unique_ptr<JavaInterop> java_interop;

JavaInterop::JavaInterop( )
{
    jint vm_count = 0;
    /* Try to get the first created jvm */
    jint error = JNI_GetCreatedJavaVMs( &vm, 1, &vm_count );

    /* Check error */
    if( error != JNI_OK || vm_count == 0 )
        throw std::runtime_error( "Failed to get created Java VMs" );

    /* Attach thread */
    error = vm->AttachCurrentThread( ( void** )&env, NULL );

    /* Check error */
    if( error != JNI_OK )
        throw std::runtime_error( "Failed to attach thread" );
}

JavaInterop::~JavaInterop( )
{
    std::cout << "Detaching thread" << std::endl;
    if( !vm )
        return;
    /* Detach thread */
    vm->DetachCurrentThread( );
}

jclass JavaInterop::find_class( const char* name )
{
    /* Find class */
    jclass clazz = env->FindClass( name );

    /* Check error */
    if( !clazz )
        throw std::runtime_error( "Failed to find class" );

    return clazz;
}

jmethodID JavaInterop::find_method( jclass clazz, const char* name, const char* sig )
{
    /* Find method */
    jmethodID method = env->GetMethodID( clazz, name, sig );

    /* Check error */
    if( !method )
        throw std::runtime_error( "Failed to find method" );

    return method;
}

jmethodID JavaInterop::find_static_method( jclass clazz, const char* name, const char* sig )
{
    /* Find method */
    jmethodID method = env->GetStaticMethodID( clazz, name, sig );

    /* Check error */
    if( !method )
        throw std::runtime_error( "Failed to find static method" );

    return method;
}

jfieldID JavaInterop::find_field( jclass clazz, const char* name, const char* sig )
{
    /* Find field */
    jfieldID field = env->GetFieldID( clazz, name, sig );

    /* Check error */
    if( !field )
        throw std::runtime_error( "Failed to find field" );

    return field;
}

java::InstanceKlass* JavaInterop::get_instance_class( jclass clazz )
{
    /* Ensure non-null */
    if( clazz == nullptr )
        return nullptr;

    /* Dereference klass and get ptr at +0x10 */
    void* klass_ptr = *(void**)( clazz );
    if ( klass_ptr == nullptr )
        return nullptr;
        
    klass_ptr = *(void**)( ( uintptr_t )klass_ptr + 0x10 );
    return (java::InstanceKlass*)klass_ptr;
}