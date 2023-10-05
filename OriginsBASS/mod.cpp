#include "pch.h"
#include "HiteModLoader.h"
#include "Helpers.h"
#include "SigScan.h"
#include "IniFile.hpp"
#include "mod.hpp"

ModLoader* ModLoaderData;
const char** ModPaths;
int ModPathCount;
Channel Channels[0x10];

// Config
float Volume = 0.5f;
float BlueSphereTempos[] = { 5.0f, 10.0f, 15.0f, 20.0f };

static void ReleaseStream(uint32 channel)
{
	if (channel >= 0x10)
	{
		printf("[OriginsBASS] Attempt to release stream out of bounds. channel = %u \n", channel);
		return;
	}

	if (Channels[channel].basschan != 0)
	{
		BASS_ChannelStop(Channels[channel].basschan);
		BASS_StreamFree(Channels[channel].basschan);
		Channels[channel].basschan = 0;
	}
}

static void __stdcall LoopTrack(HSYNC handle, DWORD channel, DWORD data, void* user)
{
	BASS_ChannelSetPosition(channel, (uint32)user, BASS_POS_BYTE);
}

HOOK(int32, __fastcall, PlayStream, SigPlayStream(), const char* filename, uint32 slot, uint32 startPos, uint32 loopPoint, bool32 loadASync)
{
	//printf("[OriginsBASS] PlayStream(\"%s\", %u, %u, %u, %u)\n", filename, slot, startPos, loopPoint, loadASync);
	if (slot >= 0x10)
	{
		return -1;
	}

	bool speedup = false;
	DWORD basschan = Channels[slot].basschan;
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
	if (!strcmp(Channels[slot].currentmusic, "3K/SpecialStage.ogg") 
		&& strstr(filename, "3K/SpecialStageS"))
	{
		tmp = strstr(filename, ".ogg");
		if (tmp)
		{
			int stage = *(tmp - 1) - '0';
			printf("[OriginsBASS] Changing blue sphere tempo to stage %u\n", stage);
			BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_TEMPO, BlueSphereTempos[stage]);
			return slot;
		}
	}

	if (!strcmp(Channels[slot].currentmusic, name))
	{
		if (speedup)
		{
			BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_TEMPO, 20.0f);
			Channels[slot].isfast = true;
			return slot;
		}
		else if (Channels[slot].isfast)
		{
			BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_TEMPO, 0);
			Channels[slot].isfast = false;
			return slot;
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
			ReleaseStream(slot);
			strcpy(Channels[slot].currentmusic, name);
			bool useloop = loopPoint != 0;
			
			Channels[slot].basschan = basschan = BASS_StreamCreateFile(false, fn2, 0, 0, BASS_STREAM_DECODE);

			if (basschan != 0)
			{
				printf("[OriginsBASS] Playing file stream \"%s\" in channel %u\n", name, slot);
				Channels[slot].basschan = basschan = BASS_FX_TempoCreate(basschan, (useloop ? BASS_SAMPLE_LOOP : 0) | BASS_FX_FREESOURCE);
				if (useloop)
					BASS_ChannelSetSync(basschan, BASS_SYNC_END | BASS_SYNC_MIXTIME, 0, LoopTrack, (void*)loopPoint);
				BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_VOL, Volume);
				if (speedup)
					BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_TEMPO, 20.0f);
				BASS_ChannelPlay(basschan, true);
				BASS_ChannelSetPosition(basschan, startPos * 4, BASS_POS_BYTE);
				return slot;
			}
		}
	}
	printf("[OriginsBASS] Failed to load file stream \"%s\"\n", name);
	// Don't play anything if it fails
	return -1;
}

HOOK(void, __fastcall, SetChannelAttributes, SigSetChannelAttributes(), uint8 channel, float volume, float panning, float speed)
{
	// Game seems to just keep on using channels, I don't really like buffer overruns
	if (channel >= 0x10)
	{
		return;
	}
	
	// Clamp volume
	volume = fminf(4.0f, volume);
	volume = fmaxf(0.0f, volume);
	
	printf("[OriginsBASS] Ch: %u, Vol: %f, Tem: %f\n", channel, volume, speed);
	BASS_ChannelSetAttribute(Channels[channel].basschan, BASS_ATTRIB_VOL, volume * Volume);
	BASS_ChannelSetAttribute(Channels[channel].basschan, BASS_ATTRIB_PAN, panning);
	BASS_ChannelSetAttribute(Channels[channel].basschan, BASS_ATTRIB_TEMPO, (speed - 1.0f) * 100.0f);
}

HOOK(void, __fastcall, StopChannel, 0x1400DDF50, uint32 channel)
{
	if (channel >= 0x10)
	{
		printf("[OriginsBASS] Attempted to stop channel out of bounds %i\n", channel);
		return;
	}

	printf("[OriginsBASS] Closing stream %u\n", channel);
	ReleaseStream(channel);
}

HOOK(void, __fastcall, PauseChannel, 0x1400DDE90, uint32 channel)
{
	if (channel >= 0x10)
	{
		return;
	}

	printf("[OriginsBASS] Pausing stream %u\n", channel);
	BASS_ChannelPause(Channels[channel].basschan);
}

HOOK(void, __fastcall, ResumeChannel, 0x1400DDEE0, uint32 channel)
{
	if (channel >= 0x10)
	{
		return;
	}

	printf("[OriginsBASS] Playing stream %u\n", channel);
	BASS_ChannelPlay(Channels[channel].basschan, false);
}


extern "C" __declspec(dllexport) void Init(ModInfo *modInfo)
{
	ModLoaderData = modInfo->ModLoader;

	if (ModLoaderData == (void*)1)
	{
		MessageBoxW(nullptr, L"The installed version of HiteModLoader is too old for OriginsBASS to run,\nplease update the modloader to " ML_VERSION " or newer.", L"OriginsBASS Error", MB_ICONERROR);
		return;
	}

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

	INSTALL_HOOK(PlayStream);
	INSTALL_HOOK(SetChannelAttributes);
	INSTALL_HOOK(StopChannel);
	INSTALL_HOOK(PauseChannel);
	INSTALL_HOOK(ResumeChannel);
}