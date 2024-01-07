#include <utility/hook.hpp>
#include <hde/hde64.hpp>
#include <memory>
#include <java.hpp>
#include <thread>
#include <intrin.h>
#include <TlHelp32.h>
#include <Psapi.h>

extern "C" {
	int jhook_shellcode_numelems();
	void jhook_shellcode_stub();
	uint8_t* jhook_shellcode_getcode();
    uint64_t jhook_end_shellcode_magic();
}

namespace hook
{   
    std::unordered_map< PVOID, PVOID > original_functions;
    std::map< PVOID, std::uint8_t* > hook_map;

    // Simple RAII wrapper for VirtualProtect.
    struct ScopedVirtualProtect {
        ScopedVirtualProtect(void* Addr, size_t Size, DWORD NewProtect) : Addr(Addr), Size(Size) { VirtualProtect(Addr, Size, NewProtect, &OldProtect); }
	    ~ScopedVirtualProtect() { VirtualProtect(Addr, Size, OldProtect, &OldProtect); }
        
        void* Addr;
        size_t Size;
        DWORD OldProtect;
    };

    // The naked shell has an array of pointers embedded at the start, prior to the first instruction, which are used as arguments in the shellcode
    // This function will set those embedded arguments, in order, to the values passed in the variadic template
    // Note that 'Pointer[4]' does not need to be passed as it's set by the shellcode itself
    /*
        Pointer[0] = Support level of FXSAVE/XSAVE (0 = None, 1 = FXSAVE, 2 = XSAVE)
        Pointer[1] = Buffer to be used for saving/restoring FPU state (nullptr if support level is 0)
        Pointer[2] = Address of callback function
        Pointer[3] = Address of next hook
        Pointer[4] = Saved (potentially unaligned) stack pointer after preserving all registers but prior to alignment
    */
    template<typename... TArgs>
    void jhook_shellcode_setargs(TArgs... args) {
        ScopedVirtualProtect vp(jhook_shellcode_stub, 0x1, PAGE_EXECUTE_READWRITE);

        const auto jhook_setarg = [](uint8_t*& args_addr, const auto& arg) {
            std::memcpy(args_addr, &arg, sizeof(arg));
            args_addr += sizeof(uintptr_t);
        };

        uint8_t* args_addr = reinterpret_cast<uint8_t*>(jhook_shellcode_stub);
        (jhook_setarg(args_addr, args), ...);
    }

   enum CPUID_XSAVE_BITS {
        BITS_FXSAVE = 1 << 24,
        BITS_XSAVE = 1 << 26,
        BITS_OSXSAVE = 1 << 27
    };

    enum XCRO_FEATURE_MASK_BITS {
        XCR0_SSE_BIT = 1 << 1, // Supports XMM registers
        XCR0_AVX_BIT = 1 << 2, // Supports YMM registers
        
        XCR0_AVX_SUPPORT = (XCR0_SSE_BIT | XCR0_AVX_BIT)
    };

   enum XSAVE_SUPPORT_LEVEL {
        XSAVE_NOT_SUPPORTED = 0,
        XSAVE_LEGACY_SSE_ONLY = 1,
        XSAVE_SUPPORTED = 2
    };

    enum XSAVE_ALIGNMENT_SIZE {
        FXSAVE_ALIGNMENT = 16,
        XSAVE_ALIGNMENT = 64
    };

    size_t fxsave_required_size() {
        int cpu_info[4];
        __cpuid(cpu_info, 0x80000001);
        return static_cast<size_t>(cpu_info[3] & 0x30) ? 512 : ((cpu_info[3] & 0x2) ? 256 : ((cpu_info[3] & 0x1) ? 128 : 0));
    }

    size_t xsave_required_size() {
        int cpu_info[4];
        __cpuidex(cpu_info, 0x0D, 0);
        return static_cast<size_t>(cpu_info[1]);
    }

   uint64_t get_xsave_support_level() {
        int cpu_info[4];
        __cpuid(cpu_info, 1);

        bool fxsave_support = cpu_info[2] & BITS_FXSAVE;
        bool osxsave_support = 0;//cpu_info[2] & BITS_OSXSAVE;

        if (osxsave_support) {
            unsigned long long xcr_feature_mask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
            osxsave_support = (xcr_feature_mask & XCR0_AVX_SUPPORT) == XCR0_AVX_SUPPORT;
        }

        return static_cast<uint64_t>(osxsave_support ? XSAVE_SUPPORTED : fxsave_support ? XSAVE_LEGACY_SSE_ONLY : XSAVE_NOT_SUPPORTED);
    }

    size_t get_xsave_state_size(uint64_t xsave_support_level) {
        switch (xsave_support_level) {
        case XSAVE_SUPPORTED:
            return xsave_required_size();
        case XSAVE_LEGACY_SSE_ONLY:
            return fxsave_required_size();
        default:
            return 0ULL;
        }
    }

    void* alloc_xsave_state(uint64_t xsave_support_level, size_t xsave_state_len) {
	    const size_t xsave_state_align = (xsave_support_level == XSAVE_SUPPORTED ? XSAVE_ALIGNMENT : FXSAVE_ALIGNMENT);
	    void* xsave_state_buf = (xsave_support_level != XSAVE_NOT_SUPPORTED ? _aligned_malloc(xsave_state_len, xsave_state_align) : nullptr);

	    /* Zero out the FPU state buffer */
	    if (xsave_state_buf != nullptr)
	    	memset(xsave_state_buf, 0, xsave_state_len);

	    return xsave_state_buf;
    }

    __declspec(noinline) void run_xsave_callback(size_t SupportLevel, size_t SizeRequired, size_t IsRestoring) {
	    if (SupportLevel == XSAVE_NOT_SUPPORTED)
	    	return;

	    thread_local std::unordered_map<void*, void*> xsave_state_bufs { };

	    auto hooked_xsave_buf = xsave_state_bufs.find(_ReturnAddress());

	    if (hooked_xsave_buf == xsave_state_bufs.end())
	    	hooked_xsave_buf = xsave_state_bufs.emplace(_ReturnAddress(), alloc_xsave_state(SupportLevel, SizeRequired)).first;

	    void* xsave_state_buf = hooked_xsave_buf->second;

	    if (SupportLevel == XSAVE_SUPPORTED) {
	    	(IsRestoring ? _xrstor64(xsave_state_buf, -1) : _xsave64(xsave_state_buf, -1));
	    } else if (SupportLevel == XSAVE_LEGACY_SSE_ONLY) {
	    	(IsRestoring ? _fxrstor64(xsave_state_buf) : _fxsave64(xsave_state_buf));
	    }
    } 




    
    __declspec(noinline) void init_shell_args(PVOID callback, PVOID trampoline)
    {
    	/* Get the FXSAVE/XSAVE support level */
    	const uint64_t xsave_support_level = get_xsave_support_level();
    	/* Get the size of the FXSAVE/XSAVE area */
    	const size_t xsave_state_len = get_xsave_state_size(xsave_support_level);
    	/* Set the arguments for the shellcode */
    	jhook_shellcode_setargs(xsave_support_level, xsave_state_len, run_xsave_callback, callback, trampoline);
    } 

    void suspend_all_threads( )
    {
        /* WinAPI get all threads */
        HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
        if( snapshot == INVALID_HANDLE_VALUE )
            return;
        
        /* Get current thread */
        DWORD current_thread = GetCurrentThreadId( );

        /* Iterate over all threads */
        THREADENTRY32 thread_entry;
        thread_entry.dwSize = sizeof( THREADENTRY32 );
        if( Thread32First( snapshot, &thread_entry ) )
        {
            do
            {
                /* If the thread is in our process */
                if( thread_entry.th32OwnerProcessID == GetCurrentProcessId( ) )
                {
                    /* Open the thread */
                    HANDLE thread = OpenThread( THREAD_ALL_ACCESS, FALSE, thread_entry.th32ThreadID );
                    if( thread )
                    {
                        /* Ignore current thread */
                        if( thread_entry.th32ThreadID == current_thread )
                        {
                            printf( "Ignoring current thread\n" );
                            CloseHandle( thread );
                            continue;
                        }
                        /* Suspend the thread */
                        SuspendThread( thread );
                        /* Close the thread */
                        CloseHandle( thread );
                    }
                }
            } while( Thread32Next( snapshot, &thread_entry ) );
        }
    }

    void resume_all_threads( )
    {
        /* WinAPI get all threads */
        HANDLE snapshot = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
        if( snapshot == INVALID_HANDLE_VALUE )
            return;

        /* Iterate over all threads */
        THREADENTRY32 thread_entry;
        thread_entry.dwSize = sizeof( THREADENTRY32 );
        if( Thread32First( snapshot, &thread_entry ) )
        {
            do
            {
                /* If the thread is in our process */
                if( thread_entry.th32OwnerProcessID == GetCurrentProcessId( ) )
                {
                    /* Open the thread */
                    HANDLE thread = OpenThread( THREAD_ALL_ACCESS, FALSE, thread_entry.th32ThreadID );
                    if( thread )
                    {
                        /* Resume the thread */
                        ResumeThread( thread );
                        /* Close the thread */
                        CloseHandle( thread );
                    }
                }
            } while( Thread32Next( snapshot, &thread_entry ) );
        }
    }

    std::size_t get_minimum_shell_size( PVOID target )
    {
        std::size_t size = 0;
        hde64s hs;
        do
        {
            hde64_disasm((void *)((uintptr_t)target + size), &hs);
            size += hs.len;
        } while (size < shell_size);
        return size;
    }

    void construct_shell( std::uint8_t* shell, PVOID target )
    {
        /* Construct a shell */
        shell[ 0 ] = 0x49;
        shell[ 1 ] = 0xBB; // mov r11, addr
        memcpy( shell + 2, &target, sizeof( DWORD64 ) );
        shell[ 10 ] = 0x41;
        shell[ 11 ] = 0xFF;
        shell[ 12 ] = 0xE3; // jmp r11
    }

    void* create_trampoline( PVOID target, std::size_t& size )
    {
        std::size_t len = 0;
        std::vector<std::uint8_t> trampoline;
        /* Iterate over all instructions */
        while ( len < size )
        {
            hde64s hs;
            hde64_disasm( ( void* )( ( uintptr_t )target + len ), &hs );

            /* Copy instruction to trampoline */
            for ( std::size_t i = 0; i < hs.len; i++ )
            {
                trampoline.push_back( *( std::uint8_t* )( ( uintptr_t )target + len + i ) );
            }
            len += hs.len;
        }
        /* Allocate memory for the trampoline */
        PVOID trampoline_address = VirtualAlloc( nullptr, trampoline.size( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE );
        /* Copy trampoline to allocated memory */
        memcpy( trampoline_address, trampoline.data( ), trampoline.size( ) );
        /* Update size */
        size = trampoline.size( );

        return trampoline_address;
    }

    void* create_naked_shell( PVOID callback, PVOID trampoline )
    {
        /* Get number of shellcode arguments and initialize them */
        int naked_shellcode_numargs = jhook_shellcode_numelems();
        init_shell_args( callback, trampoline );
        /* Get address of naked_shell in code */
        uint8_t* naked_shell_address = jhook_shellcode_getcode();
        uint8_t* naked_shell_codeptr = naked_shell_address;
        /* Get magic number marking the end of naked shell in code */
        uint64_t naked_shell_endmagic = jhook_end_shellcode_magic();
        /* Iterate over all bytes in the naked shell until reaching the end */
        std::vector<std::uint8_t> naked_shell_bytes;
        while (*(uint64_t*)naked_shell_codeptr != naked_shell_endmagic)
            naked_shell_bytes.push_back(*naked_shell_codeptr++);

        /* Insert the shellcode arguments at the start */
        printf("Shellcode stub: %p\n", jhook_shellcode_stub);
        printf("Naked shell address: %p\n", naked_shell_address);
        naked_shell_bytes.insert(naked_shell_bytes.begin(), (uint8_t*)jhook_shellcode_stub, (uint8_t*)naked_shell_address);
        /* Allocate new memory for the shell */
        uint8_t* naked_shell_memory = reinterpret_cast<uint8_t*>(VirtualAlloc( nullptr, naked_shell_bytes.size( ), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE ));
        /* Copy the naked shell to the allocated memory */
        memcpy( naked_shell_memory, naked_shell_bytes.data( ), naked_shell_bytes.size( ) );
        /* Return the actual first instruction in the naked shell after the embedded arguments */
        return naked_shell_memory + (sizeof(uintptr_t) * naked_shellcode_numargs);
    }

    bool hook( PVOID original, PVOID hook )
    {
        /* TODO: ADD PROPER ERROR HANDLING FOR PROD. */
        std::size_t length = get_minimum_shell_size(original);
        if(length < shell_size)
        {
            printf("Failed to hook function: %p", original);
            return false;
        }
        // Suspends all threads in the process
        suspend_all_threads( );
        // Copy original bytes
        auto original_bytes = std::make_unique<std::uint8_t[]>(length);
        memcpy(original_bytes.get(), reinterpret_cast<PVOID>(original), length);
        hook_map[original] = original_bytes.get();

        // Create jmp shellcode back to the original function after the hook
        std::uint8_t trampolineShell[shell_size];
        DWORD64 trampolineAddress = (uintptr_t)original + length;
        construct_shell(trampolineShell, (PVOID)trampolineAddress);
        
        // Allocate memory for the trampoline and copy the original bytes
        size_t hookLength = static_cast<size_t>(length);
        std::uint8_t* trampoline = reinterpret_cast<std::uint8_t*>(create_trampoline(original, hookLength));

        /* Create naked shell */
        PVOID naked_shell = create_naked_shell(hook, trampoline);

        // Create jmp shellcode to the callback function
        std::uint8_t callbackShell[shell_size];
        construct_shell(callbackShell, naked_shell);

        // Insert jmp shellcode into trampoline
        memcpy(trampoline + hookLength, trampolineShell, shell_size);
        original_functions[hook] = trampoline;
        printf("Original: %p\n", original);

        // Overwrite original function with jmp shellcode to the callback
        {
            ScopedVirtualProtect vp_orig(original, shell_size, PAGE_EXECUTE_READWRITE);
            memcpy(original, callbackShell, shell_size);
        }

        // Resume all threads in the process
        resume_all_threads( );

        return true;
    }

    bool unhook( PVOID original )
    {
        /* Get the original bytes */
        auto original_bytes = hook_map[ original ];
        /* Write the original bytes to the original pointer */
        memcpy( original, original_bytes, shell_size );
        /* Free the original bytes */
        delete[ ] original_bytes;
        /* Remove the hook from the map */
        hook_map.erase( original );
        /* Remove the original function from the map */
        original_functions.erase( original );
        return true;
    }

    PVOID get_original( PVOID method )
    {
        /* Return the original function */
        return original_functions[ method ];
    }
}