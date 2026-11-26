#!/usr/bin/env bash
# The pinned third-party sources for a Linux build (the GitHub release workflow; docs/RELEASE_PIPELINE.md): SDL at the tag
# and commit scripts/fetch-deps.ps1 pins, cloned from GitHub and checked. Android needs nothing else (the NDK has the GLES
# headers). Safe to run again: an existing tree is verified, not replaced.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
tp="$root/third_party"
sdl_tag='release-3.4.20'
sdl_commit='f00dc919ca35b9756316e86c4777392de442f9b2'
mkdir -p "$tp"
if [ ! -f "$tp/SDL/CMakeLists.txt" ]; then
    echo "[deps] cloning SDL $sdl_tag"
    git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$sdl_tag" https://github.com/libsdl-org/SDL.git "$tp/SDL"
fi
head=$(git -C "$tp/SDL" rev-parse HEAD)
if [ "$head" != "$sdl_commit" ]; then
    echo "[deps] SDL is at $head, expected $sdl_commit ($sdl_tag)" >&2
    exit 1
fi
echo "[deps] SDL verified at $sdl_commit"
