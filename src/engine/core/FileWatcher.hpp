#pragma once

#include <filesystem>
#include <string>

// Debug hot reload: polls one file and the newest file with a suffix in one directory, every PollSeconds.
// The two keep separate stamps, so the caller knows which one changed. A file mid-save (vanished, or a
// filesystem error) keeps the previous stamps for that round.
class FileWatcher {
public:
    static constexpr float PollSeconds = 0.5f;
    struct Change {
        bool file = false, dir = false;
        explicit operator bool() const { return file || dir; }
    };

    // Starts watching; the current times are the baseline (no change reported for them).
    void watch(std::filesystem::path file, std::filesystem::path dir, std::string suffix);
    // Adds dt; at most every PollSeconds rescans and reports what changed since the last report.
    Change poll(float dt);
    // Takes the current times as the baseline, e.g. after the caller saved one of the files itself.
    void sync() { stamp_ = scan(stamp_); }

private:
    struct Stamp {
        std::filesystem::file_time_type file{}, dir{};
        bool operator==(const Stamp&) const = default;
    };
    Stamp scan(const Stamp& previous) const;

    std::filesystem::path file_, dir_;
    std::string suffix_;
    Stamp stamp_;
    float timer_ = 0.0f;
};
