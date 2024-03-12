#pragma once
#include <java.hpp>
#include <Windows.h>
#include <functional>

namespace breakpoints
{
    /* Forward definition */
    typedef std::function<void( class BreakpointInfo* )> breakpoint_callback_t;
}
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

    class ConstMethod
    {
    private:
        enum class ConstMethodFlags : uint32_t
        {
            has_linenumber_table      = 1 << 0,
            has_checked_exceptions    = 1 << 1,
            has_localvariable_table   = 1 << 2,
            has_exception_table       = 1 << 3,
            has_generic_signature     = 1 << 4,
            has_method_parameters     = 1 << 5,
            is_overpass               = 1 << 6,
            has_method_annotations    = 1 << 7,
            has_parameter_annotations = 1 << 8,
            has_type_annotations      = 1 << 9,
            has_default_annotations   = 1 << 10,
            caller_sensitive          = 1 << 11,
            is_hidden                 = 1 << 12,
            has_injected_profile      = 1 << 13,
            intrinsic_candidate       = 1 << 14,
            reserved_stack_access     = 1 << 15,
            is_scoped                 = 1 << 16,
            changes_current_thread    = 1 << 17,
            jvmti_mount_transition    = 1 << 18,
            deprecated                = 1 << 19,
            deprecated_for_removal    = 1 << 20
        };
    public:
        uintptr_t end( )
        {
            return (uintptr_t)this + this->const_method_size * sizeof( uintptr_t );
        }
    private:
        uint64_t fingerprint;
        void* constant_pool;
        void* stackmap_data;
        int const_method_size;
        uint32_t flags;

        bool has_method_annotations( )
        {
            return ( this->flags & (uint32_t)ConstMethodFlags::has_method_annotations ) != 0;
        }

        bool has_parameter_annotations( )
        {
            return ( this->flags & (uint32_t)ConstMethodFlags::has_parameter_annotations ) != 0;
        }

        bool has_type_annotations( )
        {
            return ( this->flags & (uint32_t)ConstMethodFlags::has_type_annotations ) != 0;
        }

        bool has_default_annotations( )
        {
            return ( this->flags & (uint32_t)ConstMethodFlags::has_default_annotations ) != 0;
        }

        uint16_t* method_parameters_length_addr( )
        {
            int offset = 0;
            if( has_method_annotations( ) )
                offset++;
            if( has_parameter_annotations( ) )
                offset++;
            if( has_type_annotations( ) )
                offset++;
            if( has_default_annotations( ) )
                offset++;
            return (uint16_t*)( end() - offset ) - 1;
        }

    public:
        bool has_method_parameters( )
        {
            return ( this->flags & (uint32_t)ConstMethodFlags::has_method_parameters ) != 0;
        }

        int get_method_parameters_length( )
        {
            uint16_t* addr = method_parameters_length_addr();
            if( !addr )
                return -1;
            return *addr;
        }

        static size_t bytecode_start_offset;

        uint8_t* get_bytecode_start( )
        {
            return (uint8_t*)((uintptr_t)this + bytecode_start_offset);
        }

        int get_bytecode_size( )
        {
            uint8_t* start = get_bytecode_start( );
            std::unique_ptr< java::Bytecode > bytecode = std::make_unique< java::Bytecode >( start );

            int offset = 0;
            while( bytecode->get_opcode( ) != java::Bytecodes::invalid )
            {
                int length = bytecode->get_length( );
                start += length;
                bytecode = std::make_unique< java::Bytecode >( start );
            }
            return start - get_bytecode_start( );
        }

        void set_bytecode( std::vector<uint8_t>& bytecode )
        {
            int bytecode_size = get_bytecode_size( );
            if ( bytecode_size < bytecode.size( ) )
            {
                printf( "Bytecode size mismatch\n" );
                return;
            }
            memcpy( get_bytecode_start( ), bytecode.data( ), bytecode.size( ) );
            /* set remaining bytes to `nop` */
            memset( get_bytecode_start( ) + bytecode.size( ), (uint8_t)java::Bytecodes::nop, bytecode_size - bytecode.size( ) );
        }

        std::vector<uint8_t> get_bytecode( )
        {
            int bytecode_size = get_bytecode_size( );
            std::vector<uint8_t> bytecode( bytecode_size );
            memcpy( bytecode.data( ), get_bytecode_start( ), bytecode_size );
            return bytecode;
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

        int get_num_arguments( )
        {
            return get_const_method( )->get_method_parameters_length( );
        }

        ConstMethod* get_const_method( )
        {
            return *( ConstMethod** )( (uintptr_t)this + 0x10 );
        }

        void set_breakpoint( int offset, breakpoints::breakpoint_callback_t callback );
        void remove_breakpoint( int offset );
        void remove_all_breakpoints( );
    };
}