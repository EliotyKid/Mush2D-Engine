#pragma once

// The engine's audio (miniaudio): short sound effects and one music track at a time, looped, cross-faded over
// musicFade seconds. Files by name: <root>/sfx/<name>.wav and <root>/music/<name>.wav (.wav only).
// The Engine owns it and registers it as a Scene2D service (the "sound" effect action reaches it).
// update() takes real time: a hit stop freezes the scene, never the audio.
// Without an audio device (tests, headless) init() is false and everything is silent; names, the current
// music and the fade are still tracked. An unknown name or an unreadable file warns once, with its path.

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>

class Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    // Opens the default playback device. False without one: stays silent.
    bool init();
    // Finds every .wav under root/sfx and root/music (again on each call).
    void scan(const std::filesystem::path& root);
    // volume is multiplied by sfxVolume; pitch 1 = as recorded.
    void playSound(const std::string& name, float volume = 1.0f, float pitch = 1.0f);
    // Cross-fades into the track (looped) over musicFade; "" fades out; the current name does nothing.
    void playMusic(const std::string& name);
    // Every frame, with the real (unscaled) delta: the fades and the volumes, and frees finished sounds.
    void update(float realDt);

    const std::string& currentMusic() const { return music_; }
    float musicMix() const { return mix_; } // 0..1: how far the current track has faded in
    bool knows(const std::string& name) const { return sounds_.count(name) || tracks_.count(name); }

    // Juice numbers, in the F1 panel (LookPanel); runtime only, not saved.
    float masterVolume = 1.0f;
    float sfxVolume = 1.0f;
    float musicVolume = 1.0f;
    float musicFade = 1.0f; // seconds

private:
    struct Device; // miniaudio stays out of this header
    using Files = std::map<std::string, std::filesystem::path>;
    // The file of a name, or nullptr (unknown or unreadable: warned once).
    const std::filesystem::path* usable(const std::string& name, const Files& files, const char* kind);
    void warnOnce(const std::string& key, const std::string& message);

    std::unique_ptr<Device> device_;
    std::filesystem::path root_;
    Files sounds_, tracks_;
    std::map<std::filesystem::path, bool> readable_; // checked once per file
    std::set<std::string> warned_;
    std::string music_;
    float mix_ = 1.0f;     // current track's gain
    float fadeOut_ = 0.0f; // previous track's gain
};
