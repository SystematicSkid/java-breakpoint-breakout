#include <breakpoints/breakpoints.hpp>
#include <utility/vm_calls.hpp>
#include <utility/hook.hpp>

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

    uint8_t original_bytecode_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address )
    {
        /* Try to find original bytecode */
        uint8_t original_bytecode = original_bytecodes[ bytecode_address ];
        printf( "Original bytecode: %02X\n", original_bytecode );
        /* Return original bytecode */
        return original_bytecode;
    }

    void breakpoint_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address )
    {
        /* Try to find breakpoint callback */
        breakpoint_callback_t callback = breakpoint_callbacks[ bytecode_address ];
        if(!callback)
            return;
        /* Create breakpoint info */
        BreakpointInfo* breakpoint_info = new BreakpointInfo( method, bytecode_address, java_thread );
        /* Call callback */
        callback( breakpoint_info );
        /* Delete breakpoint info */
        delete breakpoint_info;
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

        
        constexpr uint8_t breakpoint_opcode = 0xCA;
        uintptr_t breakpoint_method = *(uintptr_t*)(dispatch_table + breakpoint_opcode * 8);
        if(!breakpoint_method)
            return false;

        std::vector<PVOID> vm_calls = vm_call::find_vm_calls( ( PVOID )breakpoint_method );
        if( vm_calls.size( ) < 2 )
            return false;

        PVOID runtime_get_original_bytecode = vm_calls[ 0 ];
        PVOID runtime_breakpoint_method     = vm_calls[ 1 ];

        if( !hook::hook_normal( runtime_get_original_bytecode, ( PVOID )&original_bytecode_handler ) )
            return false;
        if( !hook::hook_normal( runtime_breakpoint_method, ( PVOID )&breakpoint_handler ) )
            return false;

        return true;
    }
}