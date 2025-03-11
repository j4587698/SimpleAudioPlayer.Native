# SimpleAudioPlayer Native Library

[中文版本](README-zh.md)

## Introduction
This repository contains the native library for the [SimpleAudioPlayer](https://github.com/j4587698/SimpleAudioPlayer) project. Built on miniaudio framework and FFmpeg multimedia decoding library, it provides cross-platform audio decoding and playback capabilities.

## Key Features
- FFmpeg-based audio decoding with common format support
- Cross-platform audio backend via miniaudio
- Stream playback and audio resampling support
- Clean C/C++ API interface

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