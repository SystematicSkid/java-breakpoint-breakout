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

std::map<uintptr_t, uint8_t> original_bytecodes;

void* original_get_original_bytecode_at = nullptr;

/* static Bytecodes::Code get_original_bytecode_at(JavaThread* current, Method* method, address bcp); */
uint8_t callback_get_original_bytecode_at( void* current_thread, java::Method* method, uintptr_t bcp )
{
    /* Check if our map contains bcp */
    auto it = original_bytecodes.find( bcp );
    if( it != original_bytecodes.end( ) )
    {
        /* Return original bytecode */
        return it->second;
    }
    printf("Non-intercepted breakpoint\n");
    return 0;
}

void* original_breakpoint = nullptr;

/* static void _breakpoint(JavaThread* current, Method* method, address bcp); */
void callback_breakpoint( void* current_thread, java::Method* method, uintptr_t bcp )
{
    printf( "[callback_breakpoint]\n" );
    printf( "\tCurrent: %p\n", current_thread );
    printf( "\tMethod: %p\n", method );
    printf( "\tBCP: %p\n", bcp );
    uint32_t operand_stack_offset = vm_call::thread_operand_stack_offset;
    printf( "\tOperand stack offset: %p\n", operand_stack_offset );
    uintptr_t* operand_stack = *(uintptr_t**)( (uintptr_t)current_thread + operand_stack_offset );
    uint8_t original_bytecode = original_bytecodes[bcp];
    uint8_t stack_consumption = breakpoints::bytecode_operand_consumption[original_bytecode];
    printf( "\tOriginal bytecode: %02X\n", original_bytecode );
    printf( "\tStack consumption: %d\n", stack_consumption );

    printf("Operand 0: %p\n", operand_stack[0]);

    MessageBoxA( NULL, "Breakpoint hit", "Breakpoint", MB_OK );
}

void set_breakpoint( uint8_t* bytecode, int offset )
{
    printf( "Setting breakpoint at offset %d\n", offset );
    printf( "\tBytecode: %02X\n", bytecode[offset] );
    original_bytecodes[(uintptr_t)bytecode + offset] = bytecode[offset];
    bytecode[offset] = 0xCA;
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
        jmethodID test_mba = java_interop->find_static_method( clazz, "fibb", "(I)I" );
        java::Method* test_mba_method = *(java::Method**)(test_mba);
        //method_callback_map[test_mba_method] = callback_test_mba;
        //method_callback_map[get_fields_method] = callback_get_field1;
        //mba_method = test_mba_method;
        printf( "Entry: %p\n", test_mba_method->i2i_entry );
        uint8_t const_method_bytecode_offset = vm_call::find_bytecode_start_offset( test_mba_method->i2i_entry );
        printf( "Bytecode offset: %02X\n", const_method_bytecode_offset );
        uintptr_t interception_address = test_mba_method->i2i_entry->get_interception_address( );
        printf( "Interception address: %p\n", interception_address );
        printf("Parameters size: %d\n", test_mba_method->get_const_method()->get_method_parameters_length());
        printf("Const method: %p\n", test_mba_method->get_const_method());
        printf("Const method end: %p\n", test_mba_method->get_const_method()->end());

        uintptr_t dispatch_table = *(uintptr_t*)(interception_address + 2);
        printf("Dispatch table: %p\n", dispatch_table);
        constexpr uint8_t breakpoint_opcode = 0xCA;
        uintptr_t breakpoint_method = *(uintptr_t*)(dispatch_table + breakpoint_opcode * 8);
        printf("Breakpoint method: %p\n", breakpoint_method);

        std::vector<PVOID> vm_calls = vm_call::find_vm_calls( ( PVOID )breakpoint_method );
        if( vm_calls.size( ) != 2 )
            throw std::runtime_error( "Failed to find vm calls" );
        PVOID runtime_get_original_bytecode = vm_calls[0];
        PVOID runtime_breakpoint_method = vm_calls[1];
        printf( "Get original bytecode: %p\n", runtime_get_original_bytecode );
        printf( "Breakpoint method: %p\n", runtime_breakpoint_method );

        
        int max_bytecode_size = 0x100;
        uint8_t* bytecode = (uint8_t*)((uintptr_t)test_mba_method->get_const_method() + const_method_bytecode_offset);
        printf("Bytecode: %p\n", bytecode);
        for(int i = 0; i < max_bytecode_size; )
        {
            uint8_t opcode = bytecode[i];
            std::unique_ptr<java::Bytecode> bytecode_obj = std::make_unique<java::Bytecode>(bytecode, bytecode + i);
            if(opcode == 0xFF)
            {
                break;
            }
            printf("Bytecode %02X: %02X\n", i, opcode);
            printf("\tOpcode: %02X\n", bytecode_obj->get_opcode());
            printf("\tLength: %d\n", bytecode_obj->get_length());
            printf("\tStack consumption: %d\n", bytecode_obj->get_stack_consumption());
            i += bytecode_obj->get_length();
        }

        //set_breakpoint( bytecode, 0x2 );

        hook::hook_normal( runtime_get_original_bytecode, ( PVOID )&callback_get_original_bytecode_at );
        hook::hook_normal( runtime_breakpoint_method, ( PVOID )&callback_breakpoint );

        //hook::hook( ( PVOID )interception_address, ( PVOID )callback_handler );
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