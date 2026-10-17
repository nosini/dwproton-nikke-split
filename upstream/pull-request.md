**Title:** winewayland/win32u: Fix NIKKE's in-game store on Wayland

Fixes #ISSUE (white store, game exit when the store closes, dropdowns
closing at once). Four commits, in this order:

1. **winewayland.drv: Retire nested popup roles before their parent.**
   Tracks live popup descendants and clears their roles first, so hiding an
   owner before its owned popups no longer violates xdg-shell's destruction
   order. Compositor dismissal goes through the same path. Stands alone.
2. **win32u: Forward bitmap copies into windows of another process.**
   For a DC whose top-level belongs to another process and that has no
   window surface, clipped 32-bit `SRCCOPY` images are queued and, on
   `ReleaseDC`, mapped read-only into the top-level's process, which paints
   them through a DC of the target window. The receiver only accepts a
   whole mapped view of the expected size. Other raster operations and
   formats are discarded as before.
3. **user32: Let INTLWebViewHelper.exe use the Wayland driver.** Depends on
   2; without it, the store would stay white on Wayland.
   `nikke_launcher.exe` and `tbs_browser.exe` keep the X11 fallback.
4. **winewayland.drv: Keep non-activating tool popups unmanaged.**
   `WS_POPUP` windows with `WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW` (and not
   `WS_EX_APPWINDOW`) that are not being activated are no longer managed
   because of a thick frame, so they become ungrabbed `xdg_popup`s. For an
   unowned popup whose owner hint (the window beside it) belongs to another
   process, the foreground window is used as the parent instead.

**Testing.** Each commit builds. Small Win32 reproducers, run with the
DWProton 11.0-14 runner and these modules swapped in, under Weston and
headless Mutter with virtual input:

- nested popups: owner hidden before its popup, protocol error before,
  survives after (also through `proton run`);
- cross-process `BitBlt` into a child of another process's top-level:
  pixel check fails before, passes after, with disjoint clip regions, a
  helper-owned child and repeated frames; OpenGL output from a process
  named `INTLWebViewHelper.exe` still reaches the parent;
- Chromium-style dropdown created in the game process: before, it is
  activated and the browser child loses focus; after, the child keeps
  focus, and a click, a wheel turn and a key press each reach their window,
  also through `proton run` with a console window beside the popup; hiding
  and reshowing works.

In NIKKE the store renders, closing it keeps the game running, and its
dropdowns stay open and work with the mouse, scrollbar and keyboard.

**Known limits.** The mouse wheel does not scroll a store dropdown yet.
Forwarded images are copied once per `ReleaseDC`; a view posted to a
window that is destroyed before handling it is not unmapped. Hardware
cross-process output is not covered.

The series is based on `dwproton/11.0-14`; REBASE-NOTE.
