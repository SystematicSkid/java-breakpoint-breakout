#pragma once
#include <cstdint>
#include <java.hpp>

namespace breakpoints
{
    class BreakpointInfo
    {
    private:
        uintptr_t bytecode_address;
        uintptr_t parameters;
        java::Bytecode* bytecode;
        java::JavaThread* java_thread;
    public:
        BreakpointInfo( java::Method* method, uintptr_t bytecode_address, java::JavaThread* java_thread, uintptr_t parameters );
        ~BreakpointInfo( );

        uintptr_t get_bytecode_address( );
        java::Bytecode* get_bytecode( );
        uintptr_t* get_operand( int i );
        uintptr_t* get_operand_unsafe( int i );
        int get_operand_count( );
        uintptr_t* get_parameter( std::size_t index );
        void set_parameter( std::size_t index, uintptr_t value );
    };
}