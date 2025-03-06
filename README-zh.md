# SimpleAudioPlayer Native Library

[English Version](README.md)

## 简介
本仓库是为 [SimpleAudioPlayer](https://github.com/j4587698/SimpleAudioPlayer) 项目提供支持的Native库。基于miniaudio框架和FFmpeg多媒体解码库构建，提供跨平台音频解码与播放能力。

## 主要特性
- 基于FFmpeg的音频解码，支持常见格式
- 通过miniaudio实现跨平台音频后端
- 支持流式播放和音频重采样
- 简洁的C/C++ API接口

## 依赖项
- [FFmpeg](https://ffmpeg.org/) (版本 >= 6.1)
- [miniaudio](https://miniaud.io/) (版本 >= 0.11)
- C++17兼容编译器

## 构建说明
参见 GitHub Actions CI.yml

## 协议说明
本项目采用LGPL-3.0协议授权，关键要求：

1. 动态链接允许闭源使用
2. 修改代码必须开源
3. 必须保留原始版权声明

## 引用声明
本项目基于以下优秀库实现：

- FFmpeg (LGPL-2.1+/GPLv2+) https://ffmpeg.org/
- miniaudio (Public Domain/DMIT) https://miniaud.io/

## LICENSE
![license](https://img.shields.io/github/license/j4587698/SimpleAudioPlayer.Native)
