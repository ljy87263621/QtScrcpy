cmake_minimum_required(VERSION 3.19)
project(KeymapEditorTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)
if(MSVC)
    add_compile_options(/utf-8 /W3 /WX)
endif()
enable_testing()
find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Widgets Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Test)
add_executable(tst_keymapeditor tst_keymapeditor.cpp
    ../../QtScrcpy/keymapeditor/keymapdocument.cpp
    ../../QtScrcpy/keymapeditor/keymapoverlay.cpp
    ../../QtScrcpy/keymapeditor/keymapeditor.cpp
    ../../QtScrcpy/keymapeditor/keycapturebutton.cpp
    ../../QtScrcpy/QtScrcpyCore/src/device/controller/inputconvert/keymap/keymap.cpp)
target_include_directories(tst_keymapeditor PRIVATE ../../QtScrcpy/keymapeditor)
target_include_directories(tst_keymapeditor PRIVATE ../../QtScrcpy/QtScrcpyCore/src/device/controller/inputconvert/keymap ../../QtScrcpy/QtScrcpyCore/src/device/android)
target_link_libraries(tst_keymapeditor PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Test)
target_compile_definitions(tst_keymapeditor PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../keymap")
add_test(NAME keymapeditor COMMAND tst_keymapeditor)
set_tests_properties(keymapeditor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
if(WIN32)
    set_tests_properties(keymapeditor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows")
endif()
add_executable(keymap_editor_preview preview.cpp
    ../../QtScrcpy/keymapeditor/keymapdocument.cpp
    ../../QtScrcpy/keymapeditor/keymapoverlay.cpp
    ../../QtScrcpy/keymapeditor/keycapturebutton.cpp
    ../../QtScrcpy/keymapeditor/keymapeditor.cpp)
target_include_directories(keymap_editor_preview PRIVATE ../../QtScrcpy/keymapeditor)
target_link_libraries(keymap_editor_preview PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
