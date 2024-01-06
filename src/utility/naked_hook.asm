; Pointer[0] = Support level of FXSAVE/XSAVE (0 = None, 1 = FXSAVE, 2 = XSAVE)
; Pointer[1] = Buffer to be used for saving/restoring FPU state (nullptr if support level is 0)
; Pointer[2] = Address of callback function
; Pointer[3] = Address of next hook
; Pointer[4] = Saved (potentially unaligned) stack pointer after preserving all registers but prior to alignment
SHELL_PTR_ELEMS EQU 5

; Magic value embedded at the end of the naked shell to easily determine its size
SHELL_ENDCODE_MAGIC EQU 02BAD4B0BBAADBABEh

; Size of a pointer
PTR_SIZE EQU SIZEOF QWORD

; Offset of each embedded argument
ARG_FPUSAVE_SUPPORT     EQU 0 * PTR_SIZE
ARG_FPUSAVE_BUFFER      EQU 1 * PTR_SIZE
ARG_CALLHOOK            EQU 2 * PTR_SIZE
ARG_CALLORIG            EQU 3 * PTR_SIZE
ARG_SAVED_RSP           EQU 4 * PTR_SIZE

; Constants used by the preserve_fpu_state macro to determine whether the FPU state should be saved or restored
FPU_STATE_SAVING EQU 0
FPU_STATE_RESTORING EQU 1

; Macro for preserving all general purpose registers
save_cpu_state_gpr MACRO
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    pushfq
ENDM

; Macro for restoring all general purpose registers
restore_cpu_state_gpr MACRO
    popfq
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rbp
	pop rdi
	pop rsi
	pop rdx
	pop rcx
	pop rbx
	pop rax
ENDM

COMMENT @ FXSAVE
https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-1-manual.pdf#G11.53889
 ; Macros for saving/restoring the FPU state using the FXSAVE/FXRSTOR instructions.
 ; The buffer must be 16-byte aligned.
 ; See Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume I, Chapter 10.5 (linked above) for more information.
@
save_fpu_state_fxsave MACRO
    push r15
    mov r15, qword ptr [args + ARG_FPUSAVE_BUFFER]
    fxsave64 qword ptr [r15]
    pop r15
ENDM

restore_fpu_state_fxsave MACRO
	push r15
	mov r15, qword ptr [args + ARG_FPUSAVE_BUFFER]
	fxrstor64 qword ptr [r15]
	pop r15
ENDM

COMMENT @ XSAVE
https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-1-manual.pdf#G14.51762
 ; Macros for saving/restoring the FPU state using the XSAVE/XRSTOR instructions.
 ; The buffer must be 64-byte aligned.
 ; See Intel® 64 and IA-32 Architectures Software Developer’s Manual, Volume I, Chapter 13 (linked above) for more information.
@
start_fpu_state_xsave MACRO
    push rax
    push rdx
    mov rax, -1
    mov rdx, -1

    push r15
    mov r15, qword ptr [args + ARG_FPUSAVE_BUFFER] 
ENDM

end_fpu_state_xsave MACRO
	pop r15
	pop rdx
	pop rax
ENDM

save_fpu_state_xsave MACRO
    start_fpu_state_xsave
    xsave64 qword ptr [r15]
    end_fpu_state_xsave
ENDM

restore_fpu_state_xsave MACRO
	start_fpu_state_xsave
	xrstor64 qword ptr [r15]
	end_fpu_state_xsave
ENDM

; Macro for simplifying the use of saving and restoring the FPU state
;   Arg: Constant 0/1 to indicate whether to save or restore the FPU state
preserve_fpu_state MACRO restoring
    ; Make sure all the labels are local
    Local fpu_preserve_fxsave, fpu_preserve_xsave, fpu_preserve_end

    pushfq

    ; Check CPU support for FXSAVE/XSAVE
    cmp qword ptr [args + ARG_FPUSAVE_SUPPORT], 1

    jl fpu_preserve_end ;       0 = No CPU support for preserving FPU state
    je fpu_preserve_fxsave ;    1 = The CPU supports FXSAVE
    jg fpu_preserve_xsave ;     2 = The CPU supports XSAVE

fpu_preserve_fxsave:
    IF restoring EQ 1
		restore_fpu_state_fxsave
	ELSE
		save_fpu_state_fxsave
    ENDIF

	jmp fpu_preserve_end

fpu_preserve_xsave:
    IF restoring EQ 1
        restore_fpu_state_xsave
	ELSE
        save_fpu_state_xsave
	ENDIF

fpu_preserve_end:
    popfq
ENDM

.CODE

; This is a simple wrapper function to access the magic number marking the end of the shellcode stub.
jhook_end_shellcode_magic PROC
    mov rax, SHELL_ENDCODE_MAGIC
    ret
jhook_end_shellcode_magic ENDP

; This is a simple wrapper function to access the constant SHELL_PTR_ELEMS.
jhook_shellcode_numelems PROC
	mov eax, SHELL_PTR_ELEMS
	ret
jhook_shellcode_numelems ENDP

; This is a simple wrapper to get the base function address for the first instruction of the shellcode.
jhook_shellcode_getcode PROC
	mov rax, jhook_shellcode_stub
    lea rax, [rax + SHELL_PTR_ELEMS * PTR_SIZE]
	ret
jhook_shellcode_getcode ENDP

; Naked function, so no prologue or epilogue generated by the compiler
; NOTE: Do not remove the ALIGN directive
ALIGN 16
jhook_shellcode_stub PROC
    ; Dynamic array of values used by the shellcode.
	args QWORD SHELL_PTR_ELEMS DUP(0)

    ; Save FPU state
    preserve_fpu_state FPU_STATE_SAVING

    ; Save all general purpose registers/flags
    save_cpu_state_gpr

    ; Push the 'skip_original_call' argument for the callback function (default to 0)
    push rax
    mov qword ptr [rsp], 0

    ; Set context pointer with original registers/flags as the first argument to the callback
    mov rcx, rsp

    ; Save (possibly unaligned) stack pointer
    mov qword ptr [args + ARG_SAVED_RSP], rsp

    ; Allocate shadow space for spilled registers
    ; https://github.com/simon-whitehead/assembly-fun/blob/master/windows-x64/README.md#shadow-space
    sub rsp, 28h

    ; Make sure the stack is 16-byte aligned
    and rsp, -16

    ; Invoke callback function
    ; Note that the preserved registers are passed as a context argument and may be freely modified
    ; Any modified registers will be reflected after restoring the CPU state below
    call qword ptr [args + ARG_CALLHOOK]

    ; Restore saved (possibly unaligned) stack pointer
    mov rsp, qword ptr [args + ARG_SAVED_RSP]

    ; Restore set_original_call argument
    pop qword ptr [args + ARG_CALLORIG]
    
    ; Restore all general purpose registers/flags
    restore_cpu_state_gpr

    ; Adjust for the additional skip_original_call argument
    add rsp, PTR_SIZE

    ; Restore FPU statez
    preserve_fpu_state FPU_STATE_RESTORING

    ; Check if skip_original_call was set by the callback to determine where to go next
    cmp qword ptr [rsp - PTR_SIZE], 1
    je skip_original_call

    ; Branch back to the original function (skip_original_call == 0)
    jmp qword ptr [args + ARG_CALLORIG]
    
    ; Skip branching back to the original call trampoline (skip_original_call == 1)
skip_original_call:
    ret

end_shellcode:
    qword SHELL_ENDCODE_MAGIC

jhook_shellcode_stub ENDP

END