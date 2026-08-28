cmake_minimum_required(VERSION 3.16)

project(Image_Stitcher_Auto VERSION 1.2.1 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(DEFAULT_SETTINGS_FILE
    "${CMAKE_CURRENT_SOURCE_DIR}/Image_Stitcher_Auto.conf")
configure_file(
    "${DEFAULT_SETTINGS_FILE}"
    "${CMAKE_CURRENT_BINARY_DIR}/Image_Stitcher_Auto.conf"
    COPYONLY
)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Keep the existing local Windows OpenCV installation as a convenience, while
# allowing Linux and other Windows environments to supply OpenCV_DIR normally.
if(WIN32 AND NOT DEFINED OpenCV_DIR)
    set(_opencv_windows_hint "C:/OpenCV/opencv_install_qtmingw/x64/mingw/lib")
    if(EXISTS "${_opencv_windows_hint}")
        set(OpenCV_DIR "${_opencv_windows_hint}" CACHE PATH "OpenCV package directory")
    endif()
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent LinguistTools)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc highgui)

option(IMAGE_STITCHER_ENABLE_VULKAN "Enable the Vulkan SSIM compute path when available" ON)
if(IMAGE_STITCHER_ENABLE_VULKAN)
    find_package(Vulkan QUIET)

    # Some MinGW installations provide the Vulkan development files through
    # MSYS2 even when no standalone Vulkan SDK is installed. Find them as a
    # portable fallback; Vulkan remains optional when neither source exists.
    if(NOT Vulkan_FOUND AND WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        find_path(IMAGE_STITCHER_VULKAN_INCLUDE_DIR
            NAMES vulkan/vulkan.h
            HINTS
                "$ENV{VULKAN_SDK}/Include"
                "$ENV{VULKAN_SDK}/include"
                "$ENV{MSYSTEM_PREFIX}/include"
                "C:/msys64/mingw64/include"
        )
        find_library(IMAGE_STITCHER_VULKAN_LIBRARY
            NAMES vulkan-1
            HINTS
                "$ENV{VULKAN_SDK}/Lib"
                "$ENV{VULKAN_SDK}/lib"
                "$ENV{MSYSTEM_PREFIX}/lib"
                "C:/msys64/mingw64/lib"
        )
        if(IMAGE_STITCHER_VULKAN_INCLUDE_DIR AND IMAGE_STITCHER_VULKAN_LIBRARY)
            set(Vulkan_INCLUDE_DIR "${IMAGE_STITCHER_VULKAN_INCLUDE_DIR}" CACHE PATH
                "Vulkan include directory" FORCE)
            set(Vulkan_LIBRARY "${IMAGE_STITCHER_VULKAN_LIBRARY}" CACHE FILEPATH
                "Vulkan loader library" FORCE)
            find_package(Vulkan QUIET)
        endif()
    endif()
endif()

set(APP_SOURCES
    src/app/main.cpp
    src/app/mainwindow.cpp
    src/app/mainwindow.h
    src/app/mainwindow.ui
)

set(CORE_SOURCES
    src/core/app_settings.cpp
    src/core/app_settings.h
    src/core/image_utils.cpp
    src/core/image_utils.h
    src/core/metal_ssim.h
    src/core/trws.cpp
    src/core/trws.h
    src/core/vulkan_ssim.cpp
    src/core/vulkan_ssim.h
    src/core/vulkan_ssim_shader.h
    src/core/shaders/ssim_batch.comp
)

if(APPLE)
    list(APPEND CORE_SOURCES src/core/metal_ssim.mm)
else()
    list(APPEND CORE_SOURCES src/core/metal_ssim_stub.cpp)
endif()

set(DIALOG_SOURCES
    src/dialogs/application_settings_dialog.cpp
    src/dialogs/application_settings_dialog.h
    src/dialogs/detail_dialog.cpp
    src/dialogs/detail_dialog.h
    src/dialogs/detail_opti_dialog.cpp
    src/dialogs/detail_opti_dialog.h
    src/dialogs/detail_opti_dialog.ui
    src/dialogs/file_input_wiz.cpp
    src/dialogs/file_input_wiz.h
    src/dialogs/file_input_wiz.ui
    src/dialogs/least_squares_settings.cpp
    src/dialogs/least_squares_settings.h
    src/dialogs/least_squares_settings.ui
    src/dialogs/merge_settings.cpp
    src/dialogs/merge_settings.h
    src/dialogs/merge_settings.ui
    src/dialogs/opti_settings.cpp
    src/dialogs/opti_settings.h
    src/dialogs/opti_settings.ui
    src/dialogs/unsupported_files_dialog.cpp
    src/dialogs/unsupported_files_dialog.h
)

set(WIDGET_SOURCES
    src/widgets/autospinbox.cpp
    src/widgets/autospinbox.h
    src/widgets/campus.cpp
    src/widgets/campus.h
    src/widgets/controlpanelscrollarea.h
    src/widgets/canvas_history_graph_widget.cpp
    src/widgets/canvas_history_graph_widget.h
    src/widgets/cornerdirectionselector.cpp
    src/widgets/cornerdirectionselector.h
    src/widgets/droparea.cpp
    src/widgets/droparea.h
    src/widgets/droparea_wiz.cpp
    src/widgets/droparea_wiz.h
    src/widgets/filetablewidget.cpp
    src/widgets/filetablewidget.h
)

set(RESOURCE_SOURCES
    resources/splash.qrc
)

# Exactly one implementation of platform_setup is compiled for the target OS.
if(WIN32)
    set(TARGET_PLATFORM windows)
    set(PLATFORM_SOURCES
        src/platform/platform_setup.h
        src/platform/windows/platform_setup.cpp
        src/platform/windows/app.rc
        src/platform/windows/app.ico
    )
elseif(APPLE)
    set(TARGET_PLATFORM macos)
    set(PLATFORM_SOURCES
        src/platform/platform_setup.h
        src/platform/macos/platform_setup.cpp
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TARGET_PLATFORM linux)
    set(PLATFORM_SOURCES
        src/platform/platform_setup.h
        src/platform/linux/platform_setup.cpp
    )
else()
    message(FATAL_ERROR
        "Unsupported platform '${CMAKE_SYSTEM_NAME}'. "
        "Add an implementation under src/platform/<platform>.")
endif()

message(STATUS "Image Stitcher Auto target platform: ${TARGET_PLATFORM}")

set(PROJECT_SOURCES
    ${APP_SOURCES}
    ${CORE_SOURCES}
    ${DIALOG_SOURCES}
    ${WIDGET_SOURCES}
    ${PLATFORM_SOURCES}
    ${RESOURCE_SOURCES}
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(Image_Stitcher_Auto
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(Image_Stitcher_Auto
        ${PROJECT_SOURCES}
    )
endif()

set(TRANSLATION_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/translations/Image_Stitcher_Auto_en.ts"
)
if(QT_VERSION_MAJOR EQUAL 6)
    qt6_add_translation(TRANSLATION_FILES ${TRANSLATION_SOURCES})
else()
    qt5_add_translation(TRANSLATION_FILES ${TRANSLATION_SOURCES})
endif()
add_custom_target(Image_Stitcher_Auto_translations ALL
    DEPENDS ${TRANSLATION_FILES}
)
add_dependencies(Image_Stitcher_Auto Image_Stitcher_Auto_translations)
if(QT_VERSION_MAJOR EQUAL 6)
    set_source_files_properties(${TRANSLATION_FILES} PROPERTIES GENERATED TRUE)
    qt6_add_resources(Image_Stitcher_Auto embedded_translations
        PREFIX "/translations"
        BASE "${CMAKE_CURRENT_BINARY_DIR}"
        FILES ${TRANSLATION_FILES}
    )
endif()
add_custom_command(TARGET Image_Stitcher_Auto POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DEFAULT_SETTINGS_FILE}"
        "$<TARGET_FILE_DIR:Image_Stitcher_Auto>/Image_Stitcher_Auto.conf"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:Image_Stitcher_Auto>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${TRANSLATION_FILES}
        "$<TARGET_FILE_DIR:Image_Stitcher_Auto>/translations"
)

target_link_libraries(Image_Stitcher_Auto PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    ${OpenCV_LIBS}
)

# OpenCV's Windows packages do not participate in windeployqt deployment.
# Copy the imported OpenCV runtime libraries beside the executable so that a
# freshly built application can also be started outside Qt Creator.
if(WIN32)
    set(IMAGE_STITCHER_OPENCV_RUNTIME_TARGETS "")
    foreach(_opencv_target IN LISTS OpenCV_LIBS)
        if(TARGET "${_opencv_target}")
            get_target_property(_opencv_target_type "${_opencv_target}" TYPE)
            if(_opencv_target_type STREQUAL "SHARED_LIBRARY")
                list(APPEND IMAGE_STITCHER_OPENCV_RUNTIME_TARGETS "${_opencv_target}")
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES IMAGE_STITCHER_OPENCV_RUNTIME_TARGETS)

    foreach(_opencv_target IN LISTS IMAGE_STITCHER_OPENCV_RUNTIME_TARGETS)
        add_custom_command(TARGET Image_Stitcher_Auto POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${_opencv_target}>"
                "$<TARGET_FILE_DIR:Image_Stitcher_Auto>"
        )
    endforeach()

    # The local OpenCV imgcodecs build uses AVIF through separately installed
    # DLLs. They are not represented by OpenCV's imported CMake targets, so
    # locate and deploy them explicitly when present.
    if(TARGET opencv_imgcodecs)
        get_target_property(_opencv_imgcodecs_runtime
            opencv_imgcodecs IMPORTED_LOCATION_RELEASE)
        if(NOT _opencv_imgcodecs_runtime)
            get_target_property(_opencv_imgcodecs_runtime
                opencv_imgcodecs IMPORTED_LOCATION)
        endif()
        if(_opencv_imgcodecs_runtime)
            get_filename_component(_opencv_runtime_dir
                "${_opencv_imgcodecs_runtime}" DIRECTORY)
            get_filename_component(_opencv_dependency_root
                "${_opencv_runtime_dir}/../../../.." ABSOLUTE)

            unset(_image_stitcher_avif_dll CACHE)
            find_file(_image_stitcher_avif_dll
                NAMES libavif.dll avif.dll
                PATHS "${_opencv_dependency_root}"
                PATH_SUFFIXES
                    bin
                    deps/libavif_install/bin
                NO_DEFAULT_PATH
            )
            unset(_image_stitcher_aom_dll CACHE)
            find_file(_image_stitcher_aom_dll
                NAMES libaom.dll aom.dll
                PATHS "${_opencv_dependency_root}"
                PATH_SUFFIXES
                    bin
                    deps/aom_install/bin
                NO_DEFAULT_PATH
            )

            foreach(_opencv_codec_runtime
                    "${_image_stitcher_avif_dll}"
                    "${_image_stitcher_aom_dll}")
                if(_opencv_codec_runtime
                        AND EXISTS "${_opencv_codec_runtime}")
                    add_custom_command(TARGET Image_Stitcher_Auto POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_opencv_codec_runtime}"
                            "$<TARGET_FILE_DIR:Image_Stitcher_Auto>"
                    )
                endif()
            endforeach()
        endif()
    endif()
endif()

if(APPLE)
    find_library(IMAGE_STITCHER_METAL_FRAMEWORK Metal REQUIRED)
    find_library(IMAGE_STITCHER_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(Image_Stitcher_Auto PRIVATE
        ${IMAGE_STITCHER_METAL_FRAMEWORK}
        ${IMAGE_STITCHER_FOUNDATION_FRAMEWORK}
    )
    set_source_files_properties(src/core/metal_ssim.mm PROPERTIES
        COMPILE_OPTIONS "-fobjc-arc"
    )
    set_target_properties(Image_Stitcher_Auto PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    )
    message(STATUS "Image Stitcher Auto Metal SSIM: enabled")
endif()

if(IMAGE_STITCHER_ENABLE_VULKAN AND Vulkan_FOUND)
    target_link_libraries(Image_Stitcher_Auto PRIVATE Vulkan::Vulkan)
    target_compile_definitions(Image_Stitcher_Auto PRIVATE IMAGE_STITCHER_HAS_VULKAN=1)
    message(STATUS "Image Stitcher Auto Vulkan SSIM: enabled")
else()
    message(STATUS "Image Stitcher Auto Vulkan SSIM: unavailable; CPU fallback only")
endif()

target_include_directories(Image_Stitcher_Auto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${OpenCV_INCLUDE_DIRS}
)

if(WIN32)
    set_target_properties(Image_Stitcher_Auto PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(GNUInstallDirs)
install(TARGETS Image_Stitcher_Auto
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
if(APPLE)
    set(IMAGE_STITCHER_RUNTIME_DATA_DIR
        "Image_Stitcher_Auto.app/Contents/MacOS")
else()
    set(IMAGE_STITCHER_RUNTIME_DATA_DIR ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES "${DEFAULT_SETTINGS_FILE}"
    DESTINATION ${IMAGE_STITCHER_RUNTIME_DATA_DIR}
)
install(FILES ${TRANSLATION_FILES}
    DESTINATION ${IMAGE_STITCHER_RUNTIME_DATA_DIR}/translations
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Image_Stitcher_Auto)
endif()

# These flags are valid only for x86/x64 targets. Avoid injecting them into
# Linux builds for ARM and other architectures.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i[3-6]86)$")
    target_compile_options(Image_Stitcher_Auto PRIVATE
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-O3>
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-mavx2>
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-mfma>
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2>
        $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/arch:AVX2>
    )
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(
        FILES packaging/linux/image-stitcher-auto.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(
        FILES resources/image-stitcher-auto.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    )

    install(
        FILES LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "image-stitcher-auto")
    set(CPACK_PACKAGE_VENDOR "Your name or organization")
    set(CPACK_PACKAGE_CONTACT "Your Name <your-email@example.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Automatic image stitching application")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

    set(CPACK_DEBIAN_PACKAGE_MAINTAINER
        "Your Name <your-email@example.com>")
    set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")

    # dpkg-shlibdepsでQt/OpenCV等の依存関係を自動生成
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

    include(CPack)
endif()
