# Development

## Patch series

`upstream/` holds the fix as four Wine commits. They apply with `git am`
to wine-dwproton commit `b75a4125963073b01d2b3bfce6060d697d02066b`, the
Wine source of the published `dwproton-11.0-15` release:

1. **winewayland.drv: Retire nested popup roles before their parent.**
   Each surface keeps a list of its live child popups, and clearing a
   surface's role clears its children's roles first, as xdg-shell
   requires. Their window threads are asked to update the role afterwards;
   compositor dismissal goes through the same path. Because these lists
   link surfaces of windows that can belong to other threads, a destroyed
   window's surface is torn down with the window data lock held, as the
   driver's other callers of `wayland_surface_destroy` already do.
2. **win32u: Forward bitmap copies into windows of another process.** For a
   DC whose top-level window belongs to another process and that has no
   window surface, clipped 32-bit `SRCCOPY` images are queued. On
   `ReleaseDC` they are mapped read-only into the top-level's process and
   announced with an internal message; that process paints them through a
   DC of the target window. The receiver only accepts a whole read-only
   view that starts with the sender's tag, checks private copies of the
   header and region data, and leaves any other mapping alone.
3. **user32: Let INTLWebViewHelper.exe use the Wayland driver.** Removes the
   helper from DWProton's forced X11 list. Depends on 2.
4. **winewayland.drv: Keep non-activating tool popups unmanaged.**
   `WS_POPUP` windows with `WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW` that are not
   being activated are no longer managed because of a resizing frame, so
   they become ungrabbed `xdg_popup`s. If the window beside an unowned
   popup belongs to another process, the foreground window is used as its
   parent.

`upstream/` also holds the drafts for offering the series to DWProton.

## GE-Proton's fix for the popup order

GE-Proton fixed the same popup destruction order independently, for
crashes in AION 2 and Battle.net ([issue 834][ge-issue]), in commit
[b492349d][ge-commit] as
[0072-winewayland-destroy-owned-popups-before-their-parent-role.patch][ge-patch].
It is included from GE-Proton11-10. Before a surface with an `xdg_surface`
loses its role, that patch walks all windows under the window data lock
and clears the roles of the popups the window owns, recursively. It is
written for GE-Proton's own Wayland driver and does not apply to
DWProton's.

The first patch here keeps a list of live child popups per surface
instead, so only the affected popups are visited, and popups dismissed by
the compositor go through the same ordered teardown. Because those lists
link surfaces of windows on different threads, a destroyed window's
surface is torn down with the window data lock held.

[ge-issue]: https://github.com/GloriousEggroll/proton-ge-custom/issues/834
[ge-commit]: https://github.com/GloriousEggroll/proton-ge-custom/commit/b492349d0233dcff26c5a585ddd3f2a4c8c3095c
[ge-patch]: https://github.com/GloriousEggroll/proton-ge-custom/blob/b492349d0233dcff26c5a585ddd3f2a4c8c3095c/patches/wine-hotfixes/em-fixups/0072-winewayland-destroy-owned-popups-before-their-parent-role.patch

## Rebuilding the modules

The build needs a Wine development environment: GCC, Clang and LLD,
Autoconf, Bison, Flex, and the Wayland, EGL, xkbcommon and xkbregistry
development files. From this checkout:

```sh
./build-modules.sh /path/to/new/work/directory
```

The script fetches the release's Wine commit, checks it, applies the
series, configures with

```sh
--enable-archs=i386,x86_64 --without-x --with-wayland --disable-tests
```

and builds only the 64-bit Unix `winewayland.so` and `win32u.so` and the
32-bit and 64-bit `user32.dll`. It then replaces the files under
`modules/` and regenerates `modules/sha256.json`. A second argument sets
the number of parallel jobs. The rest of the runner stays the published
DWProton build, which is why `prepare-runner.py` checks the release's
original modules before replacing them. Moving to a newer release means
updating the commit in `build-modules.sh`, rebasing the series, and
updating the original checksums and runner name in `prepare-runner.py`.

## Reproducers

`tests/` holds small Win32 programs for each failure. Build them with

```sh
python3 tests/build.py WINE_SOURCE OUTPUT_DIRECTORY
```

and run each `.exe` with `proton run` from the runner being tested, with
`WINE_GRAPHICS_DRIVER=wayland` and a Wayland compositor.

- `window-probe.exe` hides an owner before its nested popup. Success prints
  `survived-first-hide` and `finished` and exits with status zero. Mutter
  reports the protocol error for the unpatched driver; Weston accepts the
  invalid order, so use Mutter for the negative control.
- `popup-race-probe.exe [ITERATIONS]` lets a worker thread map a nested
  popup pair owned by the main thread's window and exit without hiding
  them, while the main thread hides that window. Wine destroys a thread's
  windows without hiding them first, so this exercises popup teardown from
  two threads at once. It exits with status zero after the iterations
  (200 by default) and needs Mutter, like the popup probe.
- `render-probe.exe` launches a copy of itself named `INTLWebViewHelper.exe`
  to exercise the X11 fallback rule. Keep that copy beside it. Without
  arguments it draws with OpenGL and expects a red rectangle inside the
  parent window. `--gdi --foreign-window` checks software copies into a
  window of another process, and `--gdi-clipped --foreign-window` adds
  disjoint clipping. Success prints `parent-pixels-ok` and exits with
  status zero; with copies discarded it exits with status 7. Omitting
  `--foreign-window` makes the helper own the child while the parent still
  owns the top-level window.
- `forged-probe.exe` posts the internal bitmap message with the addresses
  of unrelated writable and read-only mappings. It exits with status zero
  if the receiver leaves both mapped.
- `dropdown/dropdown-probe.exe --local --chromium` opens a non-activating
  popup with the styles of the store's dropdown over a focused child. Run
  it on a compositor with a keyboard that focuses new windows, as GNOME
  does. It exits with status zero if the child keeps focus and with
  status 8 if the popup took it. `--wait-click`, `--wait-wheel` and
  `--wait-key` also wait for a click or wheel turn on the popup or a key
  press, exiting with status 9, 11 or 10 if missing; `--reopen` hides and
  shows the popup again. Without `--local`, a helper process creates the
  child and the popup, which is not handled.

## Validation

The probes were run through `proton run` with the published DWProton
11.0-15 runner and with the runner made by `prepare-runner.py`, on
headless Mutter and Weston:

| Check | DWProton 11.0-15 | Revision 7 |
| --- | --- | --- |
| Nested popup teardown, Mutter | Protocol error | Survives |
| Popup teardown from two threads, Mutter | Protocol error | Survives 25 of 25 runs |
| Cross-process software bitmap, Weston | Discarded, status 7 | Correct pixels |
| Disjoint bitmap clipping, Weston | — | Passes |
| Helper-owned child under a foreign top-level, Weston | — | Passes |
| OpenGL output from the helper, Weston | — | Passes |
| Forged bitmap message, Weston | — | Mappings left alone |
| Chromium-style dropdown, Mutter | Loses focus, status 8 | Keeps focus; click, wheel and key delivered |
| Dropdown hide and reopen, Mutter | — | Passes |

Each two-thread run has 300 iterations. The same series without the
teardown lock failed 9 of 25 such runs: 8 with the protocol error, and
once Mutter dropped the connection over an unreadable request. The
dropdown probe shows that a wheel event is delivered, not that a store
dropdown scrolls.

In NIKKE with revision 7, the store renders, closing it keeps the game
running, and the dropdowns work with the mouse, the scrollbar and the
keyboard.
