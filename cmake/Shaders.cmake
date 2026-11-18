# Compiles shaders/*.{vert,frag} to ${CMAKE_BINARY_DIR}/shaders/<file>.spv.
function(mush2d_compile_shaders target)
    file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/shaders/*.vert ${CMAKE_SOURCE_DIR}/shaders/*.frag)
    set(spvs)
    foreach(src ${sources})
        get_filename_component(name ${src} NAME)
        set(out ${CMAKE_BINARY_DIR}/shaders/${name}.spv)
        add_custom_command(OUTPUT ${out}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
            COMMAND Vulkan::glslc --target-env=vulkan1.3 ${src} -o ${out}
            DEPENDS ${src} VERBATIM)
        list(APPEND spvs ${out})
    endforeach()
    add_custom_target(${target}_shaders DEPENDS ${spvs})
    add_dependencies(${target} ${target}_shaders)
endfunction()

# Makes assets/ and compiled shaders/ reachable next to the exe.
function(mush2d_copy_assets target)
    # No outputs: runs on every build, so edits to assets/shaders always land.
    add_custom_target(${target}_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${target}>/assets
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/shaders $<TARGET_FILE_DIR:${target}>/shaders
        VERBATIM)
    add_dependencies(${target}_assets ${target}_shaders)
    add_dependencies(${target} ${target}_assets)
endfunction()
