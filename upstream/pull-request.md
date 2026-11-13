**Title:** winewayland/win32u: Fix NIKKE's in-game store on Wayland

Fixes #ISSUE (white store, game exit when the store closes, dropdowns
closing at once). Four commits, in this order:

1. **winewayland.drv: Retire nested popup roles before their parent.**
   Reuses 11.0-16's popup traversal from the common role-clearing path,
   recursively retiring descendants when hiding windows or handling
   compositor dismissal. Removes our previous popup child lists and the
   upstream calls at individual surface-destruction sites. A destroyed
   window's popups are retired before it leaves the window tree, and its
   surface is destroyed under the window data lock, because traversal
   visits windows on other threads. Stands alone.
2. **win32u: Forward bitmap copies into windows of another process.** For a
   DC whose top-level belongs to another process and that has no window
   surface, clipped 32-bit `SRCCOPY` images are queued and, on `ReleaseDC`,
   mapped read-only into the top-level's process and announced with an
   internal message; that process paints them through a DC of the target
   window. Any process can post that message, so the receiver only accepts
   a whole read-only view that starts with the sender's tag, checks copies
   of the header and region rather than the shared memory, and leaves any
   other mapping alone. Other raster operations, fully clipped copies and
   copies to the desktop window are still discarded and reported as
   successful. A DC that is never released queues at most 16 copies.
3. **user32: Let INTLWebViewHelper.exe use the Wayland driver.** Depends on
   2; without it, the store would stay white on Wayland.
   `nikke_launcher.exe` and `tbs_browser.exe` keep the X11 fallback.
4. **winewayland.drv: Keep non-activating tool popups unmanaged.**
   `WS_POPUP` windows with `WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW` (and not
   `WS_EX_APPWINDOW` or `WS_CAPTION`) that are not being activated are no longer managed
   because of a thick frame, so they become ungrabbed `xdg_popup`s. For an
   unowned popup whose owner hint (the window beside it) belongs to another
   process, the foreground window is used as the parent instead.

**Testing.** The complete series builds on 11.0-16. Small Win32 reproducers
were run through `proton run` with the published 11.0-16 runner and with a copy
of it using the patched modules, on headless Mutter and Weston:

| Check | 11.0-16 | Patched |
| --- | --- | --- |
| Nested popup teardown, Mutter | Protocol error | Survives |
| Popup teardown from two threads, Mutter | Survives 100 iterations | Survives |
| Cross-process software bitmap, Weston | Wrong pixels | Correct pixels |
| Disjoint bitmap clipping, Weston | — | Passes |
| Helper-owned child under a foreign top-level, Weston | — | Passes |
| OpenGL output from the helper, Weston | — | Passes |
| Chromium-style dropdown, Mutter | Loses focus | Stays open; click, wheel and key delivered |
| Dropdown hide and reopen, Mutter | — | Passes |
| Captioned non-activating tool palette, Mutter | Managed | Stays managed |

Weston accepts the unpatched popup teardown, so those cases use Mutter.
The two-thread case lets a thread exit with a nested popup pair still
mapped while the owner's thread hides the owner. The dropdown check shows that a wheel event is delivered, not that a store
dropdown scrolls. A further probe posts the internal message with the
address of an unrelated mapping; the receiver leaves that mapping alone.

In NIKKE with the series applied to 11.0-15, as with 11.0-14 before, the
store renders, closing it keeps the game running, and its dropdowns stay
open and work with the mouse, scrollbar and keyboard. NIKKE was also
confirmed working with the 11.0-16 runner on 2026-11-01.

**Known limits.** The mouse wheel does not scroll a store dropdown yet.
Forwarded images are copied once per `ReleaseDC`; a view posted to a
window that is destroyed before handling it is not unmapped. Hardware
cross-process output is not covered.

The series applies to Wine `dda80c1bc56000053a773cf6f8585331d1bd68f7`,
the source of the `dwproton-11.0-16` release.

This pull request and its patches were prepared with the help of generative AI.
