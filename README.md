# SDIOAnalyzer
A simple command line tool to decode SDIO transfers from logic analyzer captures.

Input file: A simple file consisting out of a series of bytes, where each byte is a sample of SDIO bus lines. This file can e.g. be generated using the Saleae Logic analyzer function.

A Samplerate and channel numbers for the SDIO signals need to be specified as input.
The samplerate is required to generate correct timestamp information.

# Output
The decoded output is printed to STDOUT. It can be redirected to a file and copied e.g. into Excel for easier viewing.

# Limitations
- This SDIO analyzer does not support dual data rate (DDR) transfers.
- Only very limiting testing has been applied. I cannot state, how generic it is applicable, or if the decoding is 100% correct yet.
- Command line parameters are not checked extensively, or, if the provided filename is from an existing file.

# Compile
For development the main.c file was compiled using Code::Blocks and MingW toolchain.
