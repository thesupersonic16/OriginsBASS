#include "pch.h"
#include "HiteModLoader.h"
#include "Helpers.h"
#include "SigScan.h"
#include "IniFile.hpp"
#include "bass.h"
#include "bass_fx.h"
//#include "bass_vgmstream.h"

// Data pointer and array declarations.
#define DataPointer(type, name, address) \
	static type &name = *(type *)address
#define DataArray(type, name, address, len) \
	static DataArray_t<type, len> name(address)

template<typename T, size_t len>
struct DataArray_t final
{
	typedef T value_type;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;
	typedef value_type& reference;
	typedef const value_type& const_reference;
	typedef value_type* pointer;
	typedef const value_type* const_pointer;
	typedef pointer iterator;
	typedef const_pointer const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	DataArray_t() = default; // have to declare default constructor
	DataArray_t(const DataArray_t&) = delete; // object cannot be copied, prevents accidentally using DataArray in a function call
	DataArray_t(const DataArray_t&&) = delete; // object cannot be moved

	DataArray_t(intptr_t addr)
	{
		this->addr = addr;
	}

	// Gets the underlying data for the array.
	constexpr pointer data() const noexcept { return reinterpret_cast<pointer>(addr); }
	// Gets the underlying data for the array.
	constexpr const_pointer cdata() const noexcept { return reinterpret_cast<const_pointer>(addr); }

	// Checks if the array is empty (no elements).
	constexpr bool empty() const noexcept { return len == 0; }

	// Gets the size of the array, in elements.
	constexpr size_type size() const noexcept { return len; }

	// Gets the maximum size of the array, in elements.
	constexpr size_type max_size() const noexcept { return len; }

	constexpr pointer operator&() const noexcept { return data(); }

	constexpr operator pointer() const noexcept { return data(); }

	// Gets an item from the array, with bounds checking.
	constexpr reference at(size_type i)
	{
		if (i < len)
			return data()[i];
		throw std::out_of_range("Data access out of range.");
	}

	// Gets an item from the array, with bounds checking.
	constexpr const_reference at(size_type i) const
	{
		if (i < len)
			return cdata()[i];
		throw std::out_of_range("Data access out of range.");
	}

	template<size_type I>
	// Gets an item from the array, with compile-time bounds checking.
	constexpr reference get() noexcept
	{
		static_assert(I < len, "index is within bounds");
		return data()[I];
	}

	template<size_type I>
	// Gets an item from the array, with compile-time bounds checking.
	constexpr const_reference get() const noexcept
	{
		static_assert(I < len, "index is within bounds");
		return cdata()[I];
	}

	// Gets the first item in the array.
	constexpr reference front() { return *data(); }
	// Gets the first item in the array.
	constexpr const_reference front() const { return *cdata(); }

	// Gets the last item in the array.
	constexpr reference back() { return data()[len - 1]; }
	// Gets the last item in the array.
	constexpr const_reference back() const { return cdata()[len - 1]; }

	// Gets an iterator to the beginning of the array.
	constexpr iterator begin() noexcept { return data(); }
	// Gets an iterator to the beginning of the array.
	constexpr const_iterator begin() const noexcept { return cdata(); }
	// Gets an iterator to the beginning of the array.
	constexpr const_iterator cbegin() const noexcept { return cdata(); }

	// Gets an iterator to the end of the array.
	constexpr iterator end() noexcept { return data() + len; }
	// Gets an iterator to the end of the array.
	constexpr const_iterator end() const noexcept { return cdata() + len; }
	// Gets an iterator to the end of the array.
	constexpr const_iterator cend() const noexcept { return cdata() + len; }

	// Gets a reverse iterator to the beginning of the array.
	constexpr reverse_iterator rbegin() noexcept { return data() + len; }
	// Gets a reverse iterator to the beginning of the array.
	constexpr const_reverse_iterator rbegin() const noexcept { return cdata() + len; }
	// Gets a reverse iterator to the beginning of the array.
	constexpr const_reverse_iterator crbegin() const noexcept { return cdata() + len; }

	// Gets a reverse iterator to the end of the array.
	constexpr reverse_iterator rend() noexcept { return data(); }
	// Gets a reverse iterator to the end of the array.
	constexpr const_reverse_iterator rend() const noexcept { return cdata(); }
	// Gets a reverse iterator to the end of the array.
	constexpr const_reverse_iterator crend() const noexcept { return cdata(); }

	private:
		intptr_t addr;
};

struct TrackInfo {
	char fileName[0x40];
	bool trackLoop;
	unsigned int loopPoint;
};

ModLoader* ModLoaderData;
const char** ModPaths;
int ModPathCount;
int basschan = 0;
int currentTrack;
DataArray(TrackInfo, musicTracks, ReadDataPointer((size_t)SigPlayMusic(), 0x0A, 0x07) + 0x07, 16);

HOOK(void, __fastcall, StopMusic, (char*)SigStopMusic() + 1)
{
	if (basschan != 0)
	{
		BASS_ChannelStop(basschan);
		BASS_ChannelFree(basschan);
		basschan = 0;
	}
	else
		originalStopMusic();
}

static void __stdcall LoopTrack(HSYNC handle, DWORD channel, DWORD data, void* user)
{
	BASS_ChannelSetPosition(channel, musicTracks[currentTrack].loopPoint * 4, BASS_POS_BYTE);
}

HOOK(bool, __fastcall, PlayMusic, SigPlayMusic(), int trackID)
{
	if (!musicTracks[trackID].fileName[0])
		return false;
	implOfStopMusic();
	currentTrack = trackID;
	char fnbase[MAX_PATH];
	snprintf(fnbase, MAX_PATH, "%s\\Data\\Music\\%s", ModLoaderData->GetDataPackName(), musicTracks[trackID].fileName);
	PathRemoveExtensionA(fnbase);
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
			char loopfn[MAX_PATH];
			strncpy(loopfn, fn2, MAX_PATH);
			PathRemoveFileSpecA(loopfn);
			PathAppendA(loopfn, "MusicLoops.ini");
			if (FileExists(loopfn))
			{
				IniFile ini(loopfn);
				musicTracks[trackID].loopPoint = ini.getInt("", PathFindFileNameA(fnbase), musicTracks[trackID].loopPoint);
			}
			bool useloop = false;
			useloop = musicTracks[trackID].trackLoop;
			basschan = BASS_StreamCreateFile(false, fn2, 0, 0, BASS_STREAM_DECODE);

			if (basschan != 0)
			{
				basschan = BASS_FX_TempoCreate(basschan, (useloop ? BASS_SAMPLE_LOOP : 0) | BASS_FX_FREESOURCE);
				if (useloop)
					BASS_ChannelSetSync(basschan, BASS_SYNC_END | BASS_SYNC_MIXTIME, 0, LoopTrack, nullptr);
				BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_VOL, 0.5);
				BASS_ChannelPlay(basschan, true);
				return true;
			}
		}
	}
	return originalPlayMusic(trackID);
}

HOOK(void, __fastcall, SwapMusicTrack, SigSwapMusicTrack(), const char* filePath, int trackID)
{
	if (basschan == 0)
		originalSwapMusicTrack(filePath, trackID);
	else if (strstr(filePath, "_F.ogg"))
		BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_TEMPO, 25);
	else
		BASS_ChannelSetAttribute(basschan, BASS_ATTRIB_TEMPO, 0);
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

	INSTALL_HOOK(PlayMusic);
	INSTALL_HOOK(StopMusic);
	INSTALL_HOOK(SwapMusicTrack);
}