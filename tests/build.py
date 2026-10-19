#!/usr/bin/env python3
"""Build the Win64 probes with Clang/LLD and Wine's headers, without a CRT."""

import argparse
from pathlib import Path
import shutil
import subprocess


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("wine_source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = Path(__file__).resolve().parent
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
imports = {
    "kernel32": ["GetModuleHandleA", "Sleep", "ExitProcess", "GetStdHandle", "WriteFile",
                 "GetCommandLineA", "GetModuleFileNameA", "CreateProcessA", "WaitForSingleObject",
                 "GetExitCodeProcess", "CreateFileMappingA", "MapViewOfFile", "VirtualQuery",
                 "CreateThread", "CreateEventA", "SetEvent", "CloseHandle"],
    "user32": ["RegisterClassA", "CreateWindowExA", "ShowWindow", "DestroyWindow", "UpdateWindow",
               "PeekMessageA", "TranslateMessage", "DispatchMessageA", "DefWindowProcA", "GetDC", "ReleaseDC", "FillRect",
               "SetFocus", "GetFocus", "GetWindowRect", "GetWindow", "BeginPaint", "EndPaint",
               "LoadCursorA", "GetForegroundWindow", "PostMessageA"],
    "gdi32": ["ChoosePixelFormat", "SetPixelFormat", "SwapBuffers", "CreateSolidBrush", "DeleteObject", "GdiFlush", "CreateCompatibleDC", "CreateDIBSection", "SelectObject", "BitBlt", "DeleteDC", "CreateRectRgn", "CombineRgn", "SelectClipRgn", "GetPixel"],
    "opengl32": ["wglCreateContext", "wglMakeCurrent", "wglDeleteContext", "glClearColor", "glClear"],
}
libraries = []
for name, exports in imports.items():
    definition = output / f"{name}.def"
    definition.write_text(f"LIBRARY {name}\nEXPORTS\n" + "\n".join(exports) + "\n")
    library = output / f"{name}.lib"
    subprocess.run(["llvm-dlltool", "-m", "i386:x86-64", "-d", str(definition),
                    "-l", str(library)], check=True)
    libraries.append(str(library))
for name in ("window-probe", "popup-race-probe", "render-probe", "dropdown-probe", "forged-probe"):
    subprocess.run(["clang", "--target=x86_64-pc-windows-msvc", "-D__WINE_PE_BUILD",
                    "-I", str(args.wine_source / "include"),
                    "-I", str(args.wine_source / "include/msvcrt"),
                    "-fuse-ld=lld", "-nostdlib", "-fno-stack-protector", str(source / f"{name}.c"),
                    *libraries, "-Wl,/entry:mainCRTStartup", "-Wl,/subsystem:console",
                    "-o", str(output / f"{name}.exe")], check=True)
shutil.copyfile(output / "render-probe.exe", output / "INTLWebViewHelper.exe")
# The dropdown probe launches its helper under the same name from its own directory.
(output / "dropdown").mkdir(exist_ok=True)
shutil.move(output / "dropdown-probe.exe", output / "dropdown/dropdown-probe.exe")
shutil.copyfile(output / "dropdown/dropdown-probe.exe", output / "dropdown/INTLWebViewHelper.exe")
