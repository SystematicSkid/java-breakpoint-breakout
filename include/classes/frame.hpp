#pragma once
#include <java.hpp>

namespace java
{
    class Frame
    {
    public:
        static Frame* create_frame( uintptr_t frame_address )
        {
            constexpr std::size_t size = offsetof( Frame, link );
            return ( Frame* )( frame_address - size );
        }

        void* initial_sp; // -8
        void* bcp; // -7
        void* cache; // -6
        void* mdp; // -5
        Mirror* mirror; // -4
        java::Method* method; // -3
        void* last_sp; // -2
        uintptr_t sender_sp; // -1;
        uintptr_t link; // 0
        void* return_address; // 1
        
        void debug_print( )
        {
            printf( "Frame: %p\n", this );
            printf( "\tInitial SP: %p\n", initial_sp );
            printf( "\tBCP: %p\n", bcp );
            printf( "\tCache: %p\n", cache );
            printf( "\tMDP: %p\n", mdp );
            printf( "\tMirror: %p\n", mirror );
            printf( "\tMethod: %p\n", method );
            printf( "\tLast SP: %p\n", last_sp );
            printf( "\tSender SP: %p\n", sender_sp );
            printf( "\tLink: %p\n", link );
            printf( "\tReturn Address: %p\n", return_address );
        }

    };
}