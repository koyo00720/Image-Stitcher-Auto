cmake_minimum_required(VERSION 3.16)

project(Image_Stitcher_Auto VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OpenCV_DIR "C:/OpenCV/opencv_install_qtmingw/x64/mingw/lib")

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Concurrent)
find_package(OpenCV REQUIRED COMPONENTS core imgcodecs imgproc highgui)


include_directories(
 ${OpenCV_INCLUDE_DIRS}
)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Image_Stitcher_Auto
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        campus.h campus.cpp
        app.rc
        file_input_wiz.ui file_input_wiz.h file_input_wiz.cpp
        droparea_wiz.h droparea_wiz.cpp
        image_utils.h image_utils.cpp
        filetablewidget.h filetablewidget.cpp
        cornerdirectionselector.h cornerdirectionselector.cpp
        autospinbox.h autospinbox.cpp
        detail_dialog.h detail_dialog.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Image_Stitcher_Auto APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Image_Stitcher_Auto SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Image_Stitcher_Auto
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(Image_Stitcher_Auto PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Concurrent
    ${OpenCV_LIBS}
)
target_include_directories(Image_Stitcher_Auto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OpenCV_INCLUDE_DIRS}
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Image_Stitcher_Auto)
endif()
set_target_properties(Image_Stitcher_Auto PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Image_Stitcher_Auto
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Image_Stitcher_Auto)
endif()

target_compile_options(Image_Stitcher_Auto PRIVATE
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang>>:-O3 -mavx2 -mfma>
    $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/O2 /arch:AVX2>
)

#if (WIN32)
#  enable_language(RC)
#  target_sources(Image_Stitcher_Auto PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/app.rc)
#endif()
