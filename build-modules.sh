#!/bin/bash
# Build the four replacement modules from the Wine source of the published
# DWProton 11.0-15 runner with the patch series in upstream/ applied, then
# replace modules/ and its checksum manifest with them.
set -euo pipefail

base=b75a4125963073b01d2b3bfce6060d697d02066b
source_url=https://dawn.wine/dawn-winery/wine-dwproton.git
modules=(
    dlls/winewayland.drv/winewayland.so
    dlls/win32u/win32u.so
    dlls/user32/x86_64-windows/user32.dll
    dlls/user32/i386-windows/user32.dll
)

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 NEW_WORK_DIRECTORY [JOBS]" >&2
    exit 2
fi
repo=$(cd -- "$(dirname -- "$0")" && pwd)
work=$1
jobs=${2:-$(nproc)}
if [ -e "$work" ]; then
    echo "$work already exists; use a new directory" >&2
    exit 1
fi
mkdir -p -- "$work"
work=$(cd -- "$work" && pwd)

git init -q "$work/wine-source"
git -C "$work/wine-source" fetch -q --depth 1 "$source_url" "$base"
git -C "$work/wine-source" checkout -q FETCH_HEAD
if [ "$(git -C "$work/wine-source" rev-parse HEAD)" != "$base" ]; then
    echo "fetched source is not $base" >&2
    exit 1
fi
for patch in "$repo"/upstream/000*.patch; do
    git -C "$work/wine-source" apply "$patch"
done

(cd "$work/wine-source" && ./autogen.sh)
mkdir "$work/wine-build"
cd "$work/wine-build"
../wine-source/configure --enable-archs=i386,x86_64 --without-x --with-wayland --disable-tests
make -j"$jobs" "${modules[@]}"

for module in "${modules[@]}"; do
    case $module in
    */x86_64-windows/*) target=x86_64-windows ;;
    */i386-windows/*) target=i386-windows ;;
    *) target=x86_64-unix ;;
    esac
    install -D -m 0755 "$module" "$repo/modules/files/lib/wine/$target/$(basename "$module")"
done
cd "$repo/modules"
python3 -I - <<'EOF'
import hashlib, json
from pathlib import Path
names = sorted(str(p) for p in Path("files").rglob("*") if p.is_file())
manifest = {name: hashlib.sha256(Path(name).read_bytes()).hexdigest() for name in names}
Path("sha256.json").write_text(json.dumps(manifest, indent=2) + "\n")
EOF
echo "Replaced the modules in $repo/modules"
