#pragma once
#include <java.hpp>

namespace java
{
    class Mirror
    {
    private:
        uint64_t identifier;
    public:
        uintptr_t instance_mirror;
        InstanceKlass* klass;
    };
}