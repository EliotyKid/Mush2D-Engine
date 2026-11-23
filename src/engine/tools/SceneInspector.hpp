#pragma once

#include "engine/world/Prefab.hpp"
#include "engine/world/Scene2D.hpp"

#include <set>
#include <string>
#include <unordered_map>

// F1 "Cena" window: the scene tree (roots by room, then children and components), the selected object's
// attributes through the reflection registry (every edit applies at once) and the prefab save buttons
// (engine/world/PrefabSave.hpp). Call between ImGui::NewFrame and Render.
// Only object attributes are saved; component edits are live only. A prefab hot reload drops unsaved edits.
class SceneInspector {
public:
    // pause = the "pausar o jogo" checkbox; the caller stops stepping the game while it is set.
    void draw(Scene2D& scene, PrefabLibrary& prefabs, bool& pause);

    // The selected object (may be stale: look it up with scene.findObject).
    GameObjectId selected() const { return selected_; }

private:
    void drawNode(Scene2D& scene, GameObject& o);
    void drawSelected(GameObject& o, PrefabLibrary& prefabs);

    GameObjectId selected_;
    std::unordered_map<GameObjectId, std::set<std::string>> edited_; // attributes edited live, per object
    char filter_[64] = "";
    char newPrefab_[64] = "";
    std::string status_;
};
