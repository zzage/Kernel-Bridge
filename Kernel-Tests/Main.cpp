#include "pch.h"

#include "WdkTypes.h"
#include "CtlTypes.h"
#include "FltTypes.h"
#include "User-Bridge.h"
#include "Rtl-Bridge.h"

#include <fltUser.h>
#include "CommPort.h"
#include "Flt-Bridge.h"

#include "Kernel-Tests.h"

#include <vector>
#include <string>
#include <iostream>

#define _NO_CVCONST_H
#include <dbghelp.h>
#include "SymParser.h"

#include <winnt.h>
#include <winternl.h>
#include <cstdarg>
#include "../Kernel-Bridge/API/StringsAPI.h"

void RunTests() {
    BeeperTest tBeeper(L"Beeper");
    IoplTest tIopl(L"IOPL");
    VirtualMemoryTest tVirtualMemory(L"VirtualMemory");
    MdlTest tMdl(L"Mdl");
    PhysicalMemoryTest tPhysicalMemory(L"PhysicalMemory");
    ProcessesTest tProcesses(L"Processes");
    ShellTest tShell(L"Shells");
    StuffTest tStuff(L"Stuff");
}

class FilesReader final {
private:
    PVOID Buffer;
    ULONG Size;
public:
    FilesReader(LPCWSTR FilePath) {
        HANDLE hFile = CreateFile(FilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) throw std::runtime_error("File not found!");
        ULONG Size = GetFileSize(hFile, NULL);
        BOOL Status = FALSE;
        if (Size) {
            Buffer = VirtualAlloc(NULL, Size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (!Buffer) {
                CloseHandle(hFile);
                throw std::runtime_error("Memory not allocated!");
            }
            Status = ReadFile(hFile, Buffer, Size, &Size, NULL);
        }
        CloseHandle(hFile);
        if (!Status) throw std::runtime_error("Reading failure!");
    }
    ~FilesReader() {
        if (Buffer) VirtualFree(Buffer, 0, MEM_RELEASE);
    }
    PVOID Get() const { return Buffer; }
    ULONG GetSize() const { return Size; }
};

void MapDriver(LPCWSTR Path) {
    FilesReader DriverImage(Path);
    KbRtl::KbMapDriver(DriverImage.Get(), L"\\Driver\\EnjoyTheRing0");
}

VOID WINAPI Thread(PVOID Arg)  {
    while (true);
}

#define TEST_CALLBACKS

class Stopwatch {
    LARGE_INTEGER StartTime, Frequency;
public:
    Stopwatch() {
        QueryPerformanceFrequency(&Frequency);
        Start();
    }
    ~Stopwatch() = default;
    void Start() {
        QueryPerformanceCounter(&StartTime);
    }
    float Stop() {
        LARGE_INTEGER StopTime;
        QueryPerformanceCounter(&StopTime);
        return static_cast<float>(StopTime.QuadPart - StartTime.QuadPart) / static_cast<float>(Frequency.QuadPart);
    }
};

int main() {
    KbLoader::KbUnload();
    if (KbLoader::KbLoadAsFilter(
        L"C:\\Temp\\Kernel-Bridge\\Kernel-Bridge.sys",
        L"260000" // Altitude of minifilter
    )) {
#ifdef TEST_CALLBACKS
        BOOL Status = TRUE;

        CommPortListener<KB_FLT_POST_READ_INFO, KbFltPostRead> ReadFilter;
        Status = ReadFilter.Subscribe([](CommPort& Port, MessagePacket<KB_FLT_POST_READ_INFO>& Message) -> VOID {
            auto Data = static_cast<PKB_FLT_POST_READ_INFO>(Message.GetData());
            if (wcsstr(Data->Path, L".prot") && Data->Size && Data->Size <= 4096) {
                if (Data->Mdl) {
                    printf("[READ]: Mdl: 0x%p\r\n", reinterpret_cast<PVOID>(Data->Mdl));
                    WdkTypes::PVOID Mapping = NULL;
                    BOOL MappingStatus = Mdl::KbMapMdl(&Mapping, Data->ProcessId, 0, Data->Mdl);
                    if (MappingStatus) {
                        printf("[READ]: Successfully mapped!\r\n");
                        try {
                            PCHAR Buf = reinterpret_cast<PCHAR>(Mapping);
                            printf("[READ]: %s\r\n", Buf);
                        } catch (...) {
                            printf("[READ]: Exception catched!\r\n");
                        }
                        Mdl::KbUnmapMdl(Data->Mdl, Mapping);
                    }
                }
            }
            ReplyPacket<KB_FLT_POST_READ_INFO> Reply(Message, ERROR_SUCCESS, *Data);
            Port.Reply(Reply);
        });


        CommPortListener<KB_FLT_PRE_WRITE_INFO, KbFltPreWrite> WriteFilter;
        Status = WriteFilter.Subscribe([](CommPort& Port, MessagePacket<KB_FLT_PRE_WRITE_INFO>& Message) -> VOID {
            auto Data = static_cast<PKB_FLT_PRE_WRITE_INFO>(Message.GetData());
            if (wcsstr(Data->Path, L".prot") && Data->Size && Data->Size <= 4096) {
                if (Data->Mdl) {
                    printf("[WRITE]: Mdl: 0x%p\r\n", reinterpret_cast<PVOID>(Data->Mdl));
                    WdkTypes::PVOID Mapping = NULL;
                    BOOL MappingStatus = Mdl::KbMapMdl(&Mapping, Data->ProcessId, 0, Data->Mdl);
                    if (MappingStatus) {
                        printf("[WRITE]: Successfully mapped!\r\n");
                        try {
                            PCHAR Buf = reinterpret_cast<PCHAR>(Mapping);
                            printf("[WRITE]: %s\r\n", Buf);
                        } catch (...) {
                            printf("[WRITE]: Exception catched!\r\n");
                        }
                        Mdl::KbUnmapMdl(Data->Mdl, Mapping);
                    }
                }
            }
            ReplyPacket<KB_FLT_PRE_WRITE_INFO> Reply(Message, ERROR_SUCCESS, *Data);
            Port.Reply(Reply);
        });
        if (!Status) std::cout << "Callbacks failure!" << std::endl;
        while (true) Sleep(100);
#endif

        RunTests();
        KbLoader::KbUnload();
    } else {
        std::wcout << L"Unable to load driver!" << std::endl;
    }

    std::wcout << L"Press any key to exit..." << std::endl;
    std::cin.get();
}