# Introduction
In my last [post](https://systemfailu.re/2023/12/25/hooking-java-methods-just-in-time/) I discussed how you could apply hooks to Java methods when they are compiled
by the JIT compiler. One of the obvious requirements of that technique is that the method you want to hook must be compiled by the JIT compiler. I provided
a potential way to force the JIT compiler to compile the method you want to hook. However, if you're aiming for your project to be compatible with multiple
different JVM implementations, you might want to consider a different approach. In this post, I will discuss how you can intercept and breakpoint Java methods 
at runtime without the use of JNI and without the need to force the JIT compiler to compile the method you want to hook. 

# Key Terms
To understand how this technique works, you need to understand a few key terms. If you think you are already an expert, feel free to skip this section.
Feel free to refer back to this section if you get confused later on, as I will be using these terms throughout the post.

**Bytecode** - The Java instruction set, which consists of just over 200 instructions. The bytecode is executed by the interpreter.

**JVM** - Java Virtual Machine. The JVM is a virtual machine that executes Java bytecode. The JVM is responsible for loading classes, verifying bytecode, and handling all execution of Java bytecode.

**Interpreter** - The interpreter is a component of the JVM which is responsible for executing bytecode. The interpreter is made up of small operation handlers which are responsible for executing a single bytecode instruction.

**Runtime** - A collection of JVM methods that are typically called from within the interpreter.

**Breakpoint** - A breakpoint is a special instruction in the Java instruction set that is used to trigger a breakpoint event before continuing normal execution.

**JVMTI** - Java Virtual Machine Tool Interface. The JVMTI is an interface that allows for the creation of tools that can interact with the JVM. The JVMTI is used by profilers, debuggers, and other tools which need to interact with the JVM.

# Java Bytecode Dispatch Table
For the Java interpreter to execute bytecode, it needs to have a mapping of the bytecode to a handler. This is done through the Java dispatch table.

When running in interpreted mode, a method's entry point will point to a pre-defined set of entries depending on the method type. A normal entry will consist of preserving the caller's stack and frame before setting up a new frame from the method being called. Once this is done, the entry will obtain the first bytecode from the method and jump to the dispatch table handler for the bytecode. From there, it is up to each subsequent handler to obtain the next bytecode and jump to the next handler. This is done until the method returns.

To showcase this, I have added the x86-64 handler for the `iadd` instruction below:
```asm
mov eax,dword ptr ss:[rsp]                |
add rsp,8                                 |
mov edx,dword ptr ss:[rsp]                |
add rsp,8                                 |
add eax,edx                               |
movzx ebx,byte ptr ds:[r13+1]             |
inc r13                                   |
mov r10,<jvm.dispatch_table>              |
jmp qword ptr ds:[r10+rbx*8]              |
```
The register allocation in this handler is as follows:

| Register | Description |
|----------|-------------|
| rsp      | The operand stack, used to store the operands for the instruction. |
| r13      | The bytecode instruction pointer. A pointer to the current bytecode instruction, which is incremented after each instruction is executed. |
| ebx      | The next bytecode instruction. This is obtained from the bytecode instruction pointer. |
| r10      | The dispatch table. This is used to obtain the handler for the next bytecode instruction. |

## Obtaining the Dispatch Table
Finding the dispatch table is extremely easy so long as you have a pointer to a method that hasn't been JIT compiled. The dispatch table, as mentioned before, is moved into a register before the interpreter jumps to the first handler. If you have access to the interpreter entry you can very easily scan the instructions for a move and jmp instruction. This will give you a pointer to the dispatch table, which you can then use to find the handler for the bytecode you want to hook.

## Hooking Bytecode Handlers
One thing you may consider, and may sound easy, is to simply hook the handler for some bytecode and then intercept it depending on the method being executed. I tried this some years ago, and while it works great for testing, this is an awful idea for a production environment. The sheer amount of calls that pass through the dispatch table is insane, and you will quickly find that your application is running at a snail's pace.

If you want to hook a bytecode handler anyway, you will need to be knowledgeable of the Java ABI and what registers you need to preserve on your own. You could use the naked hook handler from my JIT post if you wanted, or even use an exception-based hook such as VEH if you don't want to deal with the ABI at all.

You can, however, use certain handlers to obtain information to use in your own attached application, such as method offsets, thread offsets, etc.

# Java Breakpoints
When writing a Java application using your favorite IDE, you almost always have the option of running that application within the IDE and debugging it using breakpoints, field watches, etc. The IDE achieves this by loading a JNI agent at the start of the JVM, which tells the JVM that it can place breakpoints.
When running outside of an IDE, you can still start the application with a debugging JNI agent, unless the application has explicitly disabled agent attachments. 

Having agent attachment disabled is a common practice for Java applications that are intended to be run in production environments. 

## Breakpoint Logic
The breakpoint instruction, denoted by the opcode `0xCA`, is a single-byte instruction which performs 3 actions:
1. Obtains the original bytecode at the current instruction pointer
2. Executes the breakpoint handler from the JVM runtime
3. Executes the original instruction and continues normal execution

For your convenience, I have included the x86 assembly for the breakpoint instruction below along with some comments. Keep in mind r15 is used to store the thread and rbp is used to store the current frame:
```asm
mov rdx,qword ptr ss:[rbp-18]             |
call 21AC31ACDB8                          |
jmp 21AC31ACE53                           |
mov r8,r13                                |
lea rax,qword ptr ss:[rsp+8]              |
mov qword ptr ss:[rbp-40],r13             |
mov rcx,r15                               |
vzeroupper                                |
mov qword ptr ds:[r15+2A0],rbp            | Preserve java frame
mov qword ptr ds:[r15+290],rax            | Preserve operand stack
sub rsp,20                                |
test esp,F                                |
je 21AC31ACE02                            |
sub rsp,8                                 |
mov r10,<jvm.get_original_bytecode>       |
call r10                                  |
add rsp,8                                 |
jmp 21AC31ACE0F                           |
mov r10,<jvm.get_original_bytecode>       |
call r10                                  |
add rsp,20                                |
mov qword ptr ds:[r15+290],0              |
mov qword ptr ds:[r15+2A0],0              |
mov qword ptr ds:[r15+298],0              |
vzeroupper                                |
cmp qword ptr ds:[r15+8],0                |
je 21AC31ACE4A                            |
jmp 21AC3190F00                           |
mov r13,qword ptr ss:[rbp-40]             |
mov r14,qword ptr ss:[rbp-38]             |
ret                                       |
mov rbx,rax                               |
mov rdx,qword ptr ss:[rbp-18]             |
call 21AC31ACE64                          |
jmp 21AC31ACEFF                           |
mov r8,r13                                |
lea rax,qword ptr ss:[rsp+8]              |
mov qword ptr ss:[rbp-40],r13             |
mov rcx,r15                               |
vzeroupper                                |
mov qword ptr ds:[r15+2A0],rbp            | Preserve java frame
mov qword ptr ds:[r15+290],rax            | Preserve operand stack
sub rsp,20                                |
test esp,F                                |
je 21AC31ACEAE                            |
sub rsp,8                                 |
mov r10,<jvm.breakpoint_handler>          |
call r10                                  |
add rsp,8                                 |
jmp 21AC31ACEBB                           |
mov r10,<jvm.breakpoint_handler>          |
call r10                                  |
add rsp,20                                |
mov qword ptr ds:[r15+290],0              |
mov qword ptr ds:[r15+2A0],0              |
mov qword ptr ds:[r15+298],0              |
vzeroupper                                |
cmp qword ptr ds:[r15+8],0                |
je 21AC31ACEF6                            |
jmp 21AC3190F00                           |
mov r13,qword ptr ss:[rbp-40]             |
mov r14,qword ptr ss:[rbp-38]             |
ret                                       |
mov r10,jvm.7FFD15B13690                  |
jmp qword ptr ds:[r10+rbx*8]              | Execute original bytecode
```

When using JVMTI to place a breakpoint, there are quite a few actions taken by the JVM, including storing the original bytecode and storing the callback function to be executed when the breakpoint is hit. This is all under the assumption that the JVM allows for agent attachment. However, if agent attachment is disabled, the JVM will refuse to run any exported breakpoint methods from the JVMTI.

# VM Calls
To call methods exposed from the Java Runtime, the interpreter needs to preserve the current frame and operand stack, and then call the method. Since a Java thread can only execute one method at a time, the frame and stack are preserved on the current thread. The instruction handler which is attempting to make the VM call then checks the current stack's alignment and aligns it if necessary. The instruction handler then calls the VM method and restores the frame and stack after the VM call has completed.

I have noticed, in some JVM distributions, that the interpreter will push all registers onto the stack after a vm call and call `Runtime::CurrentThread`, checking the result against `r15`. If the result is not equal to `r15`, the JVM will throw an exception and close.

All VM calls follow the same pattern, except for the chance of some registers being different. The pattern is as follows:
```asm
mov rcx,r15                               | Store JavaThread into rcx
vzeroupper                                |
mov qword ptr ds:[r15+???],rbp            | Preserve java frame
mov qword ptr ds:[r15+???],rax            | Preserve operand stack
sub rsp,20                                |
test esp,F                                |
je no_align                               |
sub rsp,8                                 |
mov r10,<jvm.breakpoint_handler>          |
call r10                                  |
```

Since the JavaThread is stored in `rcx` immediately before the call, we can use this to access the preserved frame and stack, modifying any of the values we want. This is the key to being able to hook Java methods at runtime.

# Adding Breakpoints
Up until now, I've just discussed how the JVM and interpreter work. So what if you want to start adding your breakpoints? It's extremely easy and boils down to just a few key steps:
1. Place a hook on `Runtime::get_original_bytecode`
2. Place a hook on `Runtime::breakpoint_handler`
3. Write the breakpoint opcode (`0xCA`) to the bytecode instruction pointer you want to breakpoint

There is some effort required to get all of this working smoothly and without the need for manual intervention, but thankfully I have already done the work for you.

As discussed earlier, the dispatch table can be accessed through the interpreter entry of any interpreted Java method. Once you have this pointer, you can access the breakpoint handler. Once you have access to the breakpoint handler, it is simple enough to do a lookup for all VM calls within that handler.
In this case, the two VM calls will be `Runtime::get_original_bytecode` and `Runtime::breakpoint_handler` respectively. Hook these two methods with your favorite hooking technique, and you're good to go. I have included the types for these two methods below:
```cpp
uint8_t original_bytecode_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address );
void breakpoint_handler( java::JavaThread* java_thread, java::Method* method, uintptr_t bytecode_address );
```

It's completely up to you how you want to design these two hooks, but you *must* return the original bytecode from `Runtime::get_original_bytecode`, unless you are spoofing the original bytecode of course.

Writing the breakpoint opcode to the bytecode instruction pointer can be quite simple, but it can also pose some issues if you don't know where the bytecode is.
Thankfully for us, the offset for the `ConstMethod` and the `ConstMethod::bytecode_start` is exposed in the interpreter entry during frame creation:
```asm
push rax                        |
push rbp                        | Enter new frame
mov rbp,rsp                     |
push r13                        | RBCP register
push 0                          | Push 0
mov r13,qword ptr ds:[rbx+10]   | Get ConstMethod
lea r13,qword ptr ds:[r13+38]   | Get Bytecodes offset
push rbx                        |
mov rdx,qword ptr ds:[rbx+10]   |
mov rdx,qword ptr ds:[rdx+8]    |
```
Scanning for this is simple and once you have these offsets, you can easily write to the bytecode instruction pointer.

The execution flow of a breakpoint handler which modifies the operand stack can look like the following:
![Breakpoint Execution Flow](https://i.imgur.com/JHAEoG7.png)

## Removing Breakpoints
Removing breakpoints is easier than adding them. All you need to do is write the original bytecode back to the bytecode instruction pointer. Of course, you need to make sure that the original bytecode is still preserved, but that is a given.

# Considerations
Feel free to browse my code and build it to test around with it. 
If you want to integrate this into your project or target a specific JVM build, you *do* need at least one address. This address is the address of the interpreter entry. You can obtain this by using any sort of reverse engineering tool, or you can use the exposed `VMStructs` class. I won't go into that in this article, but it is fairly easy to do.

# Acknowledgements
I wouldn't be able to accomplish this without the help of a couple of friends and some resources:
- [Java Virtual Machine Tool Interface Docs](https://docs.oracle.com/javase/8/docs/platform/jvmti/jvmti.html)
- [Java Native Interface Docs](https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/jniTOC.html)
- [Java Bytecode Reference](https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-6.html)
  - I can only memorize so many opcodes, so this was a huge help
- [MyriadSoft](https://github.com/MyriadSoft)
  - Continuous help with my assembly code