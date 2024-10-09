namespace OriginsBASS {
    namespace Audio {

        enum ChannelStates { CHANNEL_IDLE, CHANNEL_SFX, CHANNEL_STREAM, CHANNEL_LOADING_STREAM, CHANNEL_PAUSED = 0x40 };
        
        struct SoundFX {
		    char name[MAX_PATH];
            void* buffer;
            uint32 bufferLength;
            uint8 scope;
	    };

        struct AudioChannel {
		    DWORD basschan;

            uint8 state;
            int16 soundID;
		    char currentmusic[MAX_PATH];
		    bool isfast = false;
	    };


        extern SoundFX soundFXList[SFX_COUNT];
        extern AudioChannel channels[CHANNEL_COUNT];
        extern float globalVolume;

        // Event
        static void __stdcall EventLoopTrack(HSYNC handle, DWORD channel, DWORD data, void* user);

        void ResetChannels();
        void StopChannel(uint32 channel);
        void PauseChannel(uint32 channel);
        void ResumeChannel(uint32 channel);
        void PlayChannel(uint32 channel, uint32 startPos);
        void SetChannelAttributes(uint32 channel, float volume, float panning, float speed);
        void LoadStream(uint32 channel, const char* filename, uint32 loopPoint);
        uint16 FindSFX(const char* name);
        uint16 LoadSFX(const char* filePath, const char* name, uint8 slot, uint8 scope);
        void PlaySfx(uint16 sfx, uint32 loopPoint, uint32 priority);
    }// namespace Audio
} // namespace OriginsBASS
