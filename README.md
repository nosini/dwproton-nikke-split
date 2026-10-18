# NIKKE store in split display

This experimental DWProton 11.0-14 runner fix keeps NIKKE on native Wayland
and its launcher on gamescope's private X11. With it, NIKKE's in-game store
renders, and closing the store no longer ends the game. Since revision 4,
the store's dropdown lists, such as the billing city, stay open and can be
used with the mouse, the scrollbar and the keyboard. The mouse wheel does
not scroll them yet. The patches address these reproduced driver failures:

- DWProton forces `INTLWebViewHelper.exe` to X11. An OpenGL helper drawing
  into a Wayland process's child window then leaves the view white. Letting
  that helper use Wayland makes DWProton's existing OpenGL copying path work.
- Chromium's software renderer can copy a bitmap into an HWND owned by
  another process. With Wayland, Wine's null GDI driver silently discards
  these copies. Queue clipped `SRCCOPY` bitmap updates until `ReleaseDC`,
  then let the top-level window's process paint them into its surface.
- Hiding an owned popup while its nested popup remains visible destroys
  Wayland objects in an invalid order. The compositor disconnects Wine and
  Wine terminates the game. Retire descendant popup roles first and ask
  their window threads to update them afterwards.
- The store's browser runs inside the game process and draws a `<select>`
  dropdown in an unowned, non-activating tool window with a resizing
  frame. Wine's Wayland driver treats every framed window as an
  application window and maps the dropdown as a separate toplevel. The
  compositor focuses it, the game loses keyboard focus, and the store
  closes the dropdown at once. Non-activating tool popups are now
  unmanaged, so they become an ungrabbed `xdg_popup` of the window beside
  them. If that window belongs to another process, Wayland cannot attach
  to it, so the dropdown is attached to the process's foreground window.

The launcher fallback remains enabled for `nikke_launcher.exe` and
`tbs_browser.exe`. No game files or registry overrides are involved.

The submission series in `upstream/` now targets the DWProton 11.0-15
source branch. The installer and bundled modules below remain for the
published 11.0-14 runner; modules built against 11.0-15 must not be mixed
into that runner. See [the 11.0-15 build report](upstream/build-11.0-15.md).

## Try the prepared modules

Close NIKKE and its launcher, then run from this checkout:

```sh
python3 prepare-runner.py
```

The script finds an unmodified DWProton 11.0-14 installation, checks its
four original modules against their published-build checksums, and makes
a separate `DWProton-11.0-14-nikke-split-r6` runner. The original and the
earlier experimental runners are preserved.
An installation elsewhere can be supplied as the first argument.

In NIKKE's sandbox settings, select that runner, keep **Native Wayland +
private X11 (split)**, and clear **X11 executable**. An explicit
`INTLWebViewHelper.exe` override would force the incompatible path back on.
Launch again and test opening, closing, and reopening the store and its
dropdown lists.

The prepared modules are for an x86-64 Linux system with glibc 2.38 or
newer and the normal Wine Wayland libraries. `modules/` holds the built
replacements and their checksum manifest.

To undo the change, select the original runner again. The custom runner
has a distinct path so Proton refreshes built-in DLLs when switching.
Its discovery timestamp is zero, so automatic runner selection continues
to prefer released builds.

## Source and validation

`store-split.patch` applies to DWProton's Wine commit
`b585905ec92032840e924e4e2a88382523e92ed3`. Both the 32-bit and 64-bit
`user32.dll` are rebuilt. The popup fix is in the 64-bit Unix driver used
by NIKKE's game process. `software-copy.patch` applies to the same source
and rebuilds the 64-bit Unix `win32u.so` used by the game and store helpers.
`noactivate-popups.patch` applies after both and changes the 64-bit Unix
`winewayland.so` again.

VM checks reproduce the nested-popup protocol error in unmodified DWProton
11.0-14 and GE-Proton 11-9. The patched driver survives that case. A separate
OpenGL process named `INTLWebViewHelper.exe` produces a white child view with
DWProton's fallback and visible red content with the patched `user32.dll`.
The OpenGL probe alone did not reproduce the actual store's rendering
failure: the real store's process snapshot shows patched `user32.dll`,
Wayland, SwiftShader WebGL and disabled GPU compositing. The software bitmap
probe fails its pixel check with the first build and passes with revision 2,
including disjoint clipping regions and parent-owned child windows.
Since revision 5, the receiving process only uses and unmaps a posted
image if it is a whole mapped view of the size its header describes.
Since revision 6, it also requires a read-only view that starts with the
sender's tag, checks private copies of the header and region data, and
leaves any other mapping alone; the sender drops its writable view before
posting. `tests/forged-probe.c` posts the message with the address of an
unrelated mapping: revision 5 unmaps it, revision 6 does not.
The OpenGL probe still passes with revision 2. In NIKKE, revision 2 renders
the store, and opening and closing it keeps the game running.

A Wine trace from NIKKE shows each store dropdown becoming a toplevel and
taking keyboard focus from the game window. A dropdown probe with the same
window styles reproduces this under Mutter with revisions 2 and 3: the
popup is activated and the browser child loses focus. With revision 4 the
child keeps focus, and a click, a wheel turn and a key press each reach
their window, also through Proton, where a console window lies beside the
dropdown. Hiding and reshowing the popup works, and the earlier probes
still pass. In NIKKE, revision 4 keeps the City dropdown open; selecting
with the mouse or keyboard and dragging its scrollbar work, but the mouse
wheel does nothing over it. Revision 3's
cross-process popup forwarding is dropped: NIKKE's dropdown is created in
the game process, not in a helper.

If the store renders white again, leave it open and run from this checkout:

```sh
sandbox-attach --exec -- python3 -I - < diagnose-store.py
```

If multiple sandboxes are running, add `--name NAME` before `--exec`.
The script only reads process metadata. It reports loaded display/graphics
modules, selected browser flags, display settings, and whether the mapped
`user32.dll`, `win32u.so` and `winewayland.so` files on disk match the
prepared patches. It omits full command lines and URLs. Loaded modules alone do not prove the active rendering API.

The small reproducer sources are under `tests/`. The rendering probe
launches a copy named `INTLWebViewHelper.exe` to exercise the exact fallback
rule. The popup probe hides an owner before its nested popup. The dropdown
probe, in `dropdown/` of the output directory, puts a focused child into a
window and opens a non-activating popup over it. With `--local --chromium`
it does so in one process with the styles of NIKKE's store dropdown.
Without `--local`, an `INTLWebViewHelper.exe` copy creates child and popup
from another process; that case is not handled.

Build the probes with `python3 tests/build.py WINE_SOURCE OUTPUT_DIRECTORY`
from this directory, then run each `.exe` through the runner being tested.
For the rendering probe, keep `INTLWebViewHelper.exe` beside it and supply
both a Wayland compositor and a separate X11 display. The expected view is
a red rectangle inside the parent window. For the popup probe, success
prints `survived-first-hide` and `finished` with exit status zero.
For software copies, run `render-probe.exe --gdi --foreign-window`, or
`render-probe.exe --gdi-clipped --foreign-window` to check clipping as well.
Success prints `parent-pixels-ok` and exits with status zero; the first
build exits with status 7. Omitting `--foreign-window` makes the helper
own the child while the parent still owns the top-level window.
Run the dropdown probe on a compositor with a keyboard that focuses new
windows, as GNOME does. It exits with status zero if the child keeps focus;
with `--local --chromium`, revisions 2 and 3 exit with status 8. With `--wait-click`, `--wait-wheel` or
`--wait-key` it also waits for a click or wheel turn on the popup or a key
press, exiting with status 9, 11 or 10 if missing. `--reopen` hides and
shows the popup again.

To rebuild, use a Wine development environment with GCC, Clang/LLD,
Autoconf, Bison, Flex, Wayland, EGL, xkbcommon and xkbregistry development
files. In a task-specific build directory:

```sh
git clone --depth 1 --branch dwproton/11.0-14 \
  https://dawn.wine/dawn-winery/wine-dwproton.git wine-source
git -C wine-source rev-parse HEAD
# Check the commit above before applying the patch.
git -C wine-source apply /path/to/store-split.patch
git -C wine-source apply /path/to/software-copy.patch
git -C wine-source apply /path/to/noactivate-popups.patch
(cd wine-source && ./autogen.sh)
mkdir wine-build
cd wine-build
../wine-source/configure --enable-archs=i386,x86_64 \
  --without-x --with-wayland --disable-tests
make -j4 dlls/winewayland.drv/winewayland.so \
  dlls/user32/x86_64-windows/user32.dll dlls/user32/i386-windows/user32.dll \
  dlls/win32u/win32u.so
```

Copy these four outputs into the corresponding `modules/files/lib/wine/`
directories and regenerate `modules/sha256.json` with SHA-256 values keyed
by their relative paths. This rebuilds only the replacement modules; the
rest of the runner remains the published DWProton build. Wine's source is
licensed under LGPL-2.1-or-later.
