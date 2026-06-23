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

## Version 2.1
Version 2.1 adds native recording APIs. It supports PCM streams, WAV, AAC ADTS, and M4A output, including callback-based stream output for managed callers.

Version 2.0 updated the native callback contract used by SimpleAudioPlayer. It added explicit stream length and seek capability callbacks, preserves decode failure results, and reports I/O failures separately from normal end-of-stream completion.

Use SimpleAudioPlayer.Native 2.1.0 with a SimpleAudioPlayer version that targets the 2.1 native API. Older managed packages may not call the updated native entry points correctly.

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
