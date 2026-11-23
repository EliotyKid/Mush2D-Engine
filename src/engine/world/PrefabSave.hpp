#pragma once

// Saving inspector edits back to prefab files (spec "Inspetor", decision 7).
//
// "Save to prefab" writes into the object's own prefab file (the leaf, never its base) only the values that
// differ from that file's base chain, or from the C++ defaults when it has no base. Values that came from LDtk
// fields (GameObject::ldtkOverridden) are never saved: they belong to the level.
// Several instances of one prefab saved with different values: each save writes its own values into the same
// file, the last save wins.

#include "engine/world/Prefab.hpp"

#include <filesystem>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

// The object's attributes that differ from resolvedBase (a resolved prefab document, or {} for the C++
// defaults), in prefab form: {"props": {...}, "sprite": {...}, "collider": {...}}. LDtk values are left out.
nlohmann::json diffAgainstBase(const GameObject& object, const nlohmann::json& resolvedBase);

// Merges doc into the prefab file at path (created when missing). An existing file keeps its version, base,
// type, children and components; the other sections are updated key by key, a null value removes the key and a
// section left empty goes away. false + error when the file cannot be read, parsed or written.
bool savePrefab(const std::filesystem::path& path, const nlohmann::json& doc, std::string& error);

// "Save to prefab" needs a source prefab: an object made in code only has "save as new prefab".
inline bool canSaveToPrefab(const GameObject& object) { return !object.prefab.empty(); }

// Writes the edited attributes (names, as the inspector records them) into the object's source prefab: the
// ones equal to a value the base chain sets are removed from the file; the others are written with their
// current value, even when it equals the C++ default (onCreate may set another). Clears the library's cache.
bool saveToSourcePrefab(PrefabLibrary& library, const GameObject& object, const std::set<std::string>& edited,
                        std::string& error);

// Creates <name>.prefab.json with base = the object's prefab (none for an object made in code), its type and
// the edited attributes that differ from that prefab. Refuses an existing prefab or an invalid name.
bool saveAsNewPrefab(PrefabLibrary& library, const GameObject& object, const std::string& name,
                     const std::set<std::string>& edited, std::string& error);
