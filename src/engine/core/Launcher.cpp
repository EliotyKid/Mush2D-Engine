#include "engine/core/Launcher.hpp"
#include "engine/core/Paths.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <fstream>

std::string readLastGame(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in) return "";
    // parse(in, nullptr, false): do not throw on bad JSON, give a "discarded" value instead.
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return "";
    auto it = j.find("game");
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : "";
}

void writeLastGame(const std::filesystem::path& file, std::string_view id) {
    std::ofstream out(file);
    if (out) out << nlohmann::json{{"game", std::string(id)}}.dump(2) << '\n';
}

std::vector<UncompiledGame> findUncompiledGames(const std::filesystem::path& root, const std::vector<GameEntry>& compiled) {
    namespace fs = std::filesystem;
    std::vector<UncompiledGame> found;
    for (const char* group : {"examples", "games"}) {
        std::error_code ec; // no exceptions: a missing folder is normal
        for (fs::directory_iterator it(root / group, ec), end; !ec && it != end; it.increment(ec)) {
            std::ifstream in(it->path() / "project.json");
            if (!in) continue; // no project.json: not a game folder
            nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
            if (j.is_discarded() || !j.is_object() || j.value("id", std::string()).empty()) continue;
            const std::string id = j["id"].get<std::string>();
            bool isCompiled = false;
            for (const GameEntry& g : compiled) isCompiled = isCompiled || g.id == id;
            if (!isCompiled) found.push_back({id, j.value("name", id)});
        }
    }
    return found;
}

const GameEntry* pickGame(const std::vector<GameEntry>& games, const std::filesystem::path& lastFile,
                          std::string_view cliGame, const FrameRunner& runFrame) {
    // --game: no screen.
    if (!cliGame.empty()) {
        if (const GameEntry* g = findGame(cliGame)) return g;
    }
    if (!runFrame || games.empty()) return nullptr;

    // Marked entry: the last game if it still exists, else the first.
    const std::string last = readLastGame(lastFile);
    size_t marked = 0;
    for (size_t i = 0; i < games.size(); ++i)
        if (games[i].id == last) marked = i;

#ifdef MUSH2D_SOURCE_DIR
    const std::filesystem::path root = MUSH2D_SOURCE_DIR; // Debug: look in the source tree
#else
    const std::filesystem::path root = enginePath("");
#endif
    const std::vector<UncompiledGame> uncompiled = findUncompiledGames(root, games);

    const GameEntry* chosen = nullptr;
    auto drawUi = [&] {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Mush2D", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
        ImGui::TextUnformatted("Choose a game (Enter opens the marked one)");
        ImGui::Separator();
        for (size_t i = 0; i < games.size(); ++i) {
            const bool isMarked = i == marked;
            // The marked game is shown as "> name <" and gets the keyboard focus first.
            const std::string label = (isMarked ? "> " : "  ") + games[i].name + (isMarked ? " <" : "  ") + "###" + games[i].id;
            if (isMarked && ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            if (ImGui::Button(label.c_str(), ImVec2(260, 0))) chosen = &games[i];
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) chosen = &games[marked];
        for (const UncompiledGame& u : uncompiled)
            ImGui::TextDisabled("%s (%s): sem codigo compilado", u.name.c_str(), u.id.c_str());
        ImGui::End();
    };

    while (!chosen) {
        if (!runFrame(drawUi)) return nullptr; // window closed
    }
    writeLastGame(lastFile, chosen->id);
    return chosen;
}
