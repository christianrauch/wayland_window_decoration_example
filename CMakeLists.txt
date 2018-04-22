cmake_minimum_required(VERSION 2.8)
project(wayland_window_decoration_example)

find_package(PkgConfig)
pkg_check_modules(wayland-client REQUIRED wayland-client)
pkg_check_modules(wayland-egl REQUIRED wayland-egl)
pkg_check_modules(egl REQUIRED egl)
pkg_check_modules(gl REQUIRED gl)

add_executable(${PROJECT_NAME} "main.cpp")
target_link_libraries(${PROJECT_NAME}
    ${wayland-client_LIBRARIES}
    ${wayland-egl_LIBRARIES}
    ${egl_LIBRARIES}
    ${gl_LIBRARIES}
)
