# rpack_builder

Command-line tool to create [Lysa](https://github.com/LysaEngine)  binary resources pack files consumed by the `ResourcesPack` runtime class via `app://` URIs.

---

## File format

```
┌──────────────────────────────────────┐
│  Magic   : "LYPACK"  (6 bytes)       │
│  Version : uint32                    │
│  Count   : uint32                    │
├──────────────────────────────────────┤
│  Directory  [Count entries]          │
│    entry[i].path      : char[256]    │
│    entry[i].offset    : uint64       │
│    entry[i].size      : uint64       │
├──────────────────────────────────────┤
│  Data blob                           │
│    raw resources (concatenated)      │
└──────────────────────────────────────┘
```

All multi-byte integers are written in the **native endianness** of the build host. Structures are packed with `#pragma pack(1)` — no padding bytes.

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## Usage

```
lypack_builder [OPTION...]

  -o, --output  arg   Output .pack file path          [required]
  -l, --list    arg   Text file listing resource paths [required]
  -b, --base    arg   Base directory on disk           [default: .]
  -v, --verbose       Print each resource being added
  -h, --help          Print help
```

### File list format

A plain text file with **one virtual path per line**, relative to `--base`.

- Empty lines are ignored.
- Lines starting with `#` are treated as comments.

```
# Lua scripts
lib/camera.lua
lib/main_scene.lua

# Fonts
res/fonts/Signwood.json
res/fonts/Signwood.png
res/fonts/Signwood.ttf

# Shaders
shaders/aces.comp.spv
shaders/bloom.comp.spv
```

The virtual paths are stored verbatim in the directory entries and used as lookup keys at runtime (e.g. `app://shaders/bloom.comp.spv`).


