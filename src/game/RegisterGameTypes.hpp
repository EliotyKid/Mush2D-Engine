#pragma once

// Defined in the CMake-generated all_objects.cpp, which includes every src/game/objects/*.hpp.
// Calling it once (main, tests) makes the linker keep that file, so every MUSH2D_OBJECT / MUSH2D_COMPONENT
// in those headers registers itself.
void mush2dRegisterGameTypes();
