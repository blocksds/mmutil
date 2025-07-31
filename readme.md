# Maxmod Utility

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
`-i`         | Ignore sample flags.
`-v`         | Enable verbose output.
`-p`         | Set initial panning separation for MOD/S3M.
`-z`         | Export raw WAV data (8-bit format).

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
