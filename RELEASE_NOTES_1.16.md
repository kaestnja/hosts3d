# Hosts3D 1.16

## Highlights
- Human-readable `settings.ini` replaced the old binary settings file.
- Dynamic/static host lifetime handling was added:
  - automatically discovered hosts start as `dynamic`
  - dynamic hosts are automatically removed again after the configured inactivity timeout
  - locked, selected, or promoted static hosts are kept
- `Selection > Resolve Hostnames` can resolve selected hosts and keep those names in saved layouts.
- `Configure Local Sensors (hsen)` is now available through the GUI on Windows and Linux with interface discovery and local process management.
- The help view is now a non-blocking overlay instead of a modal window.
- Menu entries show current states and real keyboard shortcuts.
- The top-right OSD now uses grouped, readable labels instead of compact abbreviations.
- Windows builds now embed version information in `Hosts3D.exe` and `hsen.exe`.
- Windows packaging is prepared as a distributable runtime ZIP.
- The continuation repository baseline was established for this line:
  - the legacy plain-text `README` was replaced with structured `README.md`
  - `CREDITS.md` was added to preserve authorship and continuation context
  - build/runtime documentation was refreshed for the current repository
  - control and menu documentation was synchronized with the actual runtime behavior

## Recommended Windows Release Asset
- `hosts3d-1.16-windows-x86.zip`

## Notes
- Runtime files inside `hsd-data` are intentionally not shipped as part of the release asset.
- `traffic.hpt` remains a Hosts3D record/replay format, not a Wireshark capture format.
