# Leapdesk KVM

Share one keyboard and mouse across several computers. Move the pointer off the
edge of one screen and it appears on the next machine; the clipboard follows.
It works over any IP network — a LAN, Tailscale, WireGuard, whatever you have.

Leapdesk KVM continues [Input Leap](https://github.com/input-leap/input-leap),
which is no longer maintained. Input Leap forked Barrier, which forked Synergy
1.x, and the goal has not changed since: one keyboard and mouse, several
machines, nothing else.

Not affiliated with Synergy, Deskflow, or Tailscale.

## Status

Small and active. Current work is on Windows 11. The macOS, Linux and BSD code
is inherited and still builds, but nothing here tests it — reports and patches
from those platforms are welcome.

No binary releases yet; build from source.

## Changes since Input Leap 3.0.3

- Builds against OpenSSL 3 and 4
- Two fewer packets per input message, which shows up on high-latency links
- Windows: the cursor stays hidden on a client the mouse has left
- Windows: suspend and resume no longer leave a dead server, a client that was
  never told to disconnect, or stale input hooks
- Windows: the background service now recognises its own processes, so a stale
  copy can't compete with the one it starts

## Build on Windows

Needs Visual Studio 2022 or 2026 with the C++ tools, CMake 3.21+, and OpenSSL.

```
winget install --id ShiningLight.OpenSSL.Dev
git clone --recurse-submodules https://github.com/bryceishunter/leapdesk-kvm
cd leapdesk-kvm
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DINPUTLEAP_BUILD_GUI=OFF -DINPUTLEAP_BUILD_TESTS=OFF -DOPENSSL_ROOT_DIR="C:/Program Files/OpenSSL-Win64"
cmake --build build --config Release
```

Use `-G "Visual Studio 17 2022"` on VS 2022. Binaries land in `build/bin/Release`.

The Qt GUI (`-DINPUTLEAP_BUILD_GUI=ON`) additionally needs Qt 6.2+ and the
Bonjour SDK, and on Windows it drives the programs through the background
service rather than running them directly. The command-line tools below do not
need either.

On Linux, macOS and BSD, see the build steps in the Input Leap wiki; they still
apply.

## Run

The server is the machine whose keyboard and mouse you are sharing. Write a
config naming both screens and how they are arranged:

```
section: screens
    desktop:
    laptop:
end
section: links
    desktop:
        right = laptop
    laptop:
        left = desktop
end
```

Screen names must match each machine's `--name` (its hostname by default).

```
# on the server
leapdesk-server.exe -f --name desktop -c leapdesk.conf

# on the other machine
leapdesk-client.exe -f --name laptop 192.0.2.10:24800
```

The server listens on TCP port 24800. The address must be the last argument on
the client. `--help` lists the rest.

## Encryption

TLS is on by default, and each side checks the other's certificate fingerprint.
The command-line tools do not create certificates — generate one per machine as
`<profile>/SSL/Leapdesk.pem` (on Windows, `%LOCALAPPDATA%\Leapdesk`) and put
each machine's SHA-256 fingerprint in the other's `SSL/Fingerprints/` file as
`v2:sha256:<hex>`.

On a network that is already private and authenticated, `--disable-crypto` on
both sides skips all of that. Note that this also removes authentication:
anything that can reach port 24800 can then connect and receive your keystrokes.

## Contributing

Issues and pull requests are welcome. Keep changes focused, and match the style
of the code around them. User-visible changes get a file in `doc/newsfragments`
(see the README there).

## License

GPL-2.0-only, inherited from Input Leap, Barrier and Synergy 1.x. See `LICENSE`.
