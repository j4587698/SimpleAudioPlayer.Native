# SimpleAudioPlayer Native Library

[中文版本](README-zh.md)

## Introduction
This repository contains the native library for the [SimpleAudioPlayer](https://github.com/j4587698/SimpleAudioPlayer) project. Built on miniaudio framework and FFmpeg multimedia decoding library, it provides cross-platform audio decoding and playback capabilities.

## Key Features
- FFmpeg-based audio decoding with common format support
- Cross-platform audio backend via miniaudio
- Stream playback and audio resampling support
- Native recording support for PCM, WAV, AAC, and M4A outputs
- Decoder callback support for stream length and seek capability
- Propagates stream and decoder failures separately from normal EOF
- Clean C/C++ API interface

## Version 2.3
Version 2.3 enables FFmpeg assembly optimizations: x86_64 uses nasm-driven SIMD (SSE/AVX) when available, ARM/ARM64 uses compiler-built-in NEON, and 32-bit x86 safely disables x86asm to avoid PIC and Android TEXTREL issues. It also fixes tail-of-file residual data being reported as a decode error instead of natural EOF.

Version 2.2 adds configurable output-device APIs and maps playback usage and content type to supported platform audio backends.

Use SimpleAudioPlayer.Native 2.3.0 with SimpleAudioPlayer 2.3.0. Older native packages do not contain the extended output-device entry point.

## Output Device Configuration

`audio_init_device_ex` accepts a versioned, size-tagged `AudioDeviceConfig` for sample rate, channels, buffer periods, performance preference, sharing mode, and cross-platform playback intent. Android maps the intent to AAudio `usage`/`contentType` and the OpenSL ES stream type. The existing `audio_init_device` entry point remains available with its original defaults.

## Dependencies
- [FFmpeg](https://ffmpeg.org/) (version >= 6.1)
- [miniaudio](https://miniaud.io/) (version >= 0.11)
- C++17 compatible compiler

## Building
Refer to GitHub Actions CI.yml

## License Information
This project is licensed under LGPL-3.0. Key requirements:

1. Dynamic linking allows proprietary use
2. Modifications must be open-sourced
3. Original copyright notices must be preserved

## Acknowledgments
This project stands on the shoulders of:

- FFmpeg (LGPL-2.1+/GPLv2+) https://ffmpeg.org/
- miniaudio (Public Domain/DMIT) https://miniaud.io/
- miniaudio-ffmpeg-decoder (Public Domain/DMIT) https://github.com/Mr-Ojii/miniaudio-ffmpeg-decoder

## License 
![license](https://img.shields.io/github/license/j4587698/SimpleAudioPlayer.Native)
