#pragma once
#include <unordered_map>
#include <map>
#include <cstdint>
#include <Windows.h>
#include <jni.h>
#include <queue>
#include <java.hpp>

namespace hook
{
    struct hook_context
    {
        uintptr_t skip_original_call;
        uintptr_t flags;
        uintptr_t r15;
        uintptr_t r14;
        uintptr_t r13;
        uintptr_t r12;
        uintptr_t r11;
        uintptr_t r10;
        uintptr_t r9;
        uintptr_t r8;
        uintptr_t rbp;
        uintptr_t rdi;
        uintptr_t rsi;
        uintptr_t rdx;
        uintptr_t rcx;
        uintptr_t rbx;
        uintptr_t rax;

        uintptr_t get_parameter( std::size_t index )
        {
            uintptr_t locals = r14;
            return *( uintptr_t* )( locals - index * 8 );
        }
    };
    
    extern std::unordered_map<PVOID, PVOID> original_functions;
    extern std::map<PVOID, std::uint8_t*> hook_map;
    constexpr std::size_t shell_size = 13;

    bool setup( );
    void update( );

    void suspend_all_threads( );
    void resume_all_threads( );
    
    std::size_t get_minimum_shell_size( PVOID target );
    void construct_shell( std::uint8_t* shell, PVOID target );
    void* create_trampoline( PVOID target, std::size_t& size );
    void* create_naked_shell( PVOID callback, PVOID trampoline );
    bool hook( PVOID original, PVOID hook );
    bool hook_method_code( jmethodID original, PVOID callback );
    bool add_hook( jmethodID original, PVOID callback );
    bool unhook( PVOID original );
    PVOID get_original( PVOID method );
}