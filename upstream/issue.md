**Title:** NIKKE store on winewayland: white view, game exits when it closes, dropdowns close at once

With DWProton 11.0-14, `WINE_GRAPHICS_DRIVER=wayland` and `DISPLAY` kept
for the launcher fallback, NIKKE's in-game store (opened from the game,
rendered by the INTL WebView/CEF) has three problems on a GNOME Wayland
session. The same white store and exit on close also happen in Bottles with
`WINE_GRAPHICS_DRIVER=wayland`. Running the whole game on X11 avoids all
three.

1. **The store view stays white.** `INTLWebViewHelper.exe` is in user32's
   list of programs forced onto winex11, so its output never reaches the
   game's Wayland window. Removing it from the list is not enough on its
   own: the helper's GPU process runs with
   `--use-angle=swiftshader-webgl` and its renderers with
   `--disable-gpu-compositing`, so Chromium's software output device copies
   each frame into the browser's HWND with `GetDC`/`BitBlt`/`ReleaseDC`.
   That HWND's top-level belongs to the game process, the helper has no
   window surface for it, and the null driver's `PutImage` drops the bits.

2. **Closing the store ends the game.** Hiding the store hides an owned
   popup while a popup it owns is still visible. winewayland destroys the
   parent `xdg_popup` first, the compositor reports a protocol error and
   Wine exits:

   ```
   xdg_wm_base: error 2: destroyed popup not top most popup
   waylanddrv_unix_read_events Protocol error
   wayland_read_events_thread Failed to read events from the compositor, terminating process
   ```

   This also reproduces with GE-Proton 11-9.

3. **Dropdowns in the store, such as the billing city, close as soon as
   they open.** CEF's browser runs in the game process. Its `<select>`
   popup is an unowned `Chrome_WidgetWin_0` with
   `WS_POPUP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_THICKFRAME` and
   `WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW`, shown without activation.
   `is_window_managed` treats any `WS_THICKFRAME` window as managed, so it
   becomes an `xdg_toplevel`; GNOME gives it keyboard focus, the game window
   loses focus and Chromium closes the dropdown. A `+waylanddrv,+keyboard`
   trace shows a keyboard leave on the game window and an enter on the new
   popup each time.

I have a four-patch series for these, developed and tested in NIKKE with
11.0-14 and 11.0-15, and now rebased onto the `dwproton-11.0-16` release
(Wine `dda80c1bc56000053a773cf6f8585331d1bd68f7`). NIKKE was also
confirmed working with the 11.0-16 build on 2026-11-01.

11.0-16 adds a popup traversal before selected surface-destruction paths.
The remaining popup patch reuses it for recursive role teardown instead
of maintaining separate child lists. The release still forces the helper
onto X11 and discards the software bitmap copies. With the released runner,
the standalone popup probe still hits the protocol error, and the software
bitmap probe still exits with incorrect pixels.

This issue and the patches were prepared with the help of generative AI.
