#include <iostream>
#include <intrin.h>
#include <Windows.h>
#include <jni.h>
#include <utility/vm_calls.hpp>
#include <utility/hook.hpp>
#include <java.hpp>
#include <breakpoints/breakpoints.hpp>

#include <tuple>
#include <functional>
#include <array>
#include <utility>
#include <cstddef>
#include <chrono>
#include <optional>

HMODULE my_module;

void main_thread( )
{
    /* Create console */
    AllocConsole( );
    freopen( "CONOUT$", "w", stdout );

    try
    {
        /* Create smart java interop */
        java_interop = std::make_unique< JavaInterop >( );

        bool setup_result = breakpoints::setup( java_interop.get( ) );
        if( !setup_result )
            throw std::runtime_error( "Failed to setup breakpoints" );

        /* Find class */
        jclass clazz = java_interop->find_class( "Main" );

        /* Get our test method */
        jmethodID test_mul = java_interop->find_static_method( clazz, "test_mul", "(II)I" );
        java::Method* test_mul_method = *(java::Method**)(test_mul);
        
        test_mul_method->set_breakpoint(
            0x00, /* Offset */
            [ ]( breakpoints::BreakpointInfo* bp )
            {
                printf( "[test_mul] called\n" );
            } 
        );

        /* Wait for HOME */
        while( !GetAsyncKeyState( VK_HOME ) )
            Sleep( 100 );
        printf( "Removing breakpoints\n" );
        test_mul_method->remove_all_breakpoints( );
    }
    catch(const std::exception& e)
    {
        std::cerr << "Exception:";
        std::cerr << e.what() << '\n';
    }
    

    /* Wait for INSERT */
    while( !GetAsyncKeyState( VK_INSERT ) )
        Sleep( 100 );

    /* Unload */
    std::cout << "Unloading" << std::endl;
    FreeLibraryAndExitThread( my_module, 0 );
}

BOOL APIENTRY DllMain( HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved ) 
{
    if( ul_reason_for_call != DLL_PROCESS_ATTACH )
        return TRUE;
    
    my_module = hModule;

    /* Create thread */
    CreateThread( NULL, 0, ( LPTHREAD_START_ROUTINE )main_thread, NULL, 0, NULL );

    return TRUE;
}