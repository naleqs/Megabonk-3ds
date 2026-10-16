#!/bin/sh
# Builds Megabonk 3DS from your own copy of Megabonk. See README.md.
cd "$(dirname "$0")" && exec python3 build_megabonk3ds.py "$@"
