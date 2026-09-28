# Winamp for Linux

A native Linux port of the Winamp desktop client: the classic skinned main
window, equalizer and playlist editor, built with GTK 3 and Cairo from the
code in this repository. It is not a Wine wrapper or a cross-compiled
Windows build.

The drawing, mouse and docking code is ported line by line from
`Src/Winamp` (`draw_main.cpp`, `draw_eq.cpp`, `draw_pe.cpp`, `draw_sa.cpp`,
`Ui.cpp`, `main_mouse.cpp`, `Equi.cpp`, `Peui.cpp`, `DOCK.cpp`, ...). GDI
calls are replaced by a small bitmap layer, so skins render
pixel-for-pixel like the Windows version. The title formatter is the
unmodified `Src/tagz`, command IDs come from `Src/Winamp/resource.h`, and the
built-in "Base Skin" is compiled from the bitmaps in `Src/Winamp/resource`.

## Building

Debian / Ubuntu:

```sh
sudo apt install build-essential cmake pkg-config libgtk-3-dev libzip-dev \
    libmpg123-dev libvorbis-dev libflac-dev libsndfile1-dev libopenmpt-dev \
    libpulse-dev libasound2-dev
```

Fedora:

```sh
sudo dnf install gcc-c++ cmake pkgconf gtk3-devel libzip-devel \
    mpg123-devel libvorbis-devel flac-devel libsndfile-devel libopenmpt-devel \
    pulseaudio-libs-devel alsa-lib-devel
```

Then, from the repository root:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/winamp                     # run from the build tree
sudo cmake --install build         # or install (binary, plug-ins, .desktop, icon)
```

Each plug-in is optional: when a library is missing, the plug-in that needs
it is skipped and CMake prints which ones it will build. Only GTK 3, libzip
and GLib are required.

| Option                | Default | Meaning                                              |
|-----------------------|---------|------------------------------------------------------|
| `WINAMP_BUILD_GUI`    | `ON`    | Build the player (off: only the core and plug-ins)   |
| `WINAMP_BUILD_TESTS`  | `ON`    | Build `wa_core_test`, a headless self test           |

To run the self test: `WINAMP_PLUGIN_DIR=build/Plugins build/Src/Linux/wa_core_test`

## Features

- The classic main window, equalizer and playlist editor, with windowshade
  modes, double size, "always on top", snapping and docking (docked windows
  move together), and the title bar, clutter bar, seek, volume and balance
  bars working as they do on Windows.
- Classic skins (`.wsz` / `.zip` or unpacked folders): `region.txt` window
  shapes, `pledit.txt` colours, `viscolor.txt`, and the bitmap font. Skins
  can be switched live from the skin browser (Alt+S) or the Options > Skins
  menu.
- Spectrum analyzer and oscilloscope with all the classic options (analyzer
  style, peaks, falloff speeds, scope style).
- 10-band equalizer (the original `eq10dsp` code) with presets, auto-load,
  and the Windows `winamp.q1` preset file format, so existing `.q1` / `.eqf`
  files load unchanged.
- Playlist editor: add files and folders; sorting, randomize, reverse,
  select and crop; drag and drop from file managers; M3U, M3U8 and PLS load
  and save; export as an HTML playlist; Jump to File (J) and Jump to Time
  (Ctrl+J).
- The original keyboard shortcuts (Z X C V B, the arrow keys, Ctrl+D double
  size, Alt+W/E windows, Ctrl+Alt+W shade, ...), right-click menus, and the
  Preferences, File Info and About dialogs.
- Shuffle, repeat, stop fade-out, and title formatting with the ATF
  (`[%artist% - ]$if2(%title%,$filepart(%filename%))`).
- Single instance: `winamp file.mp3` sends the files to the running player.
  Pass `-e` / `--enqueue` to add them instead of replacing the list, or use
  `--play`, `--pause`, `--stop`, `--next` or `--prev` to control it.
- MPRIS 2 over D-Bus, so media keys, desktop sound menus and `playerctl`
  work.

## Plug-ins

The player keeps the classic Winamp plug-in interface (`In_Module`,
`Out_Module`, `winampGetInModule2`, `winampGetOutModule`,
`winampGetExtendedFileInfo`). On Linux, plug-ins are shared objects loaded
with `dlopen`, and strings are UTF-8. The headers are in `Src/Linux/sdk`.

| Plug-in      | Formats / purpose                               | Library      |
|--------------|-------------------------------------------------|--------------|
| `in_mp3`     | MP3, MP2, MP1 (with ID3v1/v2 tags)               | libmpg123    |
| `in_vorbis`  | Ogg Vorbis                                       | libvorbisfile|
| `in_flac`    | FLAC                                             | libFLAC      |
| `in_wave`    | WAV, W64, AIFF, AU, VOC, CAF                     | libsndfile   |
| `in_mod`     | MOD, S3M, XM, IT and other tracker formats       | libopenmpt   |
| `out_pulse`  | PulseAudio / PipeWire output (default)           | libpulse     |
| `out_alsa`   | ALSA output                                      | alsa-lib     |
| `out_disk`   | Writes WAV files (the Nullsoft Disk Writer)      | none         |

Plug-ins are searched for in this order: `$WINAMP_PLUGIN_DIR`,
`~/.local/share/winamp/Plugins`, `Plugins/` next to the executable, and
`<prefix>/lib/winamp/Plugins`. The output plug-in is chosen under
Preferences > Plug-ins.

## Files

| What                     | Where                                                        |
|--------------------------|--------------------------------------------------------------|
| Settings                 | `~/.config/winamp/winamp.ini` (or `$WINAMP_CONFIG_DIR`)      |
| Saved playlist           | `~/.config/winamp/winamp.m3u8`                               |
| EQ presets               | `~/.config/winamp/winamp.q1`                                 |
| Skins                    | `~/.local/share/winamp/Skins`                                |
| Disk Writer output       | `$WINAMP_DISKWRITER_DIR`, or `~/Music`                       |

`winamp.ini` uses the same `[Winamp]` keys as the Windows version.

## Display servers

Classic Winamp positions its windows itself, which it needs for docking and
snapping. Wayland does not let applications do that, so the player runs
through XWayland by default. Set `WINAMP_ALLOW_WAYLAND=1` to use native
Wayland instead. On Wayland each window can still be dragged, but docking
and snapping won't work.

## Not ported

These parts of the Windows client depend on Windows-only frameworks or
closed components, and this port does not include them: modern (Wasabi /
`.wal`) skins, the Media Library, AVS and Milkdrop visualizations, video
playback, CD ripping and burning, internet streaming (URLs can be added
to a playlist, but no input plug-in plays them yet), and loading Windows
plug-in DLLs. The corresponding menu entries are left out or disabled.
