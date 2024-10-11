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
        float globalVolume = 1.0f;

        // Event
        static void __stdcall EventLoopTrack(HSYNC handle, DWORD channel, DWORD data, void* user) {
            AudioChannel* channelEntry = &channels[reinterpret_cast<uintptr_t>(user) & 0x0F];
            BASS_ChannelSetPosition(channel, channelEntry->loopStart, BASS_POS_BYTE);
        }

        static void __stdcall EventClearSFX(HSYNC handle, DWORD channel, DWORD data, void* user) {
            channels[reinterpret_cast<uintptr_t>(user) & 0x0F].soundID = -1;
        }

        void ResetChannels() {
            for (int i = 0; i < CHANNEL_COUNT; ++i)
                StopChannel(i);
        }

        void StopChannel(uint32 channel) {
            if (channel >= CHANNEL_COUNT) {
                printf("[OriginsBASS] Attempt to release channel out of bounds. channel = %u\n", channel);
                return;
            }

            if (channels[channel].basschan) {
                if (BASS_ChannelIsActive(channels[channel].basschan) != BASS_ACTIVE_STOPPED)
                BASS_ChannelStop(channels[channel].basschan);
                BASS_StreamFree(channels[channel].basschan);
                channels[channel].basschan = NULL;
                channels[channel].streamSpeed = 1.0f;
                channels[channel].soundID = -1;
                channels[channel].state = CHANNEL_IDLE;
            }
        }

        void PauseChannel(uint32 channel) {
            if (channel >= CHANNEL_COUNT) {
                printf("[OriginsBASS] Attempt to pause channel out of bounds. channel = %u\n", channel);
                return;
            }

            if (channels[channel].basschan) {
                BASS_ChannelPause(channels[channel].basschan);
                channels[channel].state |= CHANNEL_PAUSED;
            }
        }

        void ResumeChannel(uint32 channel) {
            if (channel >= CHANNEL_COUNT) {
                printf("[OriginsBASS] Attempt to resume channel out of bounds. channel = %u\n", channel);
                return;
            }

            if (channels[channel].basschan) {
                BASS_ChannelPlay(channels[channel].basschan, false);
                channels[channel].state &= ~CHANNEL_PAUSED;
            }
        }

        void PlayChannel(uint32 channel, uint32 startPos) {
            if (channel >= CHANNEL_COUNT) {
                printf("[OriginsBASS] Attempt to play channel out of bounds. channel = %u\n", channel);
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
                printf("[OriginsBASS] Attempt to set channel attr out of bounds. channel = %u\n", channel);
                return;
            }

            // Clamp volume
            volume = fminf(4.0f, volume);
            volume = fmaxf(0.0f, volume);

            printf("[OriginsBASS] Ch: %u, Vol: %f, Tem: %f\n", channel, volume, speed);
            BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_VOL, volume * globalVolume);
            BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_PAN, panning);
            BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_TEMPO, (speed - 1.0f) * 100.0f);
        }

        void LoadStream(uint32 channel, const char* filename, const char* name, int32 loopStart, int32 loopEnd) {
            if (channel >= CHANNEL_COUNT) {
                printf("[OriginsBASS] Attempt to load channel out of bounds. channel = %u\n", channel);
                return;
            }

            if (channels[channel].basschan)
                StopChannel(channel);

            //channels[channel].basschan = BASS_VGMSTREAM_StreamCreate(filename, (loopStart ? BASS_SAMPLE_LOOP : 0) | BASS_STREAM_DECODE);
            //if (!channels[channel].basschan)
                channels[channel].basschan = BASS_StreamCreateFile(false, filename, 0, 0, (loopStart ? BASS_SAMPLE_LOOP : 0) | BASS_STREAM_DECODE);

            if (channels[channel].basschan)
            {
                printf("[OriginsBASS] Loaded file stream \"%s\" in channel %u\n", filename, channel);
                channels[channel].state = CHANNEL_STREAM;
                channels[channel].loopStart = BASS_ChannelSeconds2Bytes(channels[channel].basschan, loopStart / 44100.0);
                channels[channel].loopEnd = BASS_ChannelSeconds2Bytes(channels[channel].basschan, loopEnd / 44100.0);
                strcpy_s(channels[channel].name, name);

                channels[channel].basschan = BASS_FX_TempoCreate(channels[channel].basschan, (loopStart ? BASS_SAMPLE_LOOP : 0));
                if (loopStart)
                    if (loopEnd == -1)
                         BASS_ChannelSetSync(channels[channel].basschan, BASS_SYNC_END, 0, EventLoopTrack, reinterpret_cast<void*>((QWORD)channel));
                     else
                         BASS_ChannelSetSync(channels[channel].basschan, BASS_SYNC_POS, channels[channel].loopEnd, EventLoopTrack, reinterpret_cast<void*>((QWORD)channel));
                 BASS_ChannelSetAttribute(channels[channel].basschan, BASS_ATTRIB_VOL, globalVolume);
            }
        }

        uint32 GetChannelPos(uint32 channel) {
            if (channels[channel].basschan) {
                BASS_CHANNELINFO info;
                BASS_ChannelGetInfo(channels[channel].basschan, &info);
                QWORD bytePos = BASS_ChannelGetPosition(channels[channel].basschan, BASS_POS_BYTE);
                QWORD bps = (QWORD)(((info.origres ? (float)info.origres : 16.0f) / 8.0f) * (float)info.chans);
                return (uint32)(bytePos / (float)bps);
            }
            return 0;
        }
        
        uint32 GetChannelSampleCount(uint32 channel) {
            if (channels[channel].basschan) {
                BASS_CHANNELINFO info;
                BASS_ChannelGetInfo(channels[channel].basschan, &info);
                QWORD byteCount = BASS_ChannelGetLength(channels[channel].basschan, BASS_POS_BYTE);
                QWORD bps = (info.origres ? info.origres : 16) * info.chans;
                return (byteCount / bps) & 0xFFFFFFFF;
            }
            return 0;
        }

        uint8 FindBestChannel() {
            int8 channel = -1;
            // Find unused channel
            for (int8 chan = 0; chan < CHANNEL_COUNT; ++chan)
                if (channels[chan].state == CHANNEL_IDLE)
                    return chan;

            // Find SFX channel that is nearing the end
            uint32 remainingSamplesMin = -1;
            for (int8 chan = 0; chan < CHANNEL_COUNT; ++chan) {
                uint32 remaining = GetChannelSampleCount(chan) - GetChannelPos(chan);
                if (channels[chan].state == CHANNEL_SFX && remaining < remainingSamplesMin) {
                    remainingSamplesMin = remaining;
                    channel = chan;
                }
            }
            return channel;
        }

        uint16 FindSFX(const char* name) {
            for (uint32 i = 0; i < SFX_COUNT; ++i) {
                if (soundFXList[i].scope != SCOPE_NONE && !strcmp(name, soundFXList[i].name))
                    return i;
            }
            return -1;
        }

        uint16 LoadSFX(const char *filePath, const char *name, uint8 slot, uint8 maxConcurrentPlays, uint8 scope) {
            if (slot == 0xFF) {
                for (uint32 i = 0; i < SFX_COUNT; ++i) {
                    if (soundFXList[i].scope == SCOPE_NONE) {
                        slot = i;
                        break;
                    }
                }
            }

            if (slot >= SFX_COUNT) {
                printf("[OriginsBASS] Attempt to load SFX out of bounds. slot = %u\n", slot);
                return -1;
            }

            SoundFX* sfx = &soundFXList[slot];

            if (sfx->buffer)
                free(sfx->buffer);

            strcpy_s(sfx->name, name);

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
                sfx->maxConcurrentPlays = maxConcurrentPlays;
            }

            return slot;
        }

        void PlaySfx(uint16 sfx, uint32 loopPoint, uint32 priority)
        {
            if (sfx >= SFX_COUNT || !soundFXList[sfx].scope)
                return;

            SoundFX* sfxEntry = &soundFXList[sfx];

            uint8 firstChannel = 0;
            uint8 count = 0;
            for (int32 c = CHANNEL_COUNT - 1; c > 0; --c) {
                if (channels[c].soundID == sfx) {
                    ++count;
                    firstChannel = c;
                }
            }

            if (count >= sfxEntry->maxConcurrentPlays) {
                StopChannel(firstChannel);
                return;
            }

            int8 channel = FindBestChannel();

            if (channel == -1)
                return;

            AudioChannel *chan = &channels[channel];

            if (chan->basschan)
                StopChannel(channel);

            chan->basschan = BASS_StreamCreateFile(true, sfxEntry->buffer, 0, sfxEntry->bufferLength, BASS_STREAM_DECODE);

            if (chan->basschan)
            {
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
