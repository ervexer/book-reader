cmake_minimum_required(VERSION 3.21)

project(BookReader VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_library(sqlite3 STATIC third_party/sqlite/sqlite3.c)
set_target_properties(sqlite3 PROPERTIES AUTOMOC OFF AUTORCC OFF)
target_include_directories(sqlite3 PUBLIC third_party)
if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

qt_add_executable(book-reader WIN32
    src/app/main.cpp
    src/data/database.cpp
    src/data/database.h
    src/parsers/fb2parser.cpp
    src/parsers/fb2parser.h
    src/ui/pages/accountpage.cpp
    src/ui/pages/accountpage.h
    src/ui/pages/bookinfopage.cpp
    src/ui/pages/bookinfopage.h
    src/ui/pages/homepage.cpp
    src/ui/pages/homepage.h
    src/ui/pages/librarypage.cpp
    src/ui/pages/librarypage.h
    src/ui/widgets/sidebar.cpp
    src/ui/widgets/sidebar.h
    src/ui/widgets/topbar.cpp
    src/ui/widgets/topbar.h
    src/ui/windows/mainwindow.cpp
    src/ui/windows/mainwindow.h
    src/ui/windows/readerwindow.cpp
    src/ui/windows/readerwindow.h
    resources/bookproject.qrc
)

target_include_directories(book-reader PRIVATE src)
target_link_libraries(book-reader PRIVATE sqlite3 Qt6::Core Qt6::Gui Qt6::Widgets)
if(MSVC)
    target_compile_options(book-reader PRIVATE /utf-8)
endif()

# Keep the executable name expected by the existing Inno Setup installer.
set_target_properties(book-reader PROPERTIES OUTPUT_NAME "book project")
install(TARGETS book-reader RUNTIME DESTINATION .)
