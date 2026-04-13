# Hosts3D 1.18

## Highlights
- `Hosts3D` has been ported from the legacy GLFW 2 API to GLFW 3.
- The old GLFW 2 threading layer has been replaced with standard C++ threading primitives:
  - `std::thread`
  - `std::mutex`
  - `std::condition_variable`
- Window creation, event callbacks, scroll handling, cursor motion, and buffer swaps now use the GLFW 3 API.
- GUI text entry no longer relies on GLFW 2 ASCII-style key callbacks; printable input now comes from the GLFW 3 character callback.
- Windows build scripts now use the installed MSYS2 GLFW 3 packages and copy `glfw3.dll` into the runtime output.
- Linux/macOS/autotools build metadata has been updated to use GLFW 3 headers and libraries.
- Windows packaging now includes `glfw3.dll` instead of the old `glfw.dll`.

## Notes
- Existing rendering remains on the current OpenGL fixed-function path; this release updates the windowing/input/runtime layer, not the renderer architecture.
- The migration was smoke-tested successfully after the port.
