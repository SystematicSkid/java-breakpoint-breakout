#include <java.hpp>
#include <breakpoints/breakpoints.hpp>


namespace java
{
    void Method::set_breakpoint( int offset, breakpoints::breakpoint_callback_t callback )
    {
        breakpoints::add_breakpoint( this, offset, callback );
    }

    void Method::remove_breakpoint( int offset )
    {
        breakpoints::remove_breakpoint( this, offset );
    }

    void Method::remove_all_breakpoints( )
    {
        breakpoints::remove_all_breakpoints( this );
    }
}