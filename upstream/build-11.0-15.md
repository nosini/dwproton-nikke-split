# DWProton 11.0-15 source build

The four-patch series in this directory targets Wine commit
`f078dded59f559884b58fbf0769847e9c9fbb3ac` on `dwproton/11.0-15`.
The corresponding DWProton branch commit is
`b167281d83904366541b691e1e39403b2b1a4986`.

This is the existing fix rebased onto 11.0-15. Its changes still apply to
every game using the patched modules, not only NIKKE. The user32 patch
removes the same helper entry from the updated fallback list; the other
three changes are unchanged. The exported patches reproduce the rebased
source tree exactly.

Each commit builds the 64-bit Unix winewayland and win32u modules and both
32-bit and 64-bit PE user32 DLLs. Configure options:

```sh
../wine/configure --enable-archs=i386,x86_64 \
  --without-x --with-wayland --disable-tests
```

The full standalone Wine build and installation completed. Ten runtime
checks passed against that installation, including the expected failures
with the unpatched 11.0-15 modules:

| Check | Base | Patched |
| --- | --- | --- |
| Cross-process software bitmap, Weston | Wrong pixels, exit 7 | Correct pixels, exit 0 |
| Disjoint bitmap clipping, Weston | — | Exit 0 |
| Helper-owned child under a foreign top-level, Weston | — | Exit 0 |
| OpenGL rendering, Weston | — | Exit 0 |
| Nested popup teardown, Mutter | Exact protocol error, exit 1 | Survives, exit 0 |
| Chromium-style dropdown, Mutter | Loses focus, exit 8 | Stays open; click, wheel and key delivered, exit 0 |
| Dropdown hide and reopen, Mutter | — | Exit 0 |

Weston accepted the base popup teardown without a protocol error, so the
negative control for that case uses Mutter. The dropdown probe checks
delivery of a wheel event; it does not establish that NIKKE's dropdown
actually scrolls.

The standalone installation needed aliases for `i386-unix/wine64` and
`i386-unix/wine64-preloader` pointing to the corresponding x86-64 loaders:
the fork's child-process loader expects Proton's directory layout.
Both loader targets are from this build. The test prefix links its user32
DLLs to the installed modules so switching between base and patched
modules cannot leave stale prefix copies.

The four compiled modules, SHA-256 manifest and build metadata are in
`builds/dwproton-11.0-15/` (ignored build output), with the module archive
`dwproton-11.0-15-nikke-split-modules.tar.xz`. The Unix modules require
glibc 2.38 or newer. This standalone test configuration has no X11,
audio or Schannel support; it is intended for the focused Wayland tests.

As of 2026-10-18, the latest published DWProton runner remains 11.0-14.
This source build is not a complete DWProton distribution. The existing
`prepare-runner.py` and `modules/` remain for 11.0-14.
The 11.0-15 build has not been tested in NIKKE.
