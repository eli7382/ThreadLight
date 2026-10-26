#include "game_audio.h"
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace {
struct CapturedWave { unsigned int rate,size,channels;std::vector<std::int16_t> samples; };
std::vector<CapturedWave> captured;
int manualAllocations=0,unloadWaves=0;
}

extern "C" {
void* MemAlloc(unsigned int size){++manualAllocations;return std::malloc(size);}
void UnloadWave(Wave wave){++unloadWaves;std::free(wave.data);}
Sound LoadSoundFromWave(Wave wave){
    const auto* samples=static_cast<const std::int16_t*>(wave.data);
    captured.push_back({wave.sampleRate,wave.sampleSize,wave.channels,
                        std::vector<std::int16_t>(samples,samples+wave.frameCount)});
    return {};
}
bool IsSoundValid(Sound){return false;}
bool IsSoundPlaying(Sound){return false;}
void SetSoundVolume(Sound,float){}
void UnloadSound(Sound){}
}

int main(){
    threadlight::GameAudio audio;
    if(!audio.initialize(true))return EXIT_FAILURE;
    audio.shutdown();
    if(manualAllocations || unloadWaves || captured.size()!=
       static_cast<std::size_t>(threadlight::audio::kEventCount*threadlight::audio::kVoiceVariants))return EXIT_FAILURE;
    for(int event=0;event<threadlight::audio::kEventCount;++event)for(int variant=0;variant<threadlight::audio::kVoiceVariants;++variant){
        const auto& wave=captured[event*threadlight::audio::kVoiceVariants+variant];
        const auto expected=threadlight::audio::synthesize(static_cast<threadlight::audio::Event>(event),variant==0?.972f:1.028f);
        if(wave.rate!=threadlight::audio::kSampleRate||wave.size!=16||wave.channels!=1||wave.samples!=expected)return EXIT_FAILURE;
    }
    std::cout<<"Borrowed audio samples retained their metadata and waveform.\n";
}
