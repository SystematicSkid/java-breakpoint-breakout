#pragma once
#include <java.hpp>

namespace java
{
    class JavaThread
    {
    public:
        static size_t preserved_sp_offset;
        static size_t preserved_fp_offset;

        uintptr_t* get_operand_stack( )
        {
            if(!preserved_sp_offset)
                throw std::runtime_error( "JavaThread::preserved_sp_offset is not initialized" );
            return *(uintptr_t**)( (uintptr_t)this + preserved_sp_offset );
        }

        java::Frame* get_top_frame( )
        {
            if(!preserved_fp_offset)
                throw std::runtime_error( "JavaThread::preserved_fp_offset is not initialized" );
            return java::Frame::create_frame( *(uintptr_t*)( (uintptr_t)this + preserved_fp_offset ) );
        }
    };
}