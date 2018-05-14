# Wayland client side window decoration example

This example demonstrates how to use subsurfaces for adding client side window decorations to a Wayland surface. The shell interfaces `wl_shell` and `xdg_wm_base` are supported, whereas `xdg_wm_base` is selected by default if available.
The window decoration consists of:
- window boarders for resizing
- tragbar for moving the window
- buttons for closing, maximising and minimising


Build dependencies:
`sudo apt install libwayland-dev libegl1-mesa-dev wayland-protocols libwayland-bin extra-cmake-modules`
