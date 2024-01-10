#include <iostream>
#include <Windows.h>
#include <jni.h>

#include <utility/hook.hpp>
#include <java.hpp>
#include <tuple>
#include <functional>
#include <array>
#include <utility>
#include <cstddef>
#include <chrono>
#include <optional>

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

uint8_t* new_code = nullptr;

std::map<java::Method*, void*> method_callback_map;

class Main
{
private:
    SET_STATIC_CLASS( Main );
public:
    SET_MEMBER( int, field1, "I" );
    SET_MEMBER( int, field2, "I" );
};

/* static int test_mba(int x, int y) */
std::optional<int*> callback_test_mba( int* x, int* y )
{
    printf( "[callback_test_mba]\n" );
    printf( "\tX: %d\n", *x );
    printf( "\tY: %d\n", *y );
    /* Spoof arg y to be 2 */
    *y = *y + 1;
    return std::nullopt;
}

long long last_call = 0;

/* private int get_field1( ) */
std::optional<int> callback_get_field1( Main** main_object )
{
    Main* obj = *main_object;
    printf( "[callback_get_field1]\n" );
    printf( "\tMain Object: %p\n", obj );
    printf( "\tField1: %d\n", obj->field1 );
    /* Get current time in ms */
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(!last_call)
    {
        last_call = now;
    }
    else
    {
        printf("\tTime since last call: %lld\n", now - last_call);
        last_call = now;
    }

    return std::nullopt;
}

void callback_handler( hook::hook_context* context )
{
    java::Frame* frame = java::Frame::create_frame( ( uintptr_t )context->rbp );
    java::Method* method = frame->method;

    /* Search method_callback_map for method */
    auto it = method_callback_map.find( method );
    if( it != method_callback_map.end( ) )
    {
        auto callback = it->second;
        uint16_t num_params = method->get_num_arguments( );
        if(num_params == 0)
        {
            reinterpret_cast<std::optional<void*>(__fastcall*)( )>( callback )( );
        }
        if( num_params == 1 )
        {
            reinterpret_cast<std::optional<void*> ( __fastcall* )( uintptr_t )>( callback )( ( uintptr_t )context->get_parameter( 0 ) );
        }
        if( num_params == 2 )
        {
            reinterpret_cast<std::optional<void*> ( __fastcall* )( uintptr_t, uintptr_t )>( callback )( ( uintptr_t )context->get_parameter( 0 ), ( uintptr_t )context->get_parameter( 1 ) );
        }
        if( num_params == 3 )
        {
            reinterpret_cast<std::optional<void*> ( __fastcall* )( uintptr_t, uintptr_t, uintptr_t )>( callback )( ( uintptr_t )context->get_parameter( 0 ), ( uintptr_t )context->get_parameter( 1 ), ( uintptr_t )context->get_parameter( 2 ) );
        }

    }
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

    //int max_size = 10 * 256;
    //uintptr_t table_addr = (uintptr_t)GetModuleHandleA( "jvm.dll" ) + 0x17233D0;
    //for(int i = 0; i < max_size; i++)
    //{
    //    uintptr_t func = *( uintptr_t* )( table_addr + i * 8 );
    //    fprintf( logFile, "%02X : %p\n", i & 0xFF, func);
    //}



    try
    {
        /* Create smart java interop */
        java_interop = std::make_unique< JavaInterop >( );

        
        //void* empty_test = malloc(0x1000);
        //Main* main_object = (Main*)empty_test;
        //printf("Field1: %d\n", main_object->field1);
        //free(empty_test);

        /* Find class */
        jclass clazz = java_interop->find_class( "Main" );
        main_class = java_interop->get_instance_class( clazz );

        /* find 'int test_mba(int, int) */
        jmethodID test_mba = java_interop->find_static_method( clazz, "test_mba", "(II)I" );
        jmethodID get_fields = java_interop->find_method( clazz, "get_field1", "()I" );
        jmethodID main = java_interop->find_static_method( clazz, "main", "([Ljava/lang/String;)V" );
        main_method = *( java::Method** )main;
        java::Method* test_mba_method = *( java::Method** )test_mba;
        java::Method* get_fields_method = *( java::Method** )get_fields;
        printf("Get fields: %p\n", get_fields_method);
        method_callback_map[test_mba_method] = callback_test_mba;
        method_callback_map[get_fields_method] = callback_get_field1;
        mba_method = test_mba_method;
        printf( "test_mba: %p\n", test_mba_method );
        printf("callback_test_mba: %p\n", callback_test_mba);
        printf( "Entry: %p\n", test_mba_method->i2i_entry );
        uintptr_t interception_address = test_mba_method->i2i_entry->get_interception_address( );
        printf( "Interception address: %p\n", interception_address );
        printf("Parameters size: %d\n", get_fields_method->get_const_method()->get_method_parameters_length());

        jfieldID field1 = java_interop->find_field( clazz, "field1", "I" );
        printf( "Field1: %p\n", field1 );
        jfieldID field2 = java_interop->find_field( clazz, "field2", "I" );
        printf( "Field2: %p\n", field2 );

        hook::hook( ( PVOID )interception_address, ( PVOID )callback_handler );
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