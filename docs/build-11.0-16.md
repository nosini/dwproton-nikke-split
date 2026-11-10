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
