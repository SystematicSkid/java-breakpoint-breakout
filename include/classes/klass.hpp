#pragma once
#include <iostream>

namespace java
{
    class InstanceKlass
    {
    private:
        void** vtable;
    public:
        const char* get_internal_name( )
        {
            void* get_internal_name_fn = vtable[ 8 ];
            return ( ( const char* ( * )( InstanceKlass* ) )get_internal_name_fn )( this );
        }

        void dump_class( );
    };
}