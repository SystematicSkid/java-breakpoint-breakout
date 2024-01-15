#pragma once

/* STL */
#include <cstdint>
#include <map>
#include <functional>

/* Includes */
#include <breakpoints/bytecode_info.hpp>
#include <breakpoints/breakpoint_info.hpp>

#include <java.hpp>

namespace breakpoints
{
    /* Define breakpoint callback type */
    /* void callback(BreakpointInfo* )*/
    typedef std::function<void( BreakpointInfo* )> breakpoint_callback_t;

    extern std::map< uintptr_t, uint8_t > original_bytecodes;
    extern std::map< uintptr_t, breakpoint_callback_t > breakpoint_callbacks;

    bool add_breakpoint( java::Method* method, int offset, breakpoint_callback_t callback );
    bool remove_breakpoint( java::Method* method, int offset );

    uint8_t original_bytecode_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address );
    void breakpoint_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address );

    bool setup( JavaInterop* interop );
}