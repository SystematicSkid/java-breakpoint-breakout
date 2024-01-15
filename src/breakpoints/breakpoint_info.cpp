#include <java.hpp>
#include <breakpoints/breakpoint_info.hpp>
#include <breakpoints/breakpoints.hpp>

namespace breakpoints
{
    BreakpointInfo::BreakpointInfo( java::Method* method, uintptr_t bytecode_address, java::JavaThread* java_thread )
    {
        this->bytecode_address = bytecode_address;
        this->bytecode = new java::Bytecode( method->get_const_method( )->get_bytecode_start( ), (uint8_t*)bytecode_address );
        this->java_thread = java_thread;
    }

    BreakpointInfo::~BreakpointInfo( )
    {
        delete this->bytecode;
    }

    uintptr_t BreakpointInfo::get_bytecode_address( )
    {
        return this->bytecode_address;
    }

    java::Bytecode* BreakpointInfo::get_bytecode( )
    {
        return this->bytecode;
    }

    uintptr_t* BreakpointInfo::get_operand( int i )
    {
        if(!this->java_thread)
            return nullptr;
        if(i >= this->bytecode->get_stack_consumption( ))
            return nullptr;
        uintptr_t* operand_stack = this->java_thread->get_operand_stack( );
        return (uintptr_t*)(operand_stack + i);
    }

    int BreakpointInfo::get_operand_count( )
    {
        return this->bytecode->get_stack_consumption( );
    }
}