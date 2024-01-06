#pragma once
#include <java.hpp>

namespace java
{
    class Frame
    {
    public:
        void* return_address; // 1
        void* pc_return_address; // 0
        void* sender_sp; // -1;
        void* last_sp; // -2
        void* method; // -3
        void* mirror; // -4
        void* mdp; // -5
        void* cache; // -6
        void* bcp; // -7
        void* initial_sp; // -8
    };
}