#include "pch.h"
#include "HiteModLoader.h"
#include "Helpers.h"
#include "SigScan.h"
#include "IniFile.hpp"
#include "mod.hpp"

namespace OriginsBASS {
    namespace Audio {

        SoundFX soundFXList[SFX_COUNT];
        AudioChannel channels[CHANNEL_COUNT];
        float globalVolume = 2.0f;

        // Event
        static void __stdcall EventLoopTrack(HSYNC handle, DWORD channel, DWORD data, void* user) {
            BASS_ChannelSetPosition(channel, reinterpret_cast<QWORD>(user), BASS_POS_BYTE);
        }

        static void __stdcall EventClearSFX(HSYNC handle, DWORD channel, DWORD data, void* user) {
            channels[reinterpret_cast<int8>(user)].soundID = -1;
        }

        void ResetChannels() {
            for (int i = 0; i < CHANNEL_COUNT; ++i) {
                channels[i].basschan = NULL;
                channels[i].isfast = false;
                channels[i].soundID = -1;
                channels[i].state = CHANNEL_IDLE;
            }
        }

        void StopChannel(uint32 channel) {
            if (channel >= CHANNEL_COUNT) {
                RSDKTable->PrintLog(PRINT_POPUP, "[OriginsBASS] Attempt to release channel out of bounds. channel = %u", channel);
                return;
            }

            if (channels[channel].basschan) {
                BASS_ChannelStop(channels[channel].basschan);
                BASS_StreamFree(channels[channel].basschan);
                channels[channel].basschan = 0;
                channels[channel].state = CHANNEL_IDLE;
            }
        }

        void PauseChannel(uint32 channel) {
            if (channel >= CHANNEL_COUNT) {
                RSDKTable->PrintLog(PRINT_POPUP, "[OriginsBASS] Attempt to pause channel out of bounds. channel = %u", channel);
                return;
            }

            if (channels[channel].basschan) {
                BASS_ChannelPause(channels[channel].basschan);
                channels[channel].state |= CHANNEL_PAUSED;
            }
        }

        void ResumeChannel(uint32 channel) {
            if (channel >= CHANNEL_COUNT) {
                RSDKTable->PrintLog(PRINT_POPUP, "[OriginsBASS] Attempt to resume channel out of bounds. channel = %u", channel);
                return;
            }

            if (channels[channel].basschan) {
                BASS_ChannelPlay(channels[channel].basschan, false);
                channels[channel].state &= ~CHANNEL_PAUSED;
            }
        }

        void PlayChannel(uint32 channel, uint32 startPos) {
            if (channel >= CHANNEL_COUNT) {
                RSDKTable->PrintLog(PRINT_POPUP, "[OriginsBASS] Attempt to play channel out of bounds. channel = %u", channel);
                return;
            }

            if (channels[channel].basschan != 0) {
                BASS_ChannelPlay(channels[channel].basschan, true);
                BASS_ChannelSetPosition(channels[channel].basschan, startPos * 4, BASS_POS_BYTE);
                channels[channel].state = CHANNEL_STREAM;
          }
        }

        void SetChannelAttributes(uint32 channel, float volume, float panning, float speed) {
            if (channel == -1)
                return;
            if (channel >= CHANNEL_COUNT) {
                RSDKTable->PrintLog(PRINT_POPUP, "[OriginsBASS] Attempt to set channel attr out of bounds. channel = %u", channel);
                return;
            }

            // Clamp volume
            volume = fminf(4.0f, volume);
            volume = fmaxf(0.0f, volume);

            RSDKTable->PrintLog(PRINT_NORMAL, "[OriginsBASS] Ch: %u, Vol: %f, Tem: %f", channel, volume, speed);
            BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_VOL, volume * globalVolume);
            BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_PAN, panning);
            BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_TEMPO, (speed - 1.0f) * 100.0f);
        }

        void LoadStream(uint32 channel, const char* filename, uint32 loopPoint) {
            if (channel >= CHANNEL_COUNT) {
                RSDKTable->PrintLog(PRINT_POPUP, "[OriginsBASS] Attempt to load channel out of bounds. channel = %u", channel);
                return;
            }

            if (channels[channel].basschan)
                StopChannel(channel);

            channels[channel].basschan = BASS_StreamCreateFile(false, filename, 0, 0, BASS_STREAM_DECODE);

            if (channels[channel].basschan)
            {
                printf("[OriginsBASS] Loaded file stream \"%s\" in channel %u\n", filename, channel);
                channels[channel].basschan = BASS_FX_TempoCreate(channels[channel].basschan, (loopPoint ? BASS_SAMPLE_LOOP : 0) | BASS_FX_FREESOURCE);
                if (loopPoint)
                    BASS_ChannelSetSync(channels[channel].basschan, BASS_SYNC_END | BASS_SYNC_MIXTIME, 0, EventLoopTrack, reinterpret_cast<void*>((QWORD)loopPoint));
                BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_VOL, globalVolume);
                channels[channel].state = CHANNEL_STREAM;
            }
        }

        uint16 FindSFX(const char* name) {
            for (uint32 i = 0; i < SFX_COUNT; ++i) {
                if (soundFXList[i].scope != SCOPE_NONE && !strcmp(name, soundFXList[i].name))
                    return i;
            }
            return -1;
        }

        uint16 LoadSFX(const char *filePath, const char *name, uint8 slot, uint8 scope) {
            if (slot == 0xFF) {
                for (uint32 i = 0; i < SFX_COUNT; ++i) {
                    if (soundFXList[i].scope == SCOPE_NONE) {
                        slot = i;
                        break;
                    }
                }
            }

            if (slot >= SFX_COUNT) {
                RSDKTable->PrintLog(PRINT_POPUP, "[OriginsBASS] Attempt to load SFX out of bounds. slot = %u", slot);
                return -1;
            }

            SoundFX* sfx = &soundFXList[slot];

            if (sfx->buffer)
                free(sfx->buffer);

            strcpy_s(sfx->name, filePath);

            FILE* file;
            fopen_s(&file, filePath, "rb");
            if (file) {
                fseek(file, 0, SEEK_END);
                sfx->buffer = malloc(sfx->bufferLength = ftell(file));
                if (!sfx->buffer)
                    return -1;
                fseek(file, 0, SEEK_SET);
                fread(sfx->buffer, 1, sfx->bufferLength, file);
                fclose(file);
                sfx->scope = scope;
            }

            return slot;
        }

        void PlaySfx(uint16 sfx, uint32 loopPoint, uint32 priority)
        {
	        RSDKTable->PrintLog(PRINT_NORMAL, "Playing sfx id %d, with looppoint %d", sfx, loopPoint); //ONLY USE THIS TO DEBUG A SOUND THAT GETS LOADED AND CRASHES. THIS SHOWS THE PLAYING ID WITH THE LOOP.
            if (sfx >= SFX_COUNT || !soundFXList[sfx].scope)
                return;

            SoundFX* sfxEntry = &soundFXList[sfx];

            uint8 count = 0;
            for (int32 c = 0; c < CHANNEL_COUNT; ++c) {
                if (channels[c].soundID == sfx)
                    ++count;
            }

            int8 channel = -1;
            // Find unused channel
            for (int32 c = 0; c < CHANNEL_COUNT && channel < 0; ++c)
                if (channels[c].soundID == -1)
                    channel = c;

            if (channel == -1)
                return;

            AudioChannel *chan = &channels[channel];

            if (chan->basschan)
                StopChannel(channel);

            chan->basschan = BASS_StreamCreateFile(true, sfxEntry->buffer, 0, sfxEntry->bufferLength, BASS_STREAM_DECODE);

            if (chan->basschan)
            {
                RSDKTable->PrintLog(PRINT_NORMAL, "[OriginsBASS] Loaded file stream \"%s\" in channel %u\n", sfxEntry->name, channel);
                channels[channel].basschan = BASS_FX_TempoCreate(channels[channel].basschan, BASS_FX_FREESOURCE);
                BASS_ChannelSetSync(channels[channel].basschan, BASS_SYNC_END | BASS_SYNC_MIXTIME, 0, EventClearSFX, reinterpret_cast<void*>(channel));
                BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_VOL, globalVolume);
                channels[channel].state = CHANNEL_SFX;
                channels[channel].soundID = sfx;
                BASS_ChannelPlay(channels[channel].basschan, true);
            }
        }
    }// namespace Audio
} // namespace OriginsBASS
