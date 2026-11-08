add_executable(rez_selftest selftest.cpp)
target_link_libraries(rez_selftest PRIVATE lithtech)
add_test(NAME rez_selftest COMMAND rez_selftest ${CMAKE_CURRENT_BINARY_DIR}/selftest-work)
# Archive::open's progress callback and cancelling.
add_executable(rez_progress_test rez_progress_test.cpp)
target_link_libraries(rez_progress_test PRIVATE lithtech)
add_test(NAME rez_progress_test COMMAND rez_progress_test ${CMAKE_CURRENT_BINARY_DIR}/progress-work)
add_executable(abc_verify abc_verify.cpp)
target_link_libraries(abc_verify PRIVATE lithtech)
# .abc v6 (Shogo, Blood 2, KISS): a file made here, checked value by value;
# and every v6 model under a directory, read and converted (abc6_verify).
add_executable(abc6_test abc6_test.cpp)
target_link_libraries(abc6_test PRIVATE lithtech)
add_test(NAME abc6_test COMMAND abc6_test)
add_executable(abc6_verify abc6_verify.cpp)
target_link_libraries(abc6_verify PRIVATE lithtech)

# DTX decoding over whole archives, and texture viewer screenshots.
add_executable(dtx_decode_all dtx_decode_all.cpp)
target_link_libraries(dtx_decode_all PRIVATE lithtech)
# Versions -2 and -3 (Blood 2, KISS Psycho Circus) on files made here.
add_executable(dtx_old_test dtx_old_test.cpp)
target_link_libraries(dtx_old_test PRIVATE lithtech)
add_test(NAME dtx_old_test COMMAND dtx_old_test)
if(TARGET Qt6::Widgets)
  set(CMAKE_AUTOMOC ON)
  add_executable(dtx_view_shots dtx_view_shots.cpp ../gui/textureview.h ../gui/textureview.cpp)
  target_link_libraries(dtx_view_shots PRIVATE lithtech Qt6::Widgets)
endif()
if(TARGET Qt6::Widgets)
  qt_add_executable(abc_preview_shot abc_preview_shot.cpp ../gui/modelview.cpp ../gui/modelview.h)
  target_include_directories(abc_preview_shot PRIVATE ../gui)
  set_target_properties(abc_preview_shot PROPERTIES AUTOMOC ON)
  target_link_libraries(abc_preview_shot PRIVATE lithtech Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
endif()
add_executable(pe_strings_test pe_strings_test.cpp)
target_link_libraries(pe_strings_test PRIVATE lithtech)
# A screenshot of the string table editor with a DLL loaded (manual check):
#   QT_QPA_PLATFORM=offscreen pe_gui_shot <dll> <out.png>
if(TARGET Qt6::Widgets)
  add_executable(pe_gui_shot pe_gui_shot.cpp ${PROJECT_SOURCE_DIR}/gui/stringtable.cpp ${PROJECT_SOURCE_DIR}/gui/stringtable.h)
  set_target_properties(pe_gui_shot PROPERTIES AUTOMOC ON)
  target_include_directories(pe_gui_shot PRIVATE ${PROJECT_SOURCE_DIR}/gui)
  target_link_libraries(pe_gui_shot PRIVATE lithtech Qt6::Widgets)
endif()
# .ltb reading over the extracted models (out/assets/ltb), and value dumps
# to compare with plugins/blender's ltb.py.
add_executable(ltb_verify ltb_verify.cpp)
target_link_libraries(ltb_verify PRIVATE lithtech)
# PS2 models over the extracted discs (out/assets/ps2), and value dumps to
# compare with plugins/blender's ps2ltb.py (tests/ps2ltb_dump.py).
add_executable(ps2ltb_verify ps2ltb_verify.cpp)
target_link_libraries(ps2ltb_verify PRIVATE lithtech)
# Sound decoding over whole archives; tests/compare_sound.py checks samples
# against FFmpeg.
# PCX, TGA and BMP: images made here, layout by layout; and every image of
# whole archives or folders, dumped for compare_images.py to check against
# ImageMagick.
add_executable(pcx_test pcx_test.cpp)
target_link_libraries(pcx_test PRIVATE lithtech)
add_test(NAME pcx_test COMMAND pcx_test)
add_executable(image_test image_test.cpp)
target_link_libraries(image_test PRIVATE lithtech)
add_test(NAME image_test COMMAND image_test)
add_executable(image_decode_all image_decode_all.cpp)
target_link_libraries(image_decode_all PRIVATE lithtech)
add_executable(sound_decode_all sound_decode_all.cpp)
target_link_libraries(sound_decode_all PRIVATE lithtech)
add_executable(sound_test sound_test.cpp)
target_link_libraries(sound_test PRIVATE lithtech)
add_test(NAME sound_test COMMAND sound_test)
# Playback on an output device (a PulseAudio null sink in docker); see the
# README.
if(TARGET Qt6::Multimedia)
  add_executable(sound_play_test sound_play_test.cpp ../gui/soundview.cpp ../gui/soundview.h ../gui/bankview.cpp
                 ../gui/bankview.h)
  set_target_properties(sound_play_test PROPERTIES AUTOMOC ON)
  target_include_directories(sound_play_test PRIVATE ../gui)
  target_link_libraries(sound_play_test PRIVATE lithtech Qt6::Widgets Qt6::Multimedia)
endif()
# PS2 archives: a small one made in the test, read and edited.
# tests/ps2_verify.py checks the shipped discs.
add_executable(ps2rez_test ps2rez_test.cpp)
target_link_libraries(ps2rez_test PRIVATE lithtech)
add_test(NAME ps2rez_test COMMAND ps2rez_test ${CMAKE_CURRENT_BINARY_DIR}/ps2rez-work)
# Sprites: files made here; spr_verify reads every one of a game's archives
# and looks their frames up.
add_executable(spr_test spr_test.cpp)
target_link_libraries(spr_test PRIVATE lithtech)
add_test(NAME spr_test COMMAND spr_test)
add_executable(spr_verify spr_verify.cpp)
target_link_libraries(spr_verify PRIVATE lithtech)
# Lip-sync: files made here; lip_verify reads every one of a game's
# archives against its voice line.
add_executable(lip_test lip_test.cpp)
target_link_libraries(lip_test PRIVATE lithtech)
add_test(NAME lip_test COMMAND lip_test)
add_executable(lip_verify lip_verify.cpp)
target_link_libraries(lip_verify PRIVATE lithtech)
# Texture effects: scripts and groups made here; texfx_verify reads every
# one of a game's archives.
add_executable(texscript_test texscript_test.cpp)
target_link_libraries(texscript_test PRIVATE lithtech)
add_test(NAME texscript_test COMMAND texscript_test)
add_executable(texfx_verify texfx_verify.cpp)
target_link_libraries(texfx_verify PRIVATE lithtech)
# Render styles: files made here; renderstyle_verify reads every one of a
# game's archives.
add_executable(renderstyle_test renderstyle_test.cpp)
target_link_libraries(renderstyle_test PRIVATE lithtech)
add_test(NAME renderstyle_test COMMAND renderstyle_test)
add_executable(renderstyle_verify renderstyle_verify.cpp)
target_link_libraries(renderstyle_verify PRIVATE lithtech)
# Compiled shaders (.ps, .ash): files made here.
add_executable(shader_test shader_test.cpp)
target_link_libraries(shader_test PRIVATE lithtech)
add_test(NAME shader_test COMMAND shader_test)
