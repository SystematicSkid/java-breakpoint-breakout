#include <iostream>
#include <Windows.h>
#include <jni.h>

#include <utility/hook.hpp>
#include <java.hpp>

HMODULE my_module;

void test_callback( hook::hook_context* context )
{
    printf( "test_callback called\n" );
    return;
}

void main_thread( )
{
    /* Create console */
    AllocConsole( );
    freopen( "CONOUT$", "w", stdout );

    try
    {
        /* Create smart java interop */
        std::unique_ptr< JavaInterop > java = std::make_unique< JavaInterop >( );

        /* Find class */
        jclass clazz = java->find_class( "Main" );

        /* find 'int test_mba(int, int) */
        jmethodID test_mba = java->find_static_method( clazz, "test_mba", "(II)I" );
        java::Method* test_mba_method = *( java::Method** )test_mba;
        printf( "test_mba: %p\n", test_mba_method );
        printf( "Entry: %p\n", test_mba_method->i2i_entry );
        uintptr_t interception_address = test_mba_method->i2i_entry->get_interception_address( );
        printf( "Interception address: %p\n", interception_address );

        hook::hook( ( PVOID )interception_address, ( PVOID )test_callback );
        /* Wait for INSERT */
        while( !GetAsyncKeyState( VK_INSERT ) )
            Sleep( 100 );
    }
    catch(const std::exception& e)
    {
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