# Maxmod Utility

## Introduction

This is a packer of songs and audio effects to be used by Maxmod.

Please, report issues [here](https://codeberg.org/blocksds/sdk/issues).

## Usage

```
mmutil [options] input files ...
```

Input files may be MOD, S3M, XM, IT, and/or WAV.

Option       | Description
-------------|---------------------------------------------------
`-o<output>` | Set output file.
`-h<header>` | Set header output file.
`-m`         | Output MAS file rather than soundbank.
`-d`         | Use for NDS projects.
`-b`         | Create test ROM. (use -d for .nds, otherwise .gba)
`-i`         | Ignore sample name flags (NDS mode only).
`-v`         | Enable verbose output.
`-p`         | Set initial panning separation for MOD/S3M.
`-z`         | Export raw WAV data (8-bit format).
`-V`         | Print version string and exit.

Sample flags are strings added to a sample name to modify the sample during
conversion.

- `%o`: Used to support the `9xx` command when using the DS hardware channels
  for playback. In MOD/XM (maybe others) you can use a sample offset command to
  start playing beyond the loop start, and then when the sample hits the loop
  end it'll still correctly loop back to the start.
- `%c`: Compress the sample using IMA-ADPCM. This only works on NDS in hardware
  mixer mode.

## Examples

- Create DS soundbank file (soundbank.bin) from input1.xm and input2.it. Also,
  output header file (soundbank.h)

  ```
  mmutil -d input1.xm input2.it -osoundbank.bin -hsoundbank.h
  ```

- Create test GBA ROM from two inputs.

  ```
  mmutil -b input1.mod input2.s3m -oTEST.gba
  ```

- Create test NDS ROM from three inputs.

  ```
  mmutil -d -b input1.xm input2.s3m testsound.wav -oTEST.nds
  ```
