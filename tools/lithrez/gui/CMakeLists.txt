set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

qt_add_executable(lithrez-studio WIN32 MACOSX_BUNDLE
  main.cpp
  mainwindow.h mainwindow.cpp
  wizard.h wizard.cpp
  textureview.h textureview.cpp
  modelview.h modelview.cpp
  stringtable.h stringtable.cpp
  soundview.h soundview.cpp
  bankview.h bankview.cpp
  spriteview.h spriteview.cpp
  lipsyncview.h lipsyncview.cpp
  texfxview.h texfxview.cpp
  util.h util.cpp
  resources.qrc
)
target_compile_definitions(lithrez-studio PRIVATE LITHREZSTUDIO_VERSION="${LITHTOOLS_VERSION}")
# Threads: rez files open on a thread of their own (MainWindow::openArchive).
find_package(Threads REQUIRED)
target_link_libraries(lithrez-studio PRIVATE lithtech Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Multimedia Threads::Threads)
set_target_properties(lithrez-studio PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "LithRez Studio"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.lithtools.lithrez-studio")
install(TARGETS lithrez-studio RUNTIME DESTINATION bin BUNDLE DESTINATION .)
