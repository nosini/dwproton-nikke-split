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
`dwproton/11.0-14` and now rebased onto `dwproton/11.0-15` (Wine
`f078dded59f559884b58fbf0769847e9c9fbb3ac`). With the series applied to
11.0-14, the store renders, closing it keeps the game running, and the
dropdowns stay open and work with the mouse, the scrollbar and the
keyboard. The mouse wheel does not scroll a dropdown yet.

The 11.0-15 source branch still forces the helper onto X11 and lacks the
popup and bitmap fixes. The 11.0-15 build has not been tested in NIKKE.

This issue and the patches were prepared with the help of generative AI.
