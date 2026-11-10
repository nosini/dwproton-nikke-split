# Development

## Patch series

`upstream/` holds the fix as five Wine commits. They apply with `git am`
to wine-dwproton commit `dda80c1bc56000053a773cf6f8585331d1bd68f7`, the
Wine source of the published `dwproton-11.0-16` release:

1. **winewayland.drv: Retire nested popup roles before their parent.**
   Reuses 11.0-16's `wayland_surface_reparent_popups` traversal from the
   common role-clearing function, making it recursive and covering hiding
   and compositor dismissal. The previous per-surface child lists and
   upstream's individual destruction-site calls are removed. A destroyed
   window's surface is torn down under the window data lock because the
   traversal visits windows belonging to other threads.
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
5. **winewayland.drv: Don't set an app_id on saved activation tokens.**
   The driver commits a new `xdg_activation_token_v1` on every key press,
   pointer button and keyboard focus change and saves it for a later
   window activation. Mutter turns each committed token into a startup
   sequence and shows the busy cursor while a sequence that has an
   application ID is pending (`meta_startup_notification_has_pending_sequences`
   in Mutter 51.1), up to its 15-second timeout. The saved tokens are
   rarely used, so the cursor kept spinning. They now keep their surface
   and input serial, which Mutter's `token_can_activate` checks, but no
   longer set an app_id. Tokens created by `wayland_surface_activate` and
   used at once still set it.

`upstream/` also holds the drafts for offering patches 1–4 to DWProton.
Patch 5 is not part of that offer. The token code comes from the Wayland
driver DWProton shares with Proton-CachyOS and Proton-EM, where an
equivalent change was declined
([CachyOS/wine-cachyos#39](https://github.com/CachyOS/wine-cachyos/pull/39))
in favour of reworking activation or changing Mutter.

## Changes already present in 11.0-16

The released Wine source adds `wayland_surface_reparent_popups` and calls
it before three surface-destruction paths. Patch 1 now extends that
traversal instead of maintaining separate child lists. Moving it to
`wayland_surface_clear_role` also covers role clearing when a window is
hidden and when the compositor dismisses a popup, and retires deeper
nested popups recursively.

The release still discards the foreign-window software copies targeted by
patch 2, still forces `INTLWebViewHelper.exe` to X11, and still treats the
store's thick-frame non-activating dropdown as managed. Patches 2–4 remain
necessary and retain their behavior. It also refreshes the saved
activation token with an app_id on every input event; patch 5 addresses
that.

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

The busy cursor of patch 5 needs no probe. Run any Wine program, such as
`notepad`, with `WINE_GRAPHICS_DRIVER=wayland` and `WAYLAND_DEBUG=client`,
then click and type in its window. Without the patch, every button and
key event is followed by `xdg_activation_token_v1.set_app_id` and
`commit`; with it, the tokens are committed without `set_app_id`.

## Validation

The 11.0-16 build and packaged-runner probe results are recorded in
[build-11.0-16.md](build-11.0-16.md). The probes run through `proton run`
with the published runner and with the copy made by `prepare-runner.py`.

Earlier 11.0-15 revision 7 passed the same bitmap, clipping, OpenGL,
forged-message and dropdown tests, plus 25 runs of 300 two-thread popup
iterations. The series without the teardown lock failed 9 of those 25
runs. The 11.0-16 popup implementation reuses upstream's traversal instead
of that revision's child lists, so its results are recorded separately.

In NIKKE with revision 7, the store renders, closing it keeps the game
running, and the dropdowns work with the mouse, the scrollbar and the
keyboard. On 2026-11-01, the user also confirmed that NIKKE still works
with the 11.0-16 runner. Wheel-event delivery in the probe does not
establish that a store dropdown scrolls.
