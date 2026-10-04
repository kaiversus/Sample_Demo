// Compile (MSVC):   cl /EHsc ManualDebuggerDetection.cpp
// Compile (MinGW):  g++ -o ManualDebuggerDetection.exe ManualDebuggerDetection.cpp

#include <windows.h>
#include <winternl.h>
#include <iostream>

struct MY_PEB
{
    BYTE  Reserved1[2];      // offset 0x00
    BYTE  BeingDebugged;     // offset 0x02  <-- ky thuat 1
    BYTE  Reserved2[5];      // dem toi 0x08
    PVOID Reserved3[2];      // dem toi 0x18
    PVOID ProcessHeap;       // offset 0x18  <-- dung cho ky thuat 3
    BYTE  Reserved4[0x68 - 0x1C]; // dem toi 0x68
    ULONG NtGlobalFlag;      // offset 0x68  <-- ky thuat 2
};

// Struct toi gian mo ta phan dau cua default process heap,
// chi can 2 field: HeapFlags (0x10) va ForceFlags (0x14).
struct MY_HEAP
{
    BYTE  Reserved[0x10];
    ULONG HeapFlags;    // offset 0x10
    ULONG ForceFlags;   // offset 0x14
};

// ---------------------------------------------------------------------
// Ky thuat 1: PEB->BeingDebugged
// Tuong duong asm:
//   mov eax, dword ptr fs:[30h]
//   mov ebx, byte ptr [eax+2]
//   test ebx, ebx
//   jnz BenignCode
// ---------------------------------------------------------------------
bool CheckBeingDebuggedFlag()
{
    PTEB  teb = NtCurrentTeb();                       // fs:[0x18] -> TEB
    MY_PEB* peb = reinterpret_cast<MY_PEB*>(teb->ProcessEnvironmentBlock);

    BYTE beingDebugged = peb->BeingDebugged;           // [eax+2]

    std::cout << "[BeingDebugged] value = " << (int)beingDebugged << "\n";
    return beingDebugged != 0;                         // test + jnz
}

// ---------------------------------------------------------------------
// Ky thuat 2: PEB->NtGlobalFlag
// Tuong duong asm:
//   mov eax, fs:[30h]
//   mov al, [eax+68h]
//   and al, 70h
//   cmp al, 70h
//   je BenignCode
// ---------------------------------------------------------------------
bool CheckNtGlobalFlag()
{
    PTEB  teb = NtCurrentTeb();
    MY_PEB* peb = reinterpret_cast<MY_PEB*>(teb->ProcessEnvironmentBlock);

    const ULONG DEBUG_FLAGS = 0x70; // Heap tail(0x10) | free(0x20) | param(0x40)

    BYTE al = static_cast<BYTE>(peb->NtGlobalFlag) & DEBUG_FLAGS;

    std::cout << "[NtGlobalFlag] AL after AND 0x70 = 0x"
              << std::hex << (int)al << std::dec << "\n";

    return al == DEBUG_FLAGS; // cmp al,70h ; je BenignCode
}

// ---------------------------------------------------------------------
// Ky thuat 3: Process Heap Flags (HeapFlags / ForceFlags)
// Tuong duong asm:
//   mov eax, fs:[30h]
//   mov eax, [eax+18h]   ; PEB->ProcessHeap
//   mov ebx, [eax+10h]   ; Heap->HeapFlags
//   and ebx, 70h
//   cmp ebx, 70h
//   je BenignCode
// ---------------------------------------------------------------------
bool CheckProcessHeapFlags()
{
    PTEB  teb = NtCurrentTeb();
    MY_PEB* peb = reinterpret_cast<MY_PEB*>(teb->ProcessEnvironmentBlock);

    MY_HEAP* heap = reinterpret_cast<MY_HEAP*>(peb->ProcessHeap); // [eax+18h]

    const ULONG DEBUG_FLAGS = 0x70;
    ULONG ebx = heap->HeapFlags & DEBUG_FLAGS;                    // [eax+10h]

    std::cout << "[HeapFlags] value after AND 0x70 = 0x"
              << std::hex << ebx << std::dec << "\n";

    return ebx == DEBUG_FLAGS; // cmp ebx,70h ; je BenignCode
}

int main()
{
#ifndef _WIN32
    std::cout << "Chi chay duoc tren Windows (can PEB/TEB).\n";
    return 1;
#endif

    std::cout << "=== Manual Debugger Detection Demo ===\n\n";

    bool beingDebugged = CheckBeingDebuggedFlag();
    std::cout << "  -> " << (beingDebugged ? "Debugger phat hien (BeingDebugged=1)"
                                            : "Khong phat hien qua BeingDebugged")
              << "\n\n";

    bool ntGlobalFlag = CheckNtGlobalFlag();
    std::cout << "  -> " << (ntGlobalFlag ? "Debugger phat hien (NtGlobalFlag)"
                                          : "Khong phat hien qua NtGlobalFlag")
              << "\n\n";

    bool heapFlags = CheckProcessHeapFlags();
    std::cout << "  -> " << (heapFlags ? "Debugger phat hien (HeapFlags)"
                                       : "Khong phat hien qua HeapFlags")
              << "\n\n";

    if (beingDebugged || ntGlobalFlag || heapFlags)
        std::cout << "KET LUAN: Tien trinh dang bi debug.\n";
    else
        std::cout << "KET LUAN: Khong phat hien debugger (chay binh thuong).\n";

    return 0;
}