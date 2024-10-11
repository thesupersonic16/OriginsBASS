#include "pch.h"
#include "HiteModLoader.h"
#include "IniReader.h"
#include "Helpers.h"
#include "SigScan.h"
#include "mod.hpp"
#include <string>
#include <unordered_map>
#include <chrono>

namespace OriginsBASS {
    RSDKFunctionTable *RSDKTable = nullptr;

    ModLoader* ModLoaderData;
    const char** ModPaths;
    int ModPathCount;

    // Lazy
    std::chrono::high_resolution_clock::time_point lastSeen = std::chrono::high_resolution_clock::now();
    bool32 gamePaused = false;
    bool32 pausedChannels[CHANNEL_COUNT]{};

    struct APIFunctionTable;
    struct GameInfo;
    struct SKUInfo;
    struct SceneInfo;
    struct ControllerState;
    struct AnalogState;
    struct TriggerState;
    struct TouchInfo;
    struct UnknownInfo;
    struct ScreenInfo;

    struct EngineInfo {
        RSDKFunctionTable *RSDKTable;
        APIFunctionTable *APITable;

        GameInfo *gameInfo;
        SKUInfo *currentSKU;
        SceneInfo *sceneInfo;

        ControllerState *controllerInfo;
        AnalogState *stickInfoL;
        AnalogState *stickInfoR;
        TriggerState *triggerInfoL;
        TriggerState *triggerInfoR;
        TouchInfo *touchInfo;

        UnknownInfo *unknownInfo;

        ScreenInfo *screenInfo;

        // only for origins, not technically needed for v5U if standalone I think
        void *hedgehogLink;
    };

    struct AudioInfo {
        int32 loopStart;
        int32 loopEnd;
    };

    std::unordered_map<std::string, AudioInfo> loopReplacements;

    void ParseAllLoopReplacements() {
        for (int i = 0; i < ModPathCount; i++) {
            char iniPath[MAX_PATH];
            sprintf_s(iniPath, "%s\\audio.ini", ModPaths[i]);
            if (GetFileAttributesA(iniPath) != INVALID_FILE_ATTRIBUTES) {
                INIReader ini(iniPath);
                printf("[OriginsBASS] Loading loop info from %s\n", iniPath);
                if (ini.ParseError() == -1) {
                    printf("[OriginsBASS] INI parse error: \"%s\"\n", iniPath);
                    return;
                }
                for (auto& section : ini.Sections()) {
                    auto it = loopReplacements.find(section);
                    if (it != loopReplacements.end()) {
                        it->second.loopStart = ini.GetInteger(section, "loopStart", it->second.loopStart);
                        it->second.loopEnd = ini.GetInteger(section, "loopEnd", it->second.loopEnd);
                    } else {
                        AudioInfo info;
                        info.loopStart = ini.GetInteger(section, "loopStart", -1);
                        info.loopEnd = ini.GetInteger(section, "loopEnd", -1);
                        loopReplacements[section] = info;
                        printf("[OriginsBASS] Added loop info to %s\n", section.c_str());
                    }
                }
            }
        }
    }

    bool32 FindModFile(char* out, uint32 outLen, char* path) {
        for (int i = 0; i < ModPathCount; i++) {
            char fn2[MAX_PATH];
            snprintf(fn2, MAX_PATH, "%s\\%s", ModPaths[i], path);
            if (GetFileAttributesA(fn2) != INVALID_FILE_ATTRIBUTES) {
                strncpy(out, fn2, outLen);
                return true;
            }
        }
        return false;
    }

    HOOK(uint16, __fastcall, GetSfx, 0x1400DDDF0, const char* path) {
        int16 id = Audio::FindSFX(path);
        return id;
    }

    HOOK(int32, __fastcall, PlaySfx, 0x1400DDEA0, uint16 sfx, int32 loopPoint, int32 priority) {
        Audio::PlaySfx(sfx, loopPoint, priority);
        return sfx;
    }

    HOOK(int32, __fastcall, PlayStream, 0x1400DDEB0, const char* filename, uint32 channel, uint32 startPos, uint32 loopPoint, bool32 loadASync) {
        printf("[OriginsBASS] PlayStream(\"%s\", %u, %u, %u, %u)\n", filename, channel, startPos, loopPoint, loadASync);

        // Legacy passes -1
        if (channel == -1)
            channel = 0;

        if (channel >= CHANNEL_COUNT)
            return -1;

        Audio::AudioChannel *channelEntry = &Audio::channels[channel];

        // I aren't rewriting this
        // Handle speed
        // The code built in S3K for handling the speed is faulty
        bool isTransition   = false;
        std::string path    = std::string(filename);
        bool isFast         = path.find("F/") != std::string::npos;
        bool isSpecialStage = path.find("3K/SpecialStage") != std::string::npos;
        float fastSpeed     = 1.2f;

        if (path.find("3K/") != std::string::npos) {
            if (isFast) {
                path.replace(path.find("F/"), 2, "");
                isTransition = !strcmp(channelEntry->name, path.c_str());
            }
            else {
                path.replace(path.find("3K/"), 3, "3K/F/");
                isTransition = !strcmp(channelEntry->name, path.c_str());
            }

            // Handle blue spheres
            if (isSpecialStage) {
                // 1 = S0, 2 = S1, ...
                int32 speedStage = 0;
                size_t pos = path.find("StageS");
                if (isTransition = isFast = (pos != std::string::npos))
                {
                    int32 speedStep = path.c_str()[pos + 6] - '0' + 1;
                    fastSpeed = speedStep * 0.05f + 1.0f;
                }
            }

            if (!isFast && isTransition)
                channelEntry->streamSpeed = 1.0f;

            if (isTransition)
            {
                float newSpeed = (isFast ? fastSpeed : 1.0f);
                bool speedChanged = channelEntry->streamSpeed != newSpeed;
                if (speedChanged)
                {
                    printf("  Speed change %f -> %f\n", channelEntry->streamSpeed, newSpeed);
                    channelEntry->streamSpeed = newSpeed;
                    BASS_ChannelSetAttribute(channelEntry->basschan, BASS_ATTRIB_TEMPO, (newSpeed - 1.0f) * 100.0f);
                    strcpy_s(channelEntry->name, filename);
                    return channelEntry->basschan ? channel : -1;
                }
            }
        }
        char filePath[MAX_PATH];
        sprintf_s(filePath, "%s\\Data\\Music\\%s", ModLoaderData->GetDataPackName(), filename);
        
        if (FindModFile(filePath, sizeof(filePath), filePath)) {
            auto it = loopReplacements.find(filename);
            if (it != loopReplacements.end())
                Audio::LoadStream(channel, filePath, filename, loopPoint ? it->second.loopStart : 0, it->second.loopEnd);
            else 
                Audio::LoadStream(channel, filePath, filename, loopPoint, -1);
            Audio::PlayChannel(channel, startPos);
        }
        else
            printf("[OriginsBASS] Failed to load file stream \"%s\"\n", filename);
        return channelEntry->basschan ? channel : -1;
    }

    HOOK(void, __fastcall, SetChannelAttributes, 0x1400DDDB0, uint8 channel, float volume, float panning, float speed) {
        Audio::SetChannelAttributes(channel, volume, panning, speed);
    }
    HOOK(void, __fastcall, StopChannel, 0x1400DDF50, uint32 channel) {
        Audio::StopChannel(channel);
    }
    HOOK(void, __fastcall, PauseChannel, 0x1400DDE90, uint32 channel) {
        Audio::PauseChannel(channel);
    }
    HOOK(void, __fastcall, ResumeChannel, 0x1400DDEE0, uint32 channel) {
        Audio::ResumeChannel(channel);
    }

    bool32 ChannelActive(uint32 channel) {
        if (channel >= CHANNEL_COUNT)
            return false;
        else
            return (Audio::channels[channel].state & 0x3F) != Audio::CHANNEL_IDLE;
    }
    uint32 GetChannelPos(uint32 channel) {
        return Audio::GetChannelPos(channel);
    }

    HOOK(void, __fastcall, LoadSfx, 0x1400DDE20, char *filename, uint8 plays, uint8 scope) {
        char filePath[MAX_PATH];
        char basePath[MAX_PATH];
        sprintf_s(basePath, "%s\\Data\\SoundFX\\%s", ModLoaderData->GetDataPackName(), filename);
        
        if (FindModFile(filePath, sizeof(filePath), basePath))
            Audio::LoadSFX(filePath, filename, -1, plays, scope);
    }

    HOOK(void, __fastcall, LoadSfxLegacy, 0x1400DDE80, char *filename, uint8 slot, uint8 plays, uint8 scope) {
        char filePath[MAX_PATH];
        char basePath[MAX_PATH];
        sprintf_s(basePath, "%s\\Data\\SoundFX\\%s", ModLoaderData->GetDataPackName(), filename);
        
        if (FindModFile(filePath, sizeof(filePath), basePath))
            Audio::LoadSFX(filePath, filename, slot, plays, scope);
        else
            printf("[OriginsBASS] Failed to load SFX \"%s\"\n", filename);
    }


    HOOK(void, __fastcall, LinkGameLogicDLL, SigLinkGameLogicDLL(), EngineInfo *info) {
        RSDKTable = info->RSDKTable;

        // Replace functions
        //RSDKTable->GetSfx = GetSfx;
        //RSDKTable->PlaySfx = PlaySfx;
        //RSDKTable->PlayStream = PlayStream;
        //RSDKTable->SetChannelAttributes = SetChannelAttributes;
        //RSDKTable->StopChannel = StopChannel;
        //RSDKTable->PauseChannel = PauseChannel;
        //RSDKTable->ResumeChannel = ResumeChannel;
        RSDKTable->ChannelActive = ChannelActive;
        RSDKTable->GetChannelPos = GetChannelPos;

        Audio::ResetChannels();
        lastSeen = std::chrono::high_resolution_clock::now();
        gamePaused = false;

        originalLinkGameLogicDLL(info);
    }


    extern "C" __declspec(dllexport) void OnFrame() {
        auto& channel = Audio::channels[0];
        
        auto lastSeenCount = ((std::chrono::duration<double, std::milli>)(std::chrono::high_resolution_clock::now() - lastSeen)).count();

        printf("%f\n", lastSeenCount);

        if (!gamePaused && lastSeenCount > 40) {
            gamePaused = true;
            for (int i = 0; i < CHANNEL_COUNT; ++i) {
                pausedChannels[i] = (Audio::channels[i].state & Audio::CHANNEL_PAUSED) == 0;
                if (pausedChannels[i])
                    Audio::PauseChannel(i);
            }
        }
    }
    extern "C" __declspec(dllexport) void OnRsdkFrame() {
        auto& channel = Audio::channels[0];
        if (gamePaused) {
            for (int i = 0; i < CHANNEL_COUNT; ++i)
                if (pausedChannels[i])
                    Audio::ResumeChannel(i);
        }
        gamePaused = false;
        lastSeen = std::chrono::high_resolution_clock::now();
        //printf("LS: %u, LE: %u, S: %u, B: %u\n", channel.loopStart, channel.loopEnd, Audio::GetChannelPos(0), BASS_ChannelGetPosition(channel.basschan, BASS_POS_BYTE));
    }

    extern "C" __declspec(dllexport) void PostInit() {
        INSTALL_HOOK(LinkGameLogicDLL);
        INSTALL_HOOK(LoadSfx);
        INSTALL_HOOK(LoadSfxLegacy);

        INSTALL_HOOK(GetSfx);
        INSTALL_HOOK(PlaySfx);
        // StopSfx
        INSTALL_HOOK(PlayStream);
        INSTALL_HOOK(SetChannelAttributes);
        INSTALL_HOOK(StopChannel);
        INSTALL_HOOK(PauseChannel);
        INSTALL_HOOK(ResumeChannel);

        ParseAllLoopReplacements();
    }

    extern "C" __declspec(dllexport) void Init(ModInfo *modInfo)
    {
        ModLoaderData = modInfo->ModLoader;

        SigLinkGameLogicDLL();

        // Check signatures
        if (!SigValid)
        {
            MessageBoxW(nullptr, L"Signature Scan Failed!\n\nThis usually means there is a conflict or the mod is running on an incompatible game version.", L"Scan Error", MB_ICONERROR);
            return;
        }

        if (!BASS_Init(-1, 44100, 0, nullptr, nullptr))
        {
            MessageBoxW(nullptr, L"BASS failed to initialize!", L"BASS Error", MB_ICONERROR);
            return;
        }

        Audio::ResetChannels();
        ModPathCount = ModLoaderData->GetIncludePaths(nullptr, 0);
        ModPaths = new const char* [ModPathCount];
        ModLoaderData->GetIncludePaths(ModPaths, ModPathCount);
    }
} // namespace OriginsBASS
