#pragma once
#include <java.hpp>
#include <Windows.h>

namespace java
{
    class InterpreterEntry
    {
    public:
        uintptr_t get_interception_address( )
        {
            /* Our pattern which identifies the *real* entrypoint to the method */
            /* ASM: jmp qword ptr ds:[r10+rbx*8]                                */
            /* This will never change, unless there is a custom jvm that changes the register order??? */
            /* Funny idea tbh */
            std::uint8_t pattern[ ] = { 0x41, 0xFF, 0x24, 0xDA };
            constexpr std::size_t pattern_size = sizeof( pattern );
            constexpr std::size_t max_search_size = 0x1000;
            constexpr std::size_t mov_size = 10;
            uintptr_t address = (uintptr_t)this;

            for ( std::size_t i = 0; i < max_search_size; i++ )
            {
                if ( std::memcmp( (void*)( address + i ), pattern, pattern_size ) == 0 )
                {
                    return address + i - mov_size;
                }
            }

            return 0;
        }
    };

    class Method
    {
    private:
        char pad[0x50];
    public:
        InterpreterEntry* i2i_entry;

        char* get_name( char* buffer, int size )
        {
            uintptr_t address = (uintptr_t)GetModuleHandleA( "jvm.dll" ) + 0x0C031C0;
            typedef char* ( __fastcall* get_name_fn )( Method*, char*, int );
            return ( ( get_name_fn )address )( this, buffer, size );
        }
    };
}