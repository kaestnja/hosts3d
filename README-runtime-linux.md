# Hosts3D 1.18 for Linux arm64

This package contains the Linux runtime files for `Hosts3D` and `hsen`.

## Included Files
- `hosts3d`
- `hsen`
- `COPYING`

## Runtime Requirements
Install the runtime libraries once on the target machine.

For Raspberry Pi OS / Debian / Ubuntu:

```bash
sudo apt update
sudo apt install libglfw3 libpcap0.8
```

`Hosts3D` itself also needs a desktop/OpenGL environment.

## First Start
1. Start `./hosts3d`
2. Right-click in the 3D view and choose `Configure Local Sensors (hsen)`
3. Select one or more capture interfaces
4. Click `Save`, then `Start`

On Linux, `Save` and `Start` try a one-time automatic `setcap` on the bundled `hsen` binary so local capture can work without a manual remote setup.

If that automatic step is not possible, Hosts3D shows the exact one-time command to run locally.

## Headless / Remote Sensor Use
If the machine should only act as a sensor:

```bash
./hsen -l
./hsen 1 eth0 127.0.0.1 -p
```

Replace `127.0.0.1` with the target Hosts3D receiver when needed.

## Notes
- `settings.ini` is plain text and can be edited with Hosts3D closed.
- On Linux/macOS, runtime data is stored in `.hosts3d/`.
- `traffic.hpt` is Hosts3D's own record/replay format, not a Wireshark capture file.

## License
Hosts3D is distributed under the GNU General Public License. See `COPYING`.
