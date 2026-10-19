**Title:** winewayland/win32u: Fix NIKKE's in-game store on Wayland

Fixes #ISSUE (white store, game exit when the store closes, dropdowns
closing at once). Four commits, in this order:

1. **winewayland.drv: Retire nested popup roles before their parent.**
   Tracks live popup descendants and clears their roles first, so hiding an
   owner before its owned popups no longer violates xdg-shell's destruction
   order. Compositor dismissal goes through the same path. A destroyed
   window's surface is torn down with the window data lock held, since the
   popup lists link surfaces of windows on other threads. Stands alone.
   GE-Proton fixed the same order independently in
   [b492349d][ge-fix] (from GE-Proton11-10) by walking all windows for
   popups owned by the surface; that patch targets GE-Proton's driver and
   does not apply here.
2. **win32u: Forward bitmap copies into windows of another process.** For a
   DC whose top-level belongs to another process and that has no window
   surface, clipped 32-bit `SRCCOPY` images are queued and, on `ReleaseDC`,
   mapped read-only into the top-level's process and announced with an
   internal message; that process paints them through a DC of the target
   window. Any process can post that message, so the receiver only accepts
   a whole read-only view that starts with the sender's tag, checks copies
   of the header and region rather than the shared memory, and leaves any
   other mapping alone. Other raster operations and formats are still
   discarded.
3. **user32: Let INTLWebViewHelper.exe use the Wayland driver.** Depends on
   2; without it, the store would stay white on Wayland.
   `nikke_launcher.exe` and `tbs_browser.exe` keep the X11 fallback.
4. **winewayland.drv: Keep non-activating tool popups unmanaged.**
   `WS_POPUP` windows with `WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW` (and not
   `WS_EX_APPWINDOW`) that are not being activated are no longer managed
   because of a thick frame, so they become ungrabbed `xdg_popup`s. For an
   unowned popup whose owner hint (the window beside it) belongs to another
   process, the foreground window is used as the parent instead.

**Testing.** Each commit builds on 11.0-15. Small Win32 reproducers were
run through `proton run` with the published 11.0-15 runner and with a copy
of it using the patched modules, on headless Mutter and Weston:

| Check | 11.0-15 | Patched |
| --- | --- | --- |
| Nested popup teardown, Mutter | Protocol error | Survives |
| Popup teardown from two threads, Mutter | Protocol error | Survives |
| Cross-process software bitmap, Weston | Wrong pixels | Correct pixels |
| Disjoint bitmap clipping, Weston | — | Passes |
| Helper-owned child under a foreign top-level, Weston | — | Passes |
| OpenGL output from the helper, Weston | — | Passes |
| Chromium-style dropdown, Mutter | Loses focus | Stays open; click, wheel and key delivered |
| Dropdown hide and reopen, Mutter | — | Passes |

Weston accepts the unpatched popup teardown, so those cases use Mutter.
The two-thread case lets a thread exit with a nested popup pair still
mapped while the owner's thread hides the owner; without the lock in the
first patch it also fails with the protocol error, intermittently. The
dropdown check shows that a wheel event is delivered, not that a store
dropdown scrolls. A further probe posts the internal message with the
address of an unrelated mapping; the receiver leaves that mapping alone.

In NIKKE with the series applied to 11.0-15, as with 11.0-14 before, the
store renders, closing it keeps the game running, and its dropdowns stay
open and work with the mouse, scrollbar and keyboard.

**Known limits.** The mouse wheel does not scroll a store dropdown yet.
Forwarded images are copied once per `ReleaseDC`; a view posted to a
window that is destroyed before handling it is not unmapped. Hardware
cross-process output is not covered.

The series applies to Wine `b75a4125963073b01d2b3bfce6060d697d02066b`,
the source of the `dwproton-11.0-15` release.

This pull request and its patches were prepared with the help of generative AI.

[ge-fix]: https://github.com/GloriousEggroll/proton-ge-custom/commit/b492349d0233dcff26c5a585ddd3f2a4c8c3095c
