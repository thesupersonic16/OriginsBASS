#include "pch.h"
#include "HiteModLoader.h"
#include "Helpers.h"
#include "SigScan.h"
#include "mod.hpp"


namespace OriginsBASS {
	RSDKFunctionTable *RSDKTable = nullptr;

	ModLoader* ModLoaderData;
	const char** ModPaths;
	int ModPathCount;
	Channel Channels[0x10];

	float BlueSphereTempos[] = { 5.0f, 10.0f, 15.0f, 20.0f };

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

	uint16 GetSfx(const char* path) {
		int16 id = Audio::FindSFX(path);

		if (id == -1) {
			char filePath[MAX_PATH];
			char basePath[MAX_PATH];
			sprintf_s(basePath, "%s\\Data\\SoundFX\\%s", ModLoaderData->GetDataPackName(), path);
		
			if (FindModFile(filePath, sizeof(filePath), basePath))
				return Audio::LoadSFX(filePath, path, -1, SCOPE_GLOBAL);
		}
		return id;
	}

	int32 PlaySfx(uint16 sfx, int32 loopPoint, int32 priority) {
		Audio::PlaySfx(sfx, loopPoint, priority);
		return sfx;
	}

	int32 PlayStream(const char* filename, uint32 channel, uint32 startPos, uint32 loopPoint, bool32 loadASync) {
		RSDKTable->PrintLog(PRINT_NORMAL, "[OriginsBASS] PlayStream(\"%s\", %u, %u, %u, %u)", filename, channel, startPos, loopPoint, loadASync);
		if (channel >= CHANNEL_COUNT)
			return -1;

		Audio::AudioChannel* chan = &Audio::channels[channel];
		bool speedup = false;
		const char* name = filename;
		char origname[MAX_PATH];
		const char* tmp = strstr(name, "_F.ogg");
		if (tmp)
		{
			strcpy(origname, name);
			strcpy(origname + (tmp - name), ".ogg");
			speedup = true;
			name = origname;
		}
		else
		{
			tmp = strstr(name, "/F/");
			if (tmp)
			{
				strcpy(origname, name);
				strcpy(origname + (tmp - name), tmp + 2);
				speedup = true;
				name = origname;
			}
		}

		// Blue Spheres
		if (!strcmp(chan->currentmusic, "3K/SpecialStage.ogg") 
			&& strstr(filename, "3K/SpecialStageS"))
		{
			tmp = strstr(filename, ".ogg");
			if (tmp)
			{
				int stage = *(tmp - 1) - '0';
				printf("[OriginsBASS] Changing blue sphere tempo to stage %u\n", stage);
				BASS_ChannelSetAttribute(chan->basschan, BASS_ATTRIB_TEMPO, BlueSphereTempos[stage]);
				return channel;
			}
		}

		if (!strcmp(chan->currentmusic, name))
		{
			if (speedup)
			{
				BASS_ChannelSetAttribute(chan->basschan, BASS_ATTRIB_TEMPO, 20.0f);
				chan->isfast = true;
				return channel;
			}
			else if (chan->isfast)
			{
				BASS_ChannelSetAttribute(chan->basschan, BASS_ATTRIB_TEMPO, 0);
				chan->isfast = false;
				return channel;
			}
		}
		char fnbase[MAX_PATH];
		snprintf(fnbase, MAX_PATH, "%s\\Data\\Music\\%s", ModLoaderData->GetDataPackName(), name);
		PathRemoveExtensionA(fnbase);
		for (tmp = strchr(fnbase, '/'); tmp; tmp = strchr(tmp, '/'))
			*(char*)tmp = '\\';
		for (int i = 0; i < ModPathCount; i++)
		{
			bool found = false;
			char fn2[MAX_PATH];
			snprintf(fn2, MAX_PATH, "%s\\%s.*", ModPaths[i], fnbase);
			WIN32_FIND_DATAA finddata;
			HANDLE hFind = FindFirstFileA(fn2, &finddata);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				found = true;
				PathRemoveFileSpecA(fn2);
				PathAppendA(fn2, finddata.cFileName);
				FindClose(hFind);
			}
			if (found)
			{
				Audio::LoadStream(channel, fn2, loopPoint);
				Audio::PlayChannel(channel, startPos);
				return channel;
			}
		}
		printf("[OriginsBASS] Failed to load file stream \"%s\"\n", name);
		// Don't play anything if it fails
		return -1;
	}
	void SetChannelAttributes(uint8 channel, float volume, float panning, float speed) {
		Audio::SetChannelAttributes(channel, volume, panning, speed);
	}
	void StopChannel(uint32 channel) {
		Audio::StopChannel(channel);
	}
	void PauseChannel(uint32 channel) {
		Audio::PauseChannel(channel);
	}
	void ResumeChannel(uint32 channel) {
		Audio::ResumeChannel(channel);
	}

	bool32 ChannelActive(uint32 channel)
	{
		if (channel >= CHANNEL_COUNT)
			return false;
		else
			return (Audio::channels[channel].state & 0x3F) != Audio::CHANNEL_IDLE;
	}


	HOOK(void, __fastcall, LinkGameLogicDLL, SigLinkGameLogicDLL(), EngineInfo *info) {
		RSDKTable = info->RSDKTable;

		// Replace functions
		RSDKTable->GetSfx = GetSfx;
		RSDKTable->PlaySfx = PlaySfx;
		RSDKTable->PlayStream = PlayStream;
        RSDKTable->SetChannelAttributes = SetChannelAttributes;
        RSDKTable->StopChannel = StopChannel;
        RSDKTable->PauseChannel = PauseChannel;
		RSDKTable->ResumeChannel = ResumeChannel;
		RSDKTable->ChannelActive = ChannelActive;

		Audio::ResetChannels();

		originalLinkGameLogicDLL(info);
	}

	extern "C" __declspec(dllexport) void PostInit() {
		INSTALL_HOOK(LinkGameLogicDLL);
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

		ModPathCount = ModLoaderData->GetIncludePaths(nullptr, 0);
		ModPaths = new const char* [ModPathCount];
		ModLoaderData->GetIncludePaths(ModPaths, ModPathCount);
	}
} // namespace OriginsBASS
