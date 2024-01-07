#include <iostream>
#include <Windows.h>
#include <jni.h>

#include <utility/hook.hpp>
#include <java.hpp>

HMODULE my_module;

// Global log file pointer and critical section
FILE* logFile = NULL;
CRITICAL_SECTION logCriticalSection;

void initializeLogging() {
    InitializeCriticalSection(&logCriticalSection);
    logFile = fopen("log.txt", "a"); // Open in append mode
    if (!logFile) {
        // Handle error, e.g., exit or print an error message
    }
}

void finalizeLogging() {
    if (logFile) {
        fclose(logFile);
        logFile = NULL;
    }
    DeleteCriticalSection(&logCriticalSection);
}

java::Method* main_method = nullptr;
java::Method* mba_method = nullptr;
java::InstanceKlass* main_class = nullptr;

void test_callback( hook::hook_context* context )
{

    char buffer[1024];
    java::Frame* frame = java::Frame::create_frame( ( uintptr_t )context->rbp );
    java::Method* method = frame->method;
    const char* name = method->get_name( buffer, sizeof( buffer ) );

    //if(!frame->sender_sp)
    //    return;
    //if(!frame->last_sp)
    //    return;
    //if(!(*( uintptr_t* )(frame->sender_sp - 0x10)))
    //    return;

    java::Frame* sender_frame = java::Frame::create_frame( frame->link );
    if(!sender_frame)
        return;
    if((uintptr_t)sender_frame % 0x10 != 0)
        return;
    java::Method* sender_method = sender_frame->method;
    if(!sender_method)
        return;
   
    //java::Method* sender_method = sender_frame->method;
    //if(!sender_method)
    //    return;
    if(!frame->mirror || !frame->mirror->klass)
        return;

    if ( mba_method == method )
    {
        printf( "Method name: %s\n", name );
        size_t arg1 = context->get_parameter( 0 );
        size_t arg2 = context->get_parameter( 1 );
        printf( "\tArg1: %d\n", arg1 );
        printf( "\tArg2: %d\n", arg2 );
        //frame->debug_print( );
        EnterCriticalSection(&logCriticalSection);
        fprintf(logFile, "Method name: %s\n", name);
        LeaveCriticalSection(&logCriticalSection);

        if(sender_method)
        {
            const char* sender_name = sender_method->get_name( buffer, sizeof( buffer ) );
            printf( "\tSender method name: %s\n", sender_name );
        }
    }
    return;
}

void main_thread( )
{
    /* Create console */
    AllocConsole( );
    freopen( "CONOUT$", "w", stdout );

    /* Delete log.txt */
    DeleteFileA( "log.txt" );

    /* Initialize logging */
    initializeLogging();

    try
    {
        /* Create smart java interop */
        std::unique_ptr< JavaInterop > java = std::make_unique< JavaInterop >( );

        /* Find class */
        jclass clazz = java->find_class( "Main" );
        main_class = java->get_instance_class( clazz );

        /* find 'int test_mba(int, int) */
        jmethodID test_mba = java->find_static_method( clazz, "test_mba", "(II)I" );
        jmethodID main = java->find_static_method( clazz, "main", "([Ljava/lang/String;)V" );
        main_method = *( java::Method** )main;
        java::Method* test_mba_method = *( java::Method** )test_mba;
        mba_method = test_mba_method;
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

    /* Finalize logging */
    finalizeLogging();
    

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