#pragma once

#include "audio_design.h"
#include "raylib.h"

#include <algorithm>
#include <array>

namespace threadlight {

class GameAudio {
public:
    bool initialize(bool deviceReady) {
        ready_ = deviceReady;
        if (!ready_) return false;

        for (int eventIndex = 0; eventIndex < audio::kEventCount; ++eventIndex) {
            const auto event = static_cast<audio::Event>(eventIndex);
            for (int variant = 0; variant < audio::kVoiceVariants; ++variant) {
                const float tuning = variant == 0 ? 0.972f : 1.028f;
                auto pcm = audio::synthesize(event, tuning);
                // LoadSoundFromWave converts into its own sound buffer before it
                // returns. This Wave only borrows the vector's storage; it must
                // never be passed to UnloadWave.
                Wave wave{static_cast<unsigned int>(pcm.size()), audio::kSampleRate,
                          16u, 1u, pcm.data()};
                sounds_[eventIndex][variant] = LoadSoundFromWave(wave);
            }
        }
        setMasterVolume(masterVolume_);
        return true;
    }

    void play(audio::Event event) {
        if (!ready_ || event == audio::Event::Count) return;

        const int eventIndex = static_cast<int>(event);
        int activeVoices = 0;
        int freeVariant = -1;
        for (int i = 0; i < audio::kEventCount; ++i) {
            for (int variant = 0; variant < audio::kVoiceVariants; ++variant) {
                Sound& sound = sounds_[i][variant];
                if (!IsSoundValid(sound)) continue;
                if (IsSoundPlaying(sound)) {
                    ++activeVoices;
                } else if (i == eventIndex && freeVariant < 0) {
                    freeVariant = variant;
                }
            }
        }

        if (freeVariant < 0) return;
        if (activeVoices >= audio::kMaximumVoices) {
            int lowestPriority = audio::priority(event);
            int victimEvent = -1;
            int victimVariant = -1;
            for (int i = 0; i < audio::kEventCount; ++i) {
                if (audio::priority(static_cast<audio::Event>(i)) >= lowestPriority) continue;
                for (int variant = 0; variant < audio::kVoiceVariants; ++variant) {
                    const Sound sound = sounds_[i][variant];
                    if (IsSoundValid(sound) && IsSoundPlaying(sound) &&
                        audio::priority(static_cast<audio::Event>(i)) < lowestPriority) {
                        lowestPriority = audio::priority(static_cast<audio::Event>(i));
                        victimEvent = i;
                        victimVariant = variant;
                    }
                }
            }
            if (victimEvent < 0) return;
            StopSound(sounds_[victimEvent][victimVariant]);
            --activeVoices;
            if (victimEvent == eventIndex) freeVariant = victimVariant;
        }

        const int nextVoiceCount = activeVoices + 1;
        const float headroom = audio::mixHeadroom(nextVoiceCount);
        applyActiveMix(nextVoiceCount, headroom);
        Sound& sound = sounds_[eventIndex][freeVariant];
        const int pitchPercent = event == audio::Event::Spark
            ? GetRandomValue(94, 106) : GetRandomValue(98, 102);
        SetSoundPitch(sound, pitchPercent / 100.0f);
        SetSoundVolume(sound, masterVolume_ * audio::eventGain(event) * headroom);
        PlaySound(sound);
        mixedVoiceCount_ = nextVoiceCount;
    }

    void adjustMasterVolume(int direction) {
        setMasterVolume(audio::masterVolumeStep(masterVolume_, direction));
    }

    void setMasterVolume(float volume) {
        masterVolume_ = std::clamp(volume, 0.0f, 1.0f);
        if (!ready_) return;
        mixedVoiceCount_ = -1;
        refreshMix();
    }

    float masterVolume() const { return masterVolume_; }

    void update() { refreshMix(); }

    void shutdown() {
        if (!ready_) return;
        for (auto& eventSounds : sounds_) {
            for (Sound& sound : eventSounds) {
                if (IsSoundValid(sound)) UnloadSound(sound);
                sound = {};
            }
        }
        ready_ = false;
    }

private:
    void applyActiveMix(int activeVoices, float headroom) {
        for (int i = 0; i < audio::kEventCount; ++i) {
            const auto event = static_cast<audio::Event>(i);
            for (Sound& sound : sounds_[i]) {
                if (IsSoundValid(sound) && IsSoundPlaying(sound))
                    SetSoundVolume(sound, masterVolume_ * audio::eventGain(event) * headroom);
            }
        }
        mixedVoiceCount_ = activeVoices;
    }

    void refreshMix() {
        if (!ready_) return;
        int activeVoices = 0;
        for (int i = 0; i < audio::kEventCount; ++i) {
            for (const Sound sound : sounds_[i]) {
                if (IsSoundValid(sound) && IsSoundPlaying(sound)) ++activeVoices;
            }
        }
        if (activeVoices == mixedVoiceCount_) return;
        const float headroom = audio::mixHeadroom(activeVoices);
        applyActiveMix(activeVoices, headroom);
    }

    std::array<std::array<Sound, audio::kVoiceVariants>, audio::kEventCount> sounds_{};
    float masterVolume_ = 0.78f;
    int mixedVoiceCount_ = -1;
    bool ready_ = false;
};

} // namespace threadlight
