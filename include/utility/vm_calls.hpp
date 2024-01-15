#pragma once
#include <Windows.h>
#include <vector>
#include <string>

namespace vm_call
{
    /* For use in breakpoints */
    extern std::size_t thread_frame_offset;
    extern std::size_t thread_operand_stack_offset;

    std::string hex_to_bytes( std::string hex_string );
    uintptr_t scan( const char* pattern, uintptr_t start, uintptr_t end );
    std::vector<PVOID> find_vm_calls( PVOID start );
    uint8_t find_bytecode_start_offset( PVOID interpreter_entry );
}