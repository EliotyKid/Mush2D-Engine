#include "engine/core/FileWatcher.hpp"

#include <algorithm>
#include <utility>

void FileWatcher::watch(std::filesystem::path file, std::filesystem::path dir, std::string suffix) {
    file_ = std::move(file);
    dir_ = std::move(dir);
    suffix_ = std::move(suffix);
    timer_ = 0.0f;
    stamp_ = scan({});
}

FileWatcher::Change FileWatcher::poll(float dt) {
    if ((timer_ += dt) < PollSeconds) return {};
    timer_ = 0.0f;
    const Stamp now = scan(stamp_); // a half-saved file is reported once, then waits for the next save
    const Change change{now.file != stamp_.file, now.dir != stamp_.dir};
    stamp_ = now;
    return change;
}

FileWatcher::Stamp FileWatcher::scan(const Stamp& previous) const {
    namespace fs = std::filesystem;
    std::error_code ec; // a file mid-save may vanish for a moment: skip it this round
    // min(), not {}: libstdc++'s file clock epoch is in 2174, so real times are negative.
    Stamp now{fs::last_write_time(file_, ec), fs::file_time_type::min()};
    if (ec) return previous;
    try {
        for (const fs::directory_entry& f : fs::directory_iterator(dir_, ec)) {
            const fs::file_time_type t = f.last_write_time(ec);
            if (!ec && f.path().string().ends_with(suffix_)) now.dir = std::max(now.dir, t);
        }
    } catch (const fs::filesystem_error&) { return previous; }
    return now;
}
