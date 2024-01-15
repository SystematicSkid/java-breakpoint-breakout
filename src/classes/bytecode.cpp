#include <classes/bytecode.hpp>
#include <breakpoints/bytecode_info.hpp>

namespace java
{

    int Bytecode::get_length( )
    {
        switch( this->get_opcode( ) )
        {
            case Bytecodes::wide:
            {
                /* Get next opcode */
                Bytecodes wide_opcode = (Bytecodes)( opcode[1] );
                if( wide_opcode == Bytecodes::iinc )
                    return 6;
                else
                    return 4;
                break;
            }
            case Bytecodes::lookupswitch:
            {
                /* NOTE: Lookupswitch and tableswitch are currently wrong! */
                /* Get offset of lookup switch */
                int position = this->bytecode_offsets[ this->opcode ];
                int padding = ( 4 - ( position % 4 ) ) % 4;
                int length = 1  /* opcode */
                + padding       /* padding */
                + 4             /* default */
                + 4             /* npairs */;
                + 8 * ( *( int32_t* )( this->opcode + 1 + padding + 4 + 4 ) );
                return length;
                break;
            }
            case Bytecodes::tableswitch:
            {
                int position = this->bytecode_offsets[ this->opcode ];
                int padding = ( 4 - ( position % 4 ) ) % 4;
                int high = *( int32_t* )( this->opcode + 1 + padding + 4 );
                int low = *( int32_t* )( this->opcode + 1 + padding + 4 + 4 );
                int length = 1  /* opcode */
                + padding       /* padding */
                + 4             /* default */
                + 4             /* high */
                + 4             /* low */
                + 4 * ( high - low + 1 );
                return length;
                break;
            }
            default:
            {
                return breakpoints::bytecode_lengths[ *this->opcode ] + 1;
            }
        }
        // Should never happen
        return 0;
    }

    int Bytecode::get_stack_consumption( )
    {
        return breakpoints::bytecode_operand_consumption[ *this->opcode ];
    }
}

/* Static bytecode map */
std::map< uint8_t*, uint16_t > java::Bytecode::bytecode_offsets;