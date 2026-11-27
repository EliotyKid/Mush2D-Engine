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
    // The engine draws it for every game while the F1 panel is open. prefabs = null: no prefab save buttons.
    void draw(Scene2D& scene, PrefabLibrary* prefabs);

    // The engine tells it, every frame, whether the F1 panel is open.
    void setOpen(bool open) { open_ = open; }
    // True while the F1 panel is open and its "pausar o jogo" box is checked: the game skips its step then.
    bool pausing() const { return open_ && pause_; }

    // The selected object (may be stale: look it up with scene.findObject).
    GameObjectId selected() const { return selected_; }

private:
    void drawNode(Scene2D& scene, GameObject& o);
    void drawSelected(GameObject& o, PrefabLibrary* prefabs);

    GameObjectId selected_;
    bool pause_ = false; // the "pausar o jogo" box
    bool open_ = false;  // the F1 panel is open this frame
    std::unordered_map<GameObjectId, std::set<std::string>> edited_; // attributes edited live, per object
    char filter_[64] = "";
    char newPrefab_[64] = "";
    std::string status_;
};
