#!/bin/sh
# build_tcc.sh OUT: TinyCC for the builder pages, which build the game's code (from the
# player's ROM) in the browser. TinyCC (LGPL 2.1, https://github.com/TinyCC/tinycc) is
# built as WebAssembly (needs emcc) for each kind of library the pages make:
#   win64    bonanza_game.dll   Windows x64
#   linux64  libbonanza_game.so Linux x86-64, Android x86_64
#   arm64    libbonanza_game.so Android arm64-v8a
#   arm      libbonanza_game.so Android armeabi-v7a (position independent; soft-float calls)
# and its runtime library for each (made by native cross compilers built first).
# OUT gets tcc-<kind>.js (the wasm inside), <kind>-libtcc1.a and include/.
# tcc-gnu-stack.patch: the libraries say they need no executable stack (PT_GNU_STACK);
# without it glibc 2.41 and later (SteamOS, Arch, Fedora 42, ...) refuse to load them.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(cd "$1" && pwd)
TCC_COMMIT=${TCC_COMMIT:-43c7708}
W=${TCC_WORK:-$OUT/.tcc-src}
if [ ! -d "$W/.git" ]; then
  git clone -q https://github.com/TinyCC/tinycc "$W"
fi
cd "$W"
git fetch -q --depth 1000 origin mob 2>/dev/null || true
git checkout -q -f "$TCC_COMMIT"
git apply "$HERE/tcc-gnu-stack.patch"
./configure > /dev/null
make -s cross-x86_64 cross-x86_64-win32 cross-arm64 > /dev/null
cp x86_64-libtcc1.a "$OUT/linux64-libtcc1.a"
cp x86_64-win32-libtcc1.a "$OUT/win64-libtcc1.a"
cp arm64-libtcc1.a "$OUT/arm64-libtcc1.a"
# ARM: position independent (Android refuses text relocations), soft-float calls
ARMDEF="-DTCC_TARGET_ARM -DTCC_ARM_VFP -DTCC_ARM_EABI -DCONFIG_TCC_PIC=1"
gcc -O2 -o arm-tcc-pic tcc.c -DONE_SOURCE=1 $ARMDEF -DCONFIG_TCC_CROSSPREFIX='"arm-"' -I. -lm -ldl -lpthread
mkdir -p armlib
for f in libtcc1.c armeabi.c alloca.S armflush.c stdatomic.c atomic.S builtin.c; do
  [ -f lib/$f ] && ./arm-tcc-pic -B. -I. -Iinclude -c lib/$f -o armlib/$(echo $f | sed 's/\.[cS]$//').o
done
./arm-tcc-pic -ar rcs "$OUT/arm-libtcc1.a" armlib/*.o
rm -rf "$OUT/include" && cp -r include "$OUT/include"
EMFLAGS="-O2 -DONE_SOURCE=1 -DCONFIG_TCC_SEMLOCK=0 -I. -sMODULARIZE=1 -sEXPORTED_RUNTIME_METHODS=[\"FS\",\"callMain\"] \
 -sINVOKE_RUN=0 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 -sENVIRONMENT=web,worker,node -sFORCE_FILESYSTEM=1 -sSINGLE_FILE=1"
emcc tcc.c $EMFLAGS -sEXPORT_NAME=TCC_win64 -DTCC_TARGET_X86_64 -DTCC_TARGET_PE -o "$OUT/tcc-win64.js"
emcc tcc.c $EMFLAGS -sEXPORT_NAME=TCC_linux64 -DTCC_TARGET_X86_64 -o "$OUT/tcc-linux64.js"
emcc tcc.c $EMFLAGS -sEXPORT_NAME=TCC_arm64 -DTCC_TARGET_ARM64 -o "$OUT/tcc-arm64.js"
emcc tcc.c $EMFLAGS -sEXPORT_NAME=TCC_arm $ARMDEF -o "$OUT/tcc-arm.js"
cp COPYING "$OUT/TINYCC-COPYING"
echo "TinyCC $TCC_COMMIT for the builder pages: $OUT"
