if(NOT PROJECT_VERSION STREQUAL CADINSPECT_VERSION)
    message(FATAL_ERROR
        "CMake project version ${PROJECT_VERSION} differs from canonical "
        "CADInspect version ${CADINSPECT_VERSION}")
endif()

file(READ "${CMAKE_SOURCE_DIR}/vcpkg.json" vcpkg_manifest)
string(JSON vcpkg_version GET "${vcpkg_manifest}" version-string)
if(NOT vcpkg_version STREQUAL CADINSPECT_VERSION)
    message(FATAL_ERROR
        "vcpkg version ${vcpkg_version} differs from ${CADINSPECT_VERSION}")
endif()

file(READ "${CMAKE_SOURCE_DIR}/installer/CADInspect.iss" installer_definition)
foreach(expected IN ITEMS
        "#define AppVersion \"${CADINSPECT_VERSION}\""
        "#define AppPublisher \"${CADINSPECT_COMPANY_NAME}\""
        "#define AppCopyright \"${CADINSPECT_LEGAL_COPYRIGHT}\""
        "#define AppExecutable \"CADInspect.exe\""
        "#define InstallerFilename \"CADInspect-Setup-x64-${CADINSPECT_VERSION}.exe\"")
    string(FIND "${installer_definition}" "${expected}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR "Installer metadata drift: missing ${expected}")
    endif()
endforeach()

file(READ "${CMAKE_SOURCE_DIR}/scripts/New-CADInspectTrustedArtifact.ps1"
     trusted_artifact_script)
foreach(expected IN ITEMS
        "\$canonicalVersion = '${CADINSPECT_VERSION}'"
        "CADInspect-Setup-x64-${CADINSPECT_VERSION}.exe")
    string(FIND "${trusted_artifact_script}" "${expected}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR
            "Trusted artifact metadata drift: missing ${expected}")
    endif()
endforeach()

file(READ "${CMAKE_SOURCE_DIR}/scripts/New-StepComparePortablePackage.ps1"
     package_script)
foreach(expected IN ITEMS
        "Version = '${CADINSPECT_VERSION}'"
        "'CADInspect.exe'"
        "CADInspectSha256")
    string(FIND "${package_script}" "${expected}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR "Package metadata drift: missing ${expected}")
    endif()
endforeach()

message(STATUS
    "CADInspect release metadata consistency: ${CADINSPECT_VERSION} "
    "(${CADINSPECT_FILE_VERSION})")
