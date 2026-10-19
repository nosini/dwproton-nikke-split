# NIKKE store in split display

This experimental fix for the DWProton 11.0-15 runner keeps NIKKE on native
Wayland and its launcher on gamescope's private X11. With it, NIKKE's
in-game store renders, and closing the store no longer ends the game. The
store's dropdown lists, such as the billing city, stay open and can be used
with the mouse, the scrollbar and the keyboard. The mouse wheel does not
scroll them yet.

No game files or registry overrides are involved. The fix replaces four
Wine modules in a separate copy of the runner; the original runner stays
as it is.

## Install

Install DWProton 11.0-15, close NIKKE and its launcher, then run from this
checkout:

```sh
python3 prepare-runner.py
```

The script finds an unmodified DWProton 11.0-15 installation, checks its
four original modules against the published build, and makes a separate
`DWProton-11.0-15-nikke-split-r7` runner in
`~/.local/share/Steam/compatibilitytools.d`. The original runner and earlier experimental
runners are left alone. An installation elsewhere can be supplied as the
first argument, and `--destination` chooses where the new runner goes.

The prepared modules are for an x86-64 Linux system with glibc 2.38 or
newer and the normal Wine Wayland libraries.

## Use

In NIKKE's sandbox settings, select `DWProton-11.0-15-nikke-split-r7`,
keep **Native Wayland + private X11 (split)**, and clear **X11
executable**. An explicit `INTLWebViewHelper.exe` override would force the
incompatible path back on. Launch the game and open, close and reopen the
store and its dropdown lists.

The custom runner's discovery timestamp is zero, so automatic runner
selection keeps preferring released builds.

To undo the change, select the original runner again. The custom runner
has its own path, so Proton refreshes its built-in DLLs when switching.

## If the store renders white again

Leave the store open and run from this checkout:

```sh
sandbox-attach --exec -- python3 -I - < diagnose-store.py
```

If several sandboxes are running, add `--name NAME` before `--exec`. The
script only reads process metadata. It reports loaded display and graphics
modules, selected browser flags and display settings, and whether the
mapped `user32.dll`, `win32u.so` and `winewayland.so` files match this fix,
naming an older revision if one is found. It omits full command lines and
URLs. Loaded modules alone do not prove which rendering API is active.

## What the fix changes

- DWProton forces `INTLWebViewHelper.exe`, the store's browser helper, to
  X11. A helper drawing into a Wayland process's child window then leaves
  the store white. The helper now uses Wayland.
- Chromium's software renderer copies each frame into a window owned by
  another process. With Wayland, Wine silently discards those copies. They
  are now forwarded to the process that owns the window, which paints them.
- Closing the store hides a popup while a popup it owns is still visible.
  Wine then destroys the Wayland popups in an order the compositor rejects,
  and the compositor disconnects the game. Nested popups are now retired
  from the innermost one outwards, also when their windows belong to
  different threads.
- The store draws each dropdown in a separate frameless tool window. Wine
  maps it as an ordinary application window, the compositor focuses it,
  and the store closes the dropdown at once. Such windows now become
  popups of the window beside them and leave the keyboard focus where it
  is.

The launcher fallback to X11 stays enabled for `nikke_launcher.exe` and
`tbs_browser.exe`. The changes apply to every game started with this
runner, not only NIKKE.

## Development

The patch series, how to rebuild the modules from DWProton's source, and
the small Win32 reproducers are described in
[docs/development.md](docs/development.md). Wine's source is licensed
under LGPL-2.1-or-later.
