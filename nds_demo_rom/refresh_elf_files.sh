#!/bin/bash

# mmutil can create demo ROMs so that non-developers can test their songs with
# Maxmod without a full SDK installed in their system.
#
# This script builds the NDS demo ROM, takes the ARM7 and ARM9 ELF files, strips
# them to reduce their size, and updates the ELF files embedded in mmutil.

set -e

BLOCKSDS="${BLOCKSDS:-/opt/blocksds/core}"
WONDERFUL_TOOLCHAIN="${WONDERFUL_TOOLCHAIN:-/opt/wonderful}"
STRIP="${WONDERFUL_TOOLCHAIN}/toolchain/gcc-arm-none-eabi/arm-none-eabi/bin/strip"

# Build ROM
make -j`nproc`

# Fetch the ELF files
cp build/maxmod_nds_demo.elf nds_arm9.elf
cp ${BLOCKSDS}/sys/arm7/main_core/arm7_maxmod.elf nds_arm7.elf

# Strip anything we don't need so that the mmutil binary is smaller
${STRIP} nds_arm9.elf
${STRIP} nds_arm7.elf

mv nds_arm9.elf ../data
mv nds_arm7.elf ../data
