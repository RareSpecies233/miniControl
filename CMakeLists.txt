cmake_minimum_required(VERSION 3.14)
project(MiniRemote VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate html_content.h from web/index.html
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/html_content.h
    COMMAND ${CMAKE_COMMAND} 
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/web/index.html 
        -DOUTPUT=${CMAKE_CURRENT_SOURCE_DIR}/src/html_content.h 
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_html.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
    COMMENT "Embedding HTML content into C++ header"
)

add_executable(MiniRemote 
    src/main.cpp
    src/html_content.h
)

target_include_directories(MiniRemote PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(MiniRemote PRIVATE 
    _WIN32_WINNT=0x0A00
    NOMINMAX
)

if(WIN32)
    target_link_libraries(MiniRemote PRIVATE ws2_32 winmm gdi32 user32 userenv shell32)
endif()
