# NIKKE store in split display

These experimental DWProton 11.0-14 patches keep NIKKE on native Wayland
and its launcher on gamescope's private X11. With them, closing NIKKE's
in-game store no longer ends the game, but the store still renders white.
The patches address these reproduced driver failures:

- DWProton forces `INTLWebViewHelper.exe` to X11. An OpenGL helper drawing
  into a Wayland process's child window then leaves the view white. Letting
  that helper use Wayland makes DWProton's existing OpenGL copying path work.
- Hiding an owned popup while its nested popup remains visible destroys
  Wayland objects in an invalid order. The compositor disconnects Wine and
  Wine terminates the game. Retire descendant popup roles first and ask
  their window threads to update them afterwards.

The launcher fallback remains enabled for `nikke_launcher.exe` and
`tbs_browser.exe`. No game files or registry overrides are involved.

## Source and validation

`store-split.patch` applies to DWProton's Wine commit
`b585905ec92032840e924e4e2a88382523e92ed3`. Both the 32-bit and 64-bit
`user32.dll` are rebuilt. The popup fix is in the 64-bit Unix driver used
by NIKKE's game process.

VM checks reproduce the nested-popup protocol error in unmodified DWProton
11.0-14 and GE-Proton 11-9. The patched driver survives that case. A separate
OpenGL process named `INTLWebViewHelper.exe` produces a white child view with
DWProton's fallback and visible red content with the patched `user32.dll`.
In NIKKE, closing the store keeps the game running, but the OpenGL probe
does not reproduce the store's remaining white view.

The small reproducer sources are under `tests/`. The rendering probe
launches a copy named `INTLWebViewHelper.exe` to exercise the exact fallback
rule. The popup probe hides an owner before its nested popup.

Build the probes with `python3 tests/build.py WINE_SOURCE OUTPUT_DIRECTORY`
from this directory, then run each `.exe` through the runner being tested.
For the rendering probe, keep `INTLWebViewHelper.exe` beside it and supply
both a Wayland compositor and a separate X11 display. The expected view is
a red rectangle inside the parent window. For the popup probe, success
prints `survived-first-hide` and `finished` with exit status zero.

To rebuild, use a Wine development environment with GCC, Clang/LLD,
Autoconf, Bison, Flex, Wayland, EGL, xkbcommon and xkbregistry development
files. In a task-specific build directory:

```sh
git clone --depth 1 --branch dwproton/11.0-14 \
  https://dawn.wine/dawn-winery/wine-dwproton.git wine-source
git -C wine-source rev-parse HEAD
# Check the commit above before applying the patch.
git -C wine-source apply /path/to/store-split.patch
(cd wine-source && ./autogen.sh)
mkdir wine-build
cd wine-build
../wine-source/configure --enable-archs=i386,x86_64 \
  --without-x --with-wayland --disable-tests
make -j4 dlls/winewayland.drv/winewayland.so \
  dlls/user32/x86_64-windows/user32.dll dlls/user32/i386-windows/user32.dll
```

Replace the matching files under `files/lib/wine/` in a copy of the
DWProton 11.0-14 runner with these outputs, and give the copy its own
folder name so Proton refreshes its built-in DLLs. The rest of the runner
remains the published DWProton build. Wine's source is licensed under
LGPL-2.1-or-later.
