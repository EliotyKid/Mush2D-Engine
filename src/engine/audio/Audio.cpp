#include "engine/audio/Audio.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <iostream>
#include <vector>

namespace {
// Windows paths are wide: the narrow miniaudio calls would garble non-ASCII folders.
#ifdef _WIN32
ma_result openDecoder(const std::filesystem::path& p, ma_decoder* d) { return ma_decoder_init_file_w(p.c_str(), nullptr, d); }
ma_result openSound(ma_engine* e, const std::filesystem::path& p, ma_uint32 flags, ma_sound* s) {
    return ma_sound_init_from_file_w(e, p.c_str(), flags, nullptr, nullptr, s);
}
#else
ma_result openDecoder(const std::filesystem::path& p, ma_decoder* d) { return ma_decoder_init_file(p.c_str(), nullptr, d); }
ma_result openSound(ma_engine* e, const std::filesystem::path& p, ma_uint32 flags, ma_sound* s) {
    return ma_sound_init_from_file(e, p.c_str(), flags, nullptr, nullptr, s);
}
#endif

struct SoundDeleter {
    void operator()(ma_sound* s) const {
        ma_sound_uninit(s);
        delete s;
    }
};
using Sound = std::unique_ptr<ma_sound, SoundDeleter>;

Sound load(ma_engine* engine, const std::filesystem::path& path, ma_uint32 flags) {
    auto* s = new ma_sound;
    if (openSound(engine, path, flags | MA_SOUND_FLAG_NO_SPATIALIZATION, s) != MA_SUCCESS) {
        delete s;
        return nullptr;
    }
    return Sound(s);
}
}

struct Audio::Device {
    ma_engine engine{};
    bool ready = false;        // engine initialised
    std::vector<Sound> sounds; // effects still playing
    Sound music, fading;       // fading: the previous track, fading out
    ~Device() {
        sounds.clear();
        music.reset();
        fading.reset();
        if (ready) ma_engine_uninit(&engine);
    }
};

Audio::Audio() = default;
Audio::~Audio() = default;

bool Audio::init() {
    auto device = std::make_unique<Device>();
    device->ready = ma_engine_init(nullptr, &device->engine) == MA_SUCCESS;
    // miniaudio falls back to its null backend: that is no device either.
    if (!device->ready || ma_engine_get_device(&device->engine)->pContext->backend == ma_backend_null) {
        std::cerr << "Audio: no audio device, sound is off\n";
        return false;
    }
    device_ = std::move(device);
    return true;
}

void Audio::scan(const std::filesystem::path& root) {
    root_ = root;
    sounds_.clear();
    tracks_.clear();
    readable_.clear();
    for (auto [dir, files] : {std::pair{"sfx", &sounds_}, std::pair{"music", &tracks_}}) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(root / dir, ec))
            if (entry.path().extension() == ".wav") (*files)[entry.path().stem().string()] = entry.path();
    }
}

void Audio::warnOnce(const std::string& key, const std::string& message) {
    if (warned_.insert(key).second) std::cerr << "Audio: " << message << '\n';
}

const std::filesystem::path* Audio::usable(const std::string& name, const Files& files, const char* kind) {
    const std::string what = std::string(kind) + " '" + name + "'";
    auto it = files.find(name);
    if (it == files.end()) {
        const char* dir = &files == &sounds_ ? "sfx" : "music";
        warnOnce(what, "unknown " + what + " (" + (root_ / dir / (name + ".wav")).string() + ")");
        return nullptr;
    }
    auto [readable, fresh] = readable_.try_emplace(it->second, false);
    if (fresh) {
        ma_decoder decoder;
        readable->second = openDecoder(it->second, &decoder) == MA_SUCCESS;
        if (readable->second) ma_decoder_uninit(&decoder);
    }
    if (!readable->second) {
        warnOnce(what, what + " cannot be read as .wav (" + it->second.string() + ")");
        return nullptr;
    }
    return &it->second;
}

void Audio::playSound(const std::string& name, float volume, float pitch) {
    const std::filesystem::path* path = usable(name, sounds_, "sound");
    if (!path || !device_) return;
    Sound s = load(&device_->engine, *path, MA_SOUND_FLAG_DECODE); // decoded on every play: miniaudio frees the data once no sound holds it
    if (!s) return warnOnce("sound '" + name + "'", "sound '" + name + "' failed to play (" + path->string() + ")");
    ma_sound_set_volume(s.get(), volume * sfxVolume);
    ma_sound_set_pitch(s.get(), pitch);
    ma_sound_start(s.get());
    // shortcut: no voice cap, every play is a new ma_sound; cap per name when spam shows in a profile
    device_->sounds.push_back(std::move(s));
}

void Audio::playMusic(const std::string& name) {
    if (name == music_) return;
    music_ = name;
    fadeOut_ = mix_; // a third track mid-fade: the oldest stops at once
    mix_ = 0.0f;
    const std::filesystem::path* path = name.empty() ? nullptr : usable(name, tracks_, "music");
    if (!device_) return;
    device_->fading = std::move(device_->music);
    if (!path) return;
    device_->music = load(&device_->engine, *path, MA_SOUND_FLAG_STREAM);
    if (!device_->music) return warnOnce("music '" + name + "'", "music '" + name + "' failed to play (" + path->string() + ")");
    ma_sound_set_looping(device_->music.get(), MA_TRUE);
    ma_sound_set_volume(device_->music.get(), 0.0f);
    ma_sound_start(device_->music.get());
}

void Audio::update(float realDt) {
    const float step = musicFade > 0.0f ? realDt / musicFade : 1.0f;
    mix_ = std::min(mix_ + step, 1.0f);
    fadeOut_ = std::max(fadeOut_ - step, 0.0f);
    if (!device_) return;
    ma_engine_set_volume(&device_->engine, masterVolume);
    if (device_->music) ma_sound_set_volume(device_->music.get(), mix_ * musicVolume);
    if (device_->fading) ma_sound_set_volume(device_->fading.get(), fadeOut_ * musicVolume);
    if (fadeOut_ <= 0.0f) device_->fading.reset();
    std::erase_if(device_->sounds, [](const Sound& s) { return ma_sound_at_end(s.get()); });
}
