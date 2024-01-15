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
            printf( "Frame: %llx\n", this );
            printf( "\tInitial SP: %llx\n", initial_sp );
            printf( "\tBCP: %llx\n", bcp );
            printf( "\tCache: %llx\n", cache );
            printf( "\tMDP: %llx\n", mdp );
            printf( "\tMirror: %llx\n", mirror );
            printf( "\tMethod: %llx\n", method );
            printf( "\tLast SP: %llx\n", last_sp );
            printf( "\tSender SP: %llx\n", sender_sp );
            printf( "\tLink: %llx\n", link );
            printf( "\tReturn Address: %llx\n", return_address );
        }

    };
}