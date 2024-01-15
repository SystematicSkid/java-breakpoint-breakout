#include <utility/vm_calls.hpp>
#include <hde/hde64.hpp>
#include <algorithm>
#include <string>

namespace vm_call
{
    std::size_t thread_frame_offset         = 0x0;
    std::size_t thread_operand_stack_offset = 0x0;

    /*
        All calls from interpreter to the VM look like this:
        000001F2AAF7717F | C5F877                   | vzeroupper                                                                           |
        000001F2AAF77182 | 49:89AF F0030000         | mov qword ptr ds:[r15+3F0],rbp                                                       |
        000001F2AAF77189 | 49:8987 E0030000         | mov qword ptr ds:[r15+3E0],rax                                                       |
        000001F2AAF77190 | 48:83EC 20               | sub rsp,20                                                                           |
        000001F2AAF77194 | 40:F6C4 0F               | test spl,F                                                                           |
        000001F2AAF77198 | 0F84 19000000            | je 1F2AAF771B7                                                                       |
        000001F2AAF7719E | 48:83EC 08               | sub rsp,8                                                                            |
        000001F2AAF771A2 | 48:B8 A0A537F9FB7F0000   | mov rax,<jvm.public: static enum Bytecodes::Code __cdecl InterpreterRuntime::get_ori |
        000001F2AAF771AC | FFD0                     | call rax                                                                             |
        000001F2AAF771AE | 48:83C4 08               | add rsp,8                                                                            |
        000001F2AAF771B2 | E9 0C000000              | jmp 1F2AAF771C3                                                                      |
        000001F2AAF771B7 | 48:B8 A0A537F9FB7F0000   | mov rax,<jvm.public: static enum Bytecodes::Code __cdecl InterpreterRuntime::get_ori |
        000001F2AAF771C1 | FFD0                     | call rax                                                                             |
        000001F2AAF771C3 | 48:83C4 20               | add rsp,20                                                                           |
    */
   
   std::string vm_call_pattern = R"(
    C5 F8 77      
    49 89 AF ? ? ? ?
    49 89 87 ? ? ? ?
    48 83 EC 20
    40 F6 C4 0F
    0F 84 ? ? ? ?
    48 83 EC 08                  
    48 B8 ? ? ? ? ? ? ? ?  
    FF D0                          
    )";

    std::string vm_call_address_pattern = R"(
    48 83 EC 08                 
    48 B8 ? ? ? ? ? ? ? ?   
    FF D0                           
    )";

    
    std::string hex_to_bytes( std::string hex_string )
    {
        std::string bytes;

        hex_string.erase( std::remove_if( hex_string.begin( ), hex_string.end( ), isspace ), hex_string.end( ) );

        for (uint32_t i = 0; i < hex_string.length(); i += 2)
        {
            if ((uint8_t)hex_string[i] == '?')
            {
                bytes += '?';
                i -= 1;

                continue;
            }

            uint8_t byte = (uint8_t)std::strtol(hex_string.substr(i, 2).c_str(), nullptr, 16);
            bytes += byte;
        }

        return bytes;

    }

    uintptr_t scan( const char* pattern, uintptr_t start, uintptr_t end )
    {
        std::string signature = hex_to_bytes( pattern );
        size_t pattern_length = signature.length( );

        uint8_t* base = ( uint8_t* )start;
        uint8_t* last = ( uint8_t* )end;
        uint8_t first = signature.at( 0 );

        for (; base < last; ++base)
        {
            if (*base != first)
                continue;

            uint8_t *bytes = base;
            uint8_t *sig = (uint8_t *)signature.c_str();

            for (; *sig; ++sig, ++bytes)
            {
                if (*sig == '?')
                    continue;

                if (*bytes != *sig)
                    goto end;
            }

            return (uintptr_t)base;

        end:;
        }

        return 0;
    }


    std::vector<PVOID> find_vm_calls( PVOID start )
    {
        std::vector<PVOID> calls;
        constexpr size_t max_len = 0x500;

        uintptr_t start_addr = (uintptr_t)start;
        uintptr_t end_addr = start_addr + max_len;

        uintptr_t vm_call_addr = scan( vm_call_pattern.c_str( ), start_addr, end_addr );
        if( vm_call_addr && !thread_frame_offset )
        {
            uintptr_t preserve_frame_mov = vm_call_addr + 3;
            printf("Preserve frame mov: %p\n", preserve_frame_mov);
            /*
                49:89AF F0030000         | mov qword ptr ds:[r15+3F0],rbp
            */
            thread_frame_offset = *(uint32_t*)(preserve_frame_mov + 3);
            uintptr_t operand_stack_mov = preserve_frame_mov + 7;
            printf("Operand stack mov: %p\n", operand_stack_mov);
            /*
                49:8987 E0030000         | mov qword ptr ds:[r15+3E0],rax
            */
            thread_operand_stack_offset = *(uint32_t*)(operand_stack_mov + 3);
        }

        while (vm_call_addr)
        {
            uintptr_t vm_call_address = scan( vm_call_address_pattern.c_str( ), vm_call_addr, vm_call_addr + 0x50 );
            calls.push_back( *( PVOID *)(vm_call_address + 6) );
            vm_call_addr = scan( vm_call_pattern.c_str( ), vm_call_addr + 1, end_addr );
        }

        return calls;
    }

    uint8_t find_bytecode_start_offset( PVOID interpreter_entry )
    {
        /*
            The bytecode start offset is referenced from the interpreter entry fixed frame generator
            This occurs near the start of the entry

            50                       | push rax                        |
            55                       | push rbp                        | Enter new frame
            48:8BEC                  | mov rbp,rsp                     |
            41:55                    | push r13                        | RBCP register
            68 00000000              | push 0                          | Push 0
            4C:8B6B 10               | mov r13,qword ptr ds:[rbx+10]   | Get ConstMethod
            4D:8D6D 38               | lea r13,qword ptr ds:[r13+38]   | Get Bytecodes offset
            53                       | push rbx                        |
            48:8B53 10               | mov rdx,qword ptr ds:[rbx+10]   |
            48:8B52 08               | mov rdx,qword ptr ds:[rdx+8]    |

            This is pretty consistent and we can scan for it
        */
       std::string fixed_frame_entry = R"(
        50
        55
        48 8B EC
        41 55
        68 00 00 00 00
        4C 8B 6B 10
        )";

        uintptr_t fixed_frame_entry_addr = scan( fixed_frame_entry.c_str( ), (uintptr_t)interpreter_entry, (uintptr_t)interpreter_entry + 0x100 );
        if (!fixed_frame_entry_addr)
            return 0;

        uintptr_t bytecode_offset = fixed_frame_entry_addr + 0x10;
        return *(uint8_t*)(bytecode_offset + 3);
    }
}