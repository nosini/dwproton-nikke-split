# DWProton 11.0-16 split-display build

Built and validated on 2026-11-01 as `DWProton-11.0-16-nikke-split-r8`.
On 2026-11-01, the user confirmed that NIKKE still works with this build.
Individual store and dropdown interactions were not separately reported
in that confirmation.

## Source and packaging

- Published release: [dwproton-11.0-16](https://dawn.wine/dawn-winery/dwproton/releases/tag/dwproton-11.0-16).
- Proton commit: `4cde85c06d468c231547c7c3f6c889ca3f1317d7`.
- Release Wine commit: `dda80c1bc56000053a773cf6f8585331d1bd68f7`.
- Patched Wine tree: `770952ca71ef92bfdf557ef13ef26fa104e25073`.

The downloaded release archive passed its published SHA-512 check. The
four patches in `upstream/` apply to the release Wine commit and reproduce
the built source tree. Configuration:

```sh
--enable-archs=i386,x86_64 --without-x --with-wayland --disable-tests
```

The build produces `winewayland.so`, `win32u.so`, and both PE architectures
of `user32.dll`. Their hashes are in `modules/sha256.json`. The Unix modules
require glibc 2.38 or newer and have the same runtime library dependencies
as the published modules. The rest of the runner comes from the release.

The updated installer was used to create the tested runner. It rejects a
stock 11.0-15 source and an existing destination. Its installed module
hashes match the shipped manifest.

## Removed overlap

11.0-16 already walks owned popup surfaces before three destruction paths.
Patch 1 now reuses that traversal from `wayland_surface_clear_role`, so it
also handles hiding, deeper descendants, and compositor dismissal. The
previous popup child lists, their list initialization and insertion, and
the now-redundant calls at individual destruction sites are removed.
Destruction retains the lock while visiting other windows' surfaces.

The release still discards the software copies targeted by patch 2,
forces `INTLWebViewHelper.exe` onto X11, and treats the store's dropdown
styles as managed. Patches 2–4 retain their behavior.

## Packaged-runner checks

All probes were run through `proton run`, using isolated prefixes and
headless Mutter 50.5 or Weston 16. All 13 checks of the patched runner
passed, including five runs of 300 popup race iterations.

| Check | Stock 11.0-16 | Split-display r8 |
| --- | --- | --- |
| Hide an owner before its nested popup, Mutter | Protocol error, exit 1 | Exit 0 |
| Thread exit while another thread hides the owner, Mutter | Exit 0, 100 iterations | Five runs of 300 iterations, all exit 0 |
| Software bitmap into a foreign window, Weston | Incorrect pixels, exit 7 | Correct pixels, exit 0 |
| Disjoint bitmap clipping, Weston | — | Exit 0 |
| Helper-owned child under a foreign top-level, Weston | — | Exit 0 |
| OpenGL output from helper, Weston | — | Exit 0 |
| Forged bitmap messages, Weston | — | Unrelated mappings retained, exit 0 |
| Chromium-style dropdown, Mutter | Loses focus, exit 8 | Keeps focus; click, wheel and key delivered, exit 0 |
| Dropdown hide and reopen, Mutter | — | Exit 0 |

The dropdown probe is built with the Windows GUI subsystem. The console
subsystem spawned a separate conhost window that stole focus before the
test reached its dropdown, producing exit 3. That harness precondition
failure was corrected before comparing dropdown behavior.

Wheel delivery in the probe does not prove that NIKKE's dropdown scrolls.
The known bitmap forwarding limits remain: only the supported 32-bit
`SRCCOPY` path is forwarded, updates flush on `ReleaseDC`, and a mapped
view can leak if its recipient window dies before handling the message.

## Revision 9: busy cursor on GNOME

Built and checked on 2026-11-10 as `DWProton-11.0-16-nikke-split-r9`. It
adds patch 5, which stops saved activation tokens from setting an app_id.
The five patches applied to the release Wine commit give tree
`acc086a3fa23326a06dfd8907b0f94bd43d888b4`; patches 1–4 alone still give
the r8 tree above.

Only `winewayland.so` is replaced. Patch 5 does not touch `win32u.so` or
`user32.dll`, so r9 keeps the validated r8 builds of those modules. A
rebuild with `build-modules.sh` produces equivalent but not byte-identical
files for them. The new `winewayland.so` requires glibc 2.38 or newer and
has the same library dependencies as before.

The checks used Mutter 51.1 in headless mode with RemoteDesktop virtual
input. They started the runner's `wine` directly in fresh prefixes rather
than through `proton run`. Notepad was clicked five times and typed in
five times, giving ten button and ten key events plus one keyboard focus
event:

| Runner | Tokens committed | With `set_app_id` | With `set_serial` |
| --- | --- | --- | --- |
| Stock 11.0-16 | 21 | 21 | 21 |
| Split-display r8 | 21 | 21 | 21 |
| Split-display r9 | 21 | 0 | 21 |
| r9 made by `prepare-runner.py` | 21 | 0 | 21 |

Mutter 51.1 shows the busy cursor only for pending startup sequences with
an application ID, and its activation check uses the token's surface and
serial, not its app_id. On 2026-11-10, the user confirmed that r9 fixes
the busy cursor on their GNOME desktop.

On r9, the nested popup probe exited 0, three runs of 300 popup race
iterations exited 0, and the dropdown probe kept focus, received the
click, wheel and key, and survived reopening (all exit 0). r8 gave the
same results for the popup and dropdown input checks.

## Revision 10: Vulkan launchers on X servers without DRI3

Built and checked on 2026-11-12 as `DWProton-11.0-16-nikke-split-r10`. It
adds patch 6, which selects Mesa's software Vulkan WSI in X11 processes
when the X server has no DRI3. The six patches applied to the release
Wine commit give tree `17f0d5db683a01c579aba8ed162c994cc62f1415`;
patches 1–5 alone still give the r9 tree above.

Only `winex11.so` is added. Patch 6 does not touch the other four
modules, so r10 keeps the r9 builds of them. `build-modules.sh` now
configures with `--with-x` and builds `winex11.so` as well. The new
module needs glibc 2.38 or newer, links the same libraries as the
published one, and loads the same eleven X11, OpenGL and Vulkan sonames.
`prepare-runner.py` checks the published `winex11.so` as a fifth original.

The checks ran on the agent VM, which has no 3D GPU, with Mesa 26.2.5's
Venus driver talking to `virgl_test_server --venus --no-virgl` over vtest,
backed by lavapipe. The runner's `wine` was started directly in fresh
prefixes on the X11 driver, running `vulkan-probe.exe`:

| X server | Runner | Result |
| --- | --- | --- |
| Xvfb 21.1.25 (no DRI3) | Stock 11.0-16 | `no-presentation-support`, exit 3 |
| Xvfb 21.1.25 (no DRI3) | r10 made by `prepare-runner.py` | 60 frames presented, exit 0; all 76,800 window pixels red in Xvfb's framebuffer |
| Xvfb, `MESA_VK_WSI_DEBUG=linear` set | r10 | `no-presentation-support`, exit 3; the variable was left alone |
| Xwayland 24.1.14 under headless Weston 16 (no glamor, so no DRI3) | Stock 11.0-16 | `no-presentation-support`, exit 3 |
| Same | r10 | 60 frames presented, exit 0 |

With `WINEDEBUG=+vulkan`, r10 logged the selection once in each X11
process (the probe and `explorer.exe`). Native `vkcube --wsi xcb` on the
same Xvfb crashed after Mesa's "No DRI3 support detected" message and
ran with `MESA_VK_WSI_DEBUG=sw`, matching the white launcher seen with
run-in-sandbox's `--virtual-gpu`.

On 2026-11-12, the user confirmed that NIKKE's launcher works with r10
under run-in-sandbox's `--virtual-gpu` in split display, with no
`MESA_VK_WSI_DEBUG` set by hand. Not checked: an X server with DRI3,
where the patch returns without setting anything (the VM has no GPU
that gives Xwayland glamor).

## Revision 11: review fixes

Built and checked on 2026-11-13 as `DWProton-11.0-16-nikke-split-r11`. A
code review of r10 found several real defects in patches 1, 2 and 4,
fixed here:

- Patch 1: a destroyed window was removed from the window tree before its
  popup role was torn down. A thread hiding its owner in that gap could
  destroy the owner's popup first, which Mutter rejects. Its role, with
  its own popups first, is now cleared while it is still in the tree.
  Clearing a popup's old role can retire its new owner; the popup then
  stays unmapped instead of being created without a parent. The tree is
  only walked for surfaces with an `xdg_surface`.
- Patch 2: the format reply to the text renderer's query now includes the
  image size, which the renderer uses to size its buffer. Copies that are
  not forwarded (other raster operations, out-of-range sources) report
  success again instead of making `BitBlt` fail. Fully clipped copies and
  copies to the desktop window are not forwarded. A DC that is never
  released queues at most 16 copies or 128 MiB. The receiver's size check
  cannot wrap on 32-bit hosts.
- Patch 4: windows with a caption, such as floating tool palettes, stay
  managed.
- `prepare-runner.py` stops if the release's tool name is missing from
  `compatibilitytool.vdf`. Both scripts work with Python older than 3.11.

The six patches applied to the release Wine commit give tree
`0bf0fc8ecb845e220dd04cf7379d2f9c728fb4e5`. Only `win32u.so` and
`winewayland.so` are replaced; r11 keeps the r10 builds of `user32.dll`
and `winex11.so`, whose sources did not change. Both new modules need
glibc 2.38 or newer and link the same libraries as before. The runner
made by `prepare-runner.py` matches `modules/sha256.json`.

All probes ran through `proton run` with the published runner, r10 and
r11, in fresh prefixes on headless Mutter 51.1 or Weston:

| Check | Stock 11.0-16 | r10 | r11 |
| --- | --- | --- | --- |
| Hide an owner before its nested popup, Mutter | Exit 1 | Exit 0 | Exit 0 |
| Popup race, 300 iterations, Mutter | 3 of 3 runs exit 1 | 3 of 3 exit 0 | 10 of 10 exit 0 |
| Software bitmap into a foreign window, Weston | Exit 7 | Exit 7 (`SRCINVERT` copy failed) | Exit 0 |
| Disjoint bitmap clipping, Weston | Exit 7 | Exit 7 | Exit 0 |
| Helper-owned child under a foreign top-level, Weston | Exit 7 | Exit 7 | Exit 0 |
| OpenGL output from helper, Weston | Exit 0 | Exit 0 | Exit 0 |
| Anti-aliased opaque text into a foreign window, Weston | Exit 0 | Exit 0 | Exit 0 |
| Forged bitmap messages, Weston | Exit 0 | Exit 0 | Exit 0 |
| Chromium-style dropdown with click, wheel and key, Mutter | Exit 8 | — | Exit 0 |
| Dropdown hide and reopen, Mutter | Exit 8 | Exit 0 | Exit 0 |
| Captioned tool palette stays managed, Mutter | Exit 8 | Exit 0 (made a popup) | Exit 8 (managed) |

The r10 bitmap failures come from the new `SRCINVERT` check in
`render-probe`: r10 made that `BitBlt` fail, so the helper stopped before
copying. The first r11 build cleared only a destroyed window's popups
before removing it from the tree, and failed 2 of 3 race runs; the
released r11 clears the window's own role as well. The text probe does
not crash r10 either, so it only shows that r11 handles that path, not
that it reproduces the old buffer overrun. The dropdown input check for
r10 used wrong click coordinates and was not repeated; r10's dropdown
code is unchanged from r9. Not checked again: the Vulkan probe, since
`winex11.so` is unchanged.
