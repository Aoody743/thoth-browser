# Sparkle is pinned and verified before extraction. Override the distribution
# directory for offline builds; it must contain Sparkle.framework.
set(THOTH_SPARKLE_ROOT "$ENV{HOME}/.cache/thoth-browser/Sparkle-2.10.0" CACHE PATH "Sparkle 2 distribution")
if(NOT EXISTS "${THOTH_SPARKLE_ROOT}/Sparkle.framework")
    set(sparkle_archive "${CMAKE_BINARY_DIR}/Sparkle-2.10.0.tar.xz")
    file(DOWNLOAD
        "https://github.com/sparkle-project/Sparkle/releases/download/2.10.0/Sparkle-2.10.0.tar.xz"
        "${sparkle_archive}"
        EXPECTED_HASH SHA256=c2bf58aa8387266ac179357b1415d6f2635f044da8be41042af32425dae6da0c
        TLS_VERIFY ON
    )
    file(MAKE_DIRECTORY "${THOTH_SPARKLE_ROOT}")
    file(ARCHIVE_EXTRACT INPUT "${sparkle_archive}" DESTINATION "${THOTH_SPARKLE_ROOT}")
endif()
target_sources(ladybird PRIVATE Thoth/Updater.mm)
set_source_files_properties(Thoth/Updater.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_link_libraries(ladybird PRIVATE "${THOTH_SPARKLE_ROOT}/Sparkle.framework")
set_property(TARGET ladybird APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")
# Run before the bundle's final signing command.
add_custom_command(TARGET ladybird POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:ladybird>/Contents/Frameworks"
    COMMAND /usr/bin/ditto "${THOTH_SPARKLE_ROOT}/Sparkle.framework" "$<TARGET_BUNDLE_DIR:ladybird>/Contents/Frameworks/Sparkle.framework"
    VERBATIM
)
