#!/usr/bin/env python3
"""Read-only snapshot of Wine display/render modules; omit URLs and arguments."""

import hashlib
import json
from pathlib import Path
import re


PATCHED_USER32 = {
    "3ce4e0e70c6d012e825b654ca41be70e7f5adf1b93b9f2ef35980e4925717c94",
    "eb0d1f7d2cd17bf719d6cfa891e6344790cab78ac031c13765b7da8bc92bdade",
}
PATCHED_WIN32U = {"b1a633f4264f7d79e5ea7e23504e8597334add179d7baecb77e46d4014b84c78"}
PATCHED_WINEWAYLAND = {"7e094785085a5d04b58eea7c8c0a9d3c961693709dc52b7f4e0edac0b1b7a319"}
PATCHED_WINEX11 = {"ce4de99241abe83e1f22d9773308b762042a411e425d8f1644fef85d46174d4d"}
# Older builds, to tell a runner that was not switched from a wrong module.
OLDER_BUILDS = {
    "b81336bac0efd33c6f1eea0d196f54e3f9b00818383c62952da728ee3596053f": "11.0-16 revision 8",
    "13faede5b4653b242cebb55061f93aa17e2230052b0068bd1d912889931a6763": "11.0-15 revision 7",
    "8fb4d65b534d3fcba58aad4fa15039d2c68abda3357f51be9958b9efc10fb531": "11.0-15 revision 7",
    "baa3a226e5e40b2ffb2f19415ff40f7c2c754934442078327356a5cd55aefeed": "11.0-15 revision 7",
    "b2bba839e4a7cafc5de0e9f03e5aeed7fd75ba8566eedb37d8c671d6a8559709": "11.0-15 revision 7",
    "86a2af537ca16025850745bdd1eb440e834ce5421b3f71d75e3d7f8653cf7561": "11.0-14 revision 6",
    "cb18c05f2d76eb734d4ca24a29da5cff109d1726ad717188a1a8086286b8ce31": "11.0-14 revision 6",
    "1a63c3754c5ff26de5984af229a5e8a5fd1ee160342ff3950adb47487e8c7310": "11.0-14 revision 6",
    "0e20269ed080bd9927f010e1290bd7d85b65acd443c2a5119e7412ad41e10a88": "11.0-14 revision 6",
    "ab8eab28c7f5513328dfaa2c790ca3490861d02afbf5d5f9368494208a4bdabe": "revision 2",
    "ca315d1e2822d023e01d9ec2cc4a28adb381c854a07f8a1e0b7b6197580c21cb": "revision 3",
    "dc7673724240ffa949ffd4b20016f4fd31cec736d8ab92e93a19152aa605495b": "revisions 2 to 4",
    "49cfc37c56ba1e9c089223f3dfdd95b0bbd786f6208ac6ff6d41cf2374265b93": "revision 5",
}
PATCHED = {"user32.dll": PATCHED_USER32, "win32u.so": PATCHED_WIN32U,
           "winewayland.so": PATCHED_WINEWAYLAND, "winex11.so": PATCHED_WINEX11}
MODULE = re.compile(
    r"(?:winewayland|winex11|opengl32|winevulkan|vulkan-1|d3d\d+|dxgi|"
    r"libcef|libegl|libglesv2|libvulkan|vk_swiftshader)\.(?:dll|so)(?:\..*)?$",
    re.I,
)
FLAG = re.compile(
    r"--(?:type|use-angle|use-gl|use-vulkan|ozone-platform)=[a-zA-Z0-9_-]+"
    r"|--(?:disable-gpu|disable-gpu-compositing|disable-software-rasterizer|"
    r"in-process-gpu|single-process)(?=\s|\x00|$)"
)
ENV_KEYS = {"WINE_GRAPHICS_DRIVER", "PROTON_USE_X11_EXCLUSIVE", "DISPLAY", "WAYLAND_DISPLAY"}
hashes = {}
records = []
unreadable = 0

for proc in sorted(Path("/proc").glob("[0-9]*"), key=lambda p: int(p.name)):
    try:
        lines = (proc / "maps").read_text().splitlines()
    except PermissionError:
        unreadable += 1
        continue
    except OSError:
        continue
    paths = set()
    for line in lines:
        fields = line.split(maxsplit=5)
        if len(fields) == 6 and fields[5].startswith("/"):
            paths.add(re.sub(r"\\([0-7]{3})", lambda m: chr(int(m[1], 8)), fields[5]))
    modules = sorted({Path(p).name for p in paths if MODULE.fullmatch(Path(p).name)})
    exes = sorted({Path(p).name for p in paths if p.lower().endswith(".exe")})
    if not modules and not any("nikke" in p.lower() or "webview" in p.lower() for p in exes):
        continue
    record = {"pid": int(proc.name), "executables": exes, "loaded_modules": modules}
    try:
        record["name"] = (proc / "comm").read_text().strip()
        args = (proc / "cmdline").read_bytes().decode(errors="replace")
        record["browser_flags"] = sorted(set(FLAG.findall(args)))
        env = (proc / "environ").read_bytes().decode(errors="replace").split("\0")
        record["display_environment"] = {
            key: value for item in env if "=" in item
            for key, value in [item.split("=", 1)] if key in ENV_KEYS
        }
    except OSError:
        record["process_details_unavailable"] = True
    disk_modules = {name: [] for name in PATCHED}
    for path in sorted(paths):
        module_name = Path(path).name.lower()
        if module_name not in disk_modules:
            continue
        expected = PATCHED[module_name]
        try:
            mapped_file = proc / "root" / path.lstrip("/")
            stat = mapped_file.stat()
            key = (stat.st_dev, stat.st_ino, stat.st_mtime_ns, stat.st_size)
            if key not in hashes:
                with mapped_file.open("rb") as stream:
                    hashes[key] = hashlib.file_digest(stream, "sha256").hexdigest()
            digest = hashes[key]
            entry = {"sha256": digest, "matches_prepared_patch": digest in expected}
            if digest in OLDER_BUILDS:
                entry["older_build"] = OLDER_BUILDS[digest]
            disk_modules[module_name].append(entry)
        except OSError:
            disk_modules[module_name].append({"unreadable": True})
    record["user32_on_disk"] = disk_modules["user32.dll"]
    record["win32u_on_disk"] = disk_modules["win32u.so"]
    record["winewayland_on_disk"] = disk_modules["winewayland.so"]
    record["winex11_on_disk"] = disk_modules["winex11.so"]
    records.append(record)

print(json.dumps({
    "note": "Loaded modules alone do not establish which rendering API is active.",
    "processes": records,
    "unreadable_process_maps": unreadable,
}, indent=2))
