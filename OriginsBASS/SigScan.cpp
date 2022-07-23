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
SIG_SCAN(SigPlayMusic, "\x48\x83\xEC\x38\x4C\x63\xC1\x48\x8D\x05\x00\x00\x00\x00\x4B\x8D\x14\xC0\x80\x3C\xD0\x00\x48\x8D\x0C\xD0\x74\x40\x44\x8B\x49\x44\x8B\x15\x00\x00\x00\x00\x44\x89", "xxxxxxxxxx????xxxxxxxxxxxxxxxxxxxx????xx");
SIG_SCAN(SigStopMusic, "\xCC\x8B\x0D\x00\x00\x00\x00\xE9\x00\x00\x00\x00\xCC", "xxx????x????x");
SIG_SCAN(SigSwapMusicTrack, "\x48\x83\xEC\x38\x4C\x8B\xC1\x48\xC7\xC0\x00\x00\x00\x00\x66\x90", "xxxxxxxxxx????xx");
