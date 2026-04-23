#!/bin/bash

# mmutil can create demo ROMs so that non-developers can test their songs with
# Maxmod without a full SDK installed in their system.
#
# This script builds the GBA demo ROM, takes the final ROM and updates the ROM
# in the data folder of mmutil.

set -e

BLOCKSDS="${BLOCKSDS:-/opt/blocksds/core}"
WONDERFUL_TOOLCHAIN="${WONDERFUL_TOOLCHAIN:-/opt/wonderful}"

# Build ROM
make -j`nproc`

mv maxmod_demo.gba ../data
