cmake_minimum_required(VERSION 2.8.12)
project(wayland_window_decoration_example)

set(CMAKE_CXX_STANDARD 11)

find_package(PkgConfig)
pkg_check_modules(wayland-client REQUIRED wayland-client)
pkg_check_modules(wayland-egl REQUIRED wayland-egl)
pkg_check_modules(egl REQUIRED egl)
pkg_check_modules(gl REQUIRED gl)

find_package(ECM REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_FIND_MODULE_DIR})

find_package(WaylandScanner REQUIRED)

ecm_add_wayland_client_protocol(WL_PROT_SRC
    PROTOCOL protocol/xdg-shell.xml
    BASENAME xdg-shell)

message("protocol header: " ${WL_PROT_SRC})

include_directories(${CMAKE_BINARY_DIR})
#message("output:", ${CMAKE_BINARY_DIR})

add_executable(${PROJECT_NAME} "main.cpp" ${WL_PROT_SRC})
target_link_libraries(${PROJECT_NAME}
    ${wayland-client_LIBRARIES}
    ${wayland-egl_LIBRARIES}
    ${egl_LIBRARIES}
    ${gl_LIBRARIES}
)
