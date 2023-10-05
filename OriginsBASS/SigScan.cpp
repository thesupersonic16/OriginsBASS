#include "pch.h"
#include "SigScan.h"
#include <Psapi.h>
#define _countof(a) (sizeof(a)/sizeof(*(a)))

bool SigValid = true;

MODULEINFO moduleInfo;

const MODULEINFO& getModuleInfo()
{
    if (moduleInfo.SizeOfImage)
        return moduleInfo;

    ZeroMemory(&moduleInfo, sizeof(MODULEINFO));
    GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &moduleInfo, sizeof(MODULEINFO));

    return moduleInfo;
}

void* sigScan(const char* signature, const char* mask)
{
    const MODULEINFO& moduleInfo = getModuleInfo();
    const size_t length = strlen(mask);

    for (size_t i = 0; i < moduleInfo.SizeOfImage; i++)
    {
        char* memory = (char*)moduleInfo.lpBaseOfDll + i;

        size_t j;
        for (j = 0; j < length; j++)
            if (mask[j] != '?' && signature[j] != memory[j])
                break;

        if (j == length)
            return memory;
    }

    return nullptr;
}

#define SIG_SCAN(x, ...) \
    void* x##Addr; \
    void* x() \
    { \
        static const char* x##Data[] = { __VA_ARGS__ }; \
        if (!x##Addr) \
        { \
            for (int i = 0; i < _countof(x##Data); i += 2) \
            { \
                x##Addr = sigScan(x##Data[i], x##Data[i + 1]); \
                if (x##Addr) \
                    return x##Addr; \
                } \
            SigValid = false; \
        } \
        return x##Addr; \
    }

// Scans
SIG_SCAN(SigPlayStream, "\x48\x83\xEC\x28\x41\x8B\xD1\xE8\x00\x00\x00\x00\xB8\x00\x00\x00\x00\x48\x83\xC4\x28\xC3", "xxxxxxxx????x????xxxxx");
SIG_SCAN(SigSetChannelAttributes, "\xF3\x0F\x59\x0D\x00\x00\x00\x00\x8B\xC1\xF3\x0F\x2C\xC9\x85\xC0\x78\x0E\x66\x0F\x6E\xC9\x8B\xC8\x0F\x5B\xC9\xE9\x00\x00\x00\x00", "xxxx????xxxxxxxxxxxxxxxxxxxx????");
// TODO: Will need to look into sig scanning these
// 1400DDF50 StopChannel
// 1400DDE90 PauseChannel
// 1400DDEE0 ResumeChannel
