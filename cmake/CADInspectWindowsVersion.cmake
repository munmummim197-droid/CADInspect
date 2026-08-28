include_guard(GLOBAL)

function(cadinspect_add_windows_version_resource
         target file_description original_filename internal_name)
    if(NOT WIN32)
        return()
    endif()

    set(CADINSPECT_FILE_DESCRIPTION "${file_description}")
    set(CADINSPECT_ORIGINAL_FILENAME "${original_filename}")
    set(CADINSPECT_INTERNAL_NAME "${internal_name}")
    set(version_resource
        "${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc")
    configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../resources/windows/cadinspect_version.rc.in"
        "${version_resource}"
        @ONLY)
    target_sources(${target} PRIVATE "${version_resource}")
endfunction()
