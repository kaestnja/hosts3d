# Hosts3D 1.18 for Linux arm64

This package contains the Linux runtime files for `Hosts3D` and `hsen`.

## Included Files
- `README.md`
- `README-runtime-linux.md`
- `README-testing.md`
- `hosts3d`
- `hsen`
- `COPYING`
- `sim-hsen.ps1`
- `sim-hsen.py`
- `demo-hsen.ps1`
- `demo-hsen.py`

## Runtime Requirements
Install the runtime libraries once on the target machine.

For Raspberry Pi OS / Debian / Ubuntu:

```bash
sudo apt update
sudo apt install libglfw3 libpcap0.8
```

`Hosts3D` itself also needs a desktop/OpenGL environment.

## Quick Install On Raspberry Pi 5
No local build is required when you already have the release tarball.

Example target path:

```bash
mkdir -p /home/pi/hosts3d
tar -xzf hosts3d-1.18-linux-arm64.tar.gz --strip-components=1 -C /home/pi/hosts3d
cd /home/pi/hosts3d
```

Optional checksum verification before unpacking:

```bash
sha256sum -c hosts3d-1.18-linux-arm64-SHA256.txt
```

If you want to prepare packet capture rights immediately instead of letting Hosts3D do it on first use:

```bash
sudo /usr/sbin/setcap cap_net_raw,cap_net_admin=eip /home/pi/hosts3d/hsen
```

## First Start
1. Start `./hosts3d`
2. Right-click in the 3D view and choose `Configure Local Sensors (hsen)`
3. Select one or more capture interfaces
4. Click `Save`, then `Start`

On Linux, `Save` and `Start` try a one-time automatic `setcap` on the bundled `hsen` binary so local capture can work without a manual remote setup.

If that automatic step is not possible, Hosts3D shows the exact one-time command to run locally.

GUI-managed local `hsen` is also tracked so `Stop` and normal `Exit` can terminate it reliably instead of leaving a launcher shell or orphaned process behind.
On Linux, `Stop` and normal `Exit` also sweep matching bundled local `hsen` processes from the same installation, which helps clean up older orphaned local sensor processes from previous runs.
Linux local `hsen` starts are also detached from the GUI process, and brief zombie startup failures are treated as failed launches instead of as running sensors.
Hosts3D also marks its UDP receive socket close-on-exec and retries the packet-socket bind once after cleaning up orphaned managed local `hsen` processes, so stale previous runs do not silently block packet reception.

The top-right OSD also includes `PS Demo` and `Py Demo` quick-launch buttons for the bundled synthetic visualization demos.
While a demo is active, the matching OSD button stays tinted until the demo finishes.
The same `RUNTIME` strip also includes a clickable `Dynamic Host TTL` row with presets from `Off` up to `1h`.
This package keeps the quick demo scripts beside the `hosts3d` binary. The runtime still also accepts the older `testing/` layout and the usual development layout as fallbacks.
Their latest timing summaries are written to:
- `.hosts3d/demo-powershell-last.txt`
- `.hosts3d/demo-python-last.txt`

Expected demo artifact lifetime:
- packet and alert objects usually disappear again within seconds
- dynamic demo hosts age out after normal inactivity cleanup, controlled by `dynamic_host_ttl_seconds`
- the default `dynamic_host_ttl_seconds` is `300` seconds (`5` minutes)
- `dynamic_host_ttl_seconds=0` means `Off`, so dynamic hosts are no longer removed automatically

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
- The quick demo scripts stay below one minute and currently run for roughly 23-25 seconds on a normal machine.
- `README-testing.md` documents the bundled synthetic sender/demo scripts and their example invocations.
- When a local sensor interface is selected, the demo prefers the first selected local adapter IPv4 as its central demo host and uses that interface's sensor ID.
- The Python demo requires Python 3.
- The PowerShell demo requires `pwsh` if you want to use that button on Linux.

## License
Hosts3D is distributed under the GNU General Public License. See `COPYING`.
This package is provided without warranty. Current project page: <https://github.com/kaestnja/hosts3d>.
