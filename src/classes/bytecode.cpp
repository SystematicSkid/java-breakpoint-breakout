#include <classes/bytecode.hpp>
#include <breakpoints/bytecode_info.hpp>
#include <breakpoints/breakpoints.hpp>

namespace java
{

    Bytecodes Bytecode::get_opcode( ) const
    {
        /* Check if this opcode is in the original map */
        if( breakpoints::original_bytecodes.find( (uintptr_t)this->opcode ) != breakpoints::original_bytecodes.end( ) )
        {
            /* Return the original opcode */
            return (Bytecodes)breakpoints::original_bytecodes[ (uintptr_t)this->opcode ];
        }
        else
        {
            /* Return the current opcode */
            return (Bytecodes)*this->opcode;
        }
    }

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
                int npairs = *( int32_t* )( this->opcode + 1 + padding + 4 + 4 );
                length += 8 * npairs;
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
                return breakpoints::bytecode_lengths[ (uint8_t)this->get_opcode( ) ] + 1;
            }
        }
        // Should never happen
        return 0;
    }

    int Bytecode::get_stack_consumption( )
    {
        return breakpoints::bytecode_operand_consumption[ (uint8_t)this->get_opcode( ) ];
    }
}

/* Static bytecode map */
std::map< uint8_t*, uint16_t > java::Bytecode::bytecode_offsets;