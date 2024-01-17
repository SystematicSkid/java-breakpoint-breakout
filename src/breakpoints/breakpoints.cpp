#include <breakpoints/breakpoints.hpp>
#include <utility/vm_calls.hpp>
#include <utility/hook.hpp>

extern "C" {
    uintptr_t __forceinline jhook_get_r14_address( );
}

namespace breakpoints
{
    std::map< uintptr_t, uint8_t > original_bytecodes;
    std::map< uintptr_t, breakpoint_callback_t > breakpoint_callbacks;

    bool add_breakpoint( java::Method* method, int offset, breakpoint_callback_t callback )
    {
        uint8_t* bytecode_address = method->get_const_method( )->get_bytecode_start( ) + offset;
        /* Read bytecode */
        uint8_t bytecode = *bytecode_address;
        /* Add original bytecode to map */
        original_bytecodes[ (uintptr_t)bytecode_address ] = bytecode;
        /* Set callback */
        breakpoint_callbacks[ (uintptr_t)bytecode_address ] = callback;
        /* Set breakpoint */
        *bytecode_address = 0xCA;

        // TODO: Add error checking

        return true;
    }

    bool remove_breakpoint( java::Method* method, int offset )
    {
        uint8_t* bytecode_address = method->get_const_method( )->get_bytecode_start( ) + offset;
        /* Read original bytecode */
        uint8_t bytecode = original_bytecodes[ (uintptr_t)bytecode_address ];
        /* Remove original bytecode from map */
        original_bytecodes.erase( (uintptr_t)bytecode_address );
        /* Remove callback */
        breakpoint_callbacks.erase( (uintptr_t)bytecode_address );
        /* Remove breakpoint */
        *bytecode_address = bytecode;

        return true;
    }

    bool remove_all_breakpoints( java::Method* method )
    {
        /* Get bytecode start */
        uint8_t* bytecode_start = method->get_const_method( )->get_bytecode_start( );
        /* Iterate over all bytecodes */
        for( uint8_t* bytecode_address = bytecode_start; ; bytecode_address++ )
        {
            /* Exit case */
            if(*bytecode_address == 0xFF)
                break;
            /* Check if this bytecode is a breakpoint */
            if( original_bytecodes.find( (uintptr_t)bytecode_address ) != original_bytecodes.end( ) )
            {
                /* Read original bytecode */
                uint8_t bytecode = original_bytecodes[ (uintptr_t)bytecode_address ];
                /* Remove original bytecode from map */
                original_bytecodes.erase( (uintptr_t)bytecode_address );
                /* Remove callback */
                breakpoint_callbacks.erase( (uintptr_t)bytecode_address );
                /* Remove breakpoint */
                *bytecode_address = bytecode;
            }
        }

        return true;
    }

    uint8_t original_bytecode_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address )
    {
        /* Try to find original bytecode */
        uint8_t original_bytecode = original_bytecodes[ bytecode_address ];
        printf( "Original bytecode: %02X\n", original_bytecode );
        /* Return original bytecode */
        return original_bytecode;
    }

    _declspec( noinline )
    void breakpoint_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address, uintptr_t parameters )
    {
        /* Try to find breakpoint callback */
        breakpoint_callback_t callback = breakpoint_callbacks[ bytecode_address ];
        if(!callback)
            return;
        /* Create breakpoint info */
        BreakpointInfo* breakpoint_info = new BreakpointInfo( method, bytecode_address, java_thread, parameters );
        /* Call callback */
        callback( breakpoint_info );
        /* Delete breakpoint info */
        delete breakpoint_info;
    }

    /*
        This is the callback we use to intercept the real breakpoint handler
        This is a hacky wrapper around `breakpoint_handler` to prevent the compiler from using r14
        before we can get it
    */
    void breakpoint_callback( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address )
    {
        uintptr_t parameters = jhook_get_r14_address( );
        breakpoint_handler( java_thread, method, bytecode_address, parameters );
    }

    bool setup( JavaInterop* interop )
    {
        /* 
            TODO: Find a common class, then perform VMCall access on it
            Get breakpoint addresses via this method
            Then apply the hook stuff
        */

       /* Our target method: java.lang.Integer.hashCode() */
        jclass integer_klass = interop->find_class( "java/lang/Integer" );
        if(!integer_klass)
           return false;
        jmethodID integer_hash_code = interop->find_method( integer_klass, "hashCode", "()I" );
        if(!integer_hash_code)
            return false;
        java::Method* hash_method = *(java::Method**)( integer_hash_code );
        if(!hash_method)
            return false;

        uintptr_t interception_address = hash_method->i2i_entry->get_interception_address( );
        if(!interception_address)
            return false;

        /* Store bytecode start address */
        /* I am well aware this is ugly! */
        vm_call::find_bytecode_start_offset( hash_method->i2i_entry );

        uintptr_t dispatch_table = *(uintptr_t*)( interception_address + 2 );
        if(!dispatch_table)
            return false;

        
        uintptr_t breakpoint_method = *(uintptr_t*)(dispatch_table + breakpoint_opcode * 8);
        if(!breakpoint_method)
            return false;

        printf("Breakpoint method: %p\n", breakpoint_method);

        std::vector<PVOID> vm_calls = vm_call::find_vm_calls( ( PVOID )breakpoint_method );
        if( vm_calls.size( ) < 2 )
            return false;

        PVOID runtime_get_original_bytecode = vm_calls[ 0 ];
        PVOID runtime_breakpoint_method     = vm_calls[ 1 ];

        if( !hook::hook_normal( runtime_get_original_bytecode, ( PVOID )&original_bytecode_handler ) )
            return false;
        if( !hook::hook_normal( runtime_breakpoint_method, ( PVOID )&breakpoint_callback ) )
            return false;

        return true;
    }
}