#include <java.hpp>

namespace java
{
    struct ConstantPoolReconstituter
    {
        int constant_pool_size;
        void* symmap;
        void* classmap;
        struct
        {
            void* obj;
            void* handle;
        }cpool;
        java::InstanceKlass* ik;
        int error;

        ConstantPoolReconstituter(java::InstanceKlass* ik)
        {
            //symmap = malloc(0x1000);
            //classmap = malloc(0x1000);
            uintptr_t addr = (uintptr_t)GetModuleHandleA("jvm.dll") + 0x9FD120;
            reinterpret_cast<void(__fastcall*)(ConstantPoolReconstituter*, java::InstanceKlass*)>(addr)(this, ik);
        }
        
    };

    struct ClassFileReconstituter : public ConstantPoolReconstituter
    {
        size_t buffer_size;
        uint8_t* buffer;
        uint8_t* buffer_ptr;
        java::JavaThread* thread;

        ClassFileReconstituter(java::InstanceKlass* ik) : ConstantPoolReconstituter(ik)
        {
            buffer_size = 0x4000;
            buffer = (uint8_t*)malloc(buffer_size);
            buffer_ptr = buffer;
            thread = java::JavaThread::current();
        }

        ~ClassFileReconstituter( )
        {
            free(buffer);
        }
    };

    void InstanceKlass::dump_class( )
    {
        ClassFileReconstituter reconstituter(this);
        uintptr_t write_addr = (uintptr_t)GetModuleHandleA("jvm.dll") + 0x9FE850;
        reinterpret_cast<void(__fastcall*)(ClassFileReconstituter*)>(write_addr)(&reconstituter);
        /* Write all bytes in buffer */
        for(size_t i = 0; i < reconstituter.buffer_ptr - reconstituter.buffer; i++)
        {
            std::cout << std::hex << (int)reconstituter.buffer[i] << " ";
        }
        std::cout << std::endl;

        free(reconstituter.buffer);
    }
}