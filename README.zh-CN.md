# Megabonk 3ds

[English](README.md)

大概 75% 是靠 vibe coding 写出来的。

这是 Megabonk 的非官方任天堂 3DS 移植版，基于 libctru 用 C 语言从零编写（3D 使用 citro3d，界面使用 citro2d）。**本仓库不包含任何游戏文件。** 仓库里只有移植版的代码，以及一个构建工具：它会把**你自己拥有的 Megabonk**（Steam 版）中的资源转换成 3DS 自制软件。

## 构建

需要 Python 3.9 或更高版本、网络连接，以及通过 Steam 安装的 Megabonk。

- **Windows：** 双击 `build.bat`
- **Linux / macOS：** 运行 `./build.sh`
- 或者直接运行：`python build_megabonk3ds.py [--game-dir "Megabonk 的路径"]`

构建工具会：
1. 通过 Steam 找到 Megabonk（也可以用 `--game-dir` 指定包含 `Megabonk_Data` 的文件夹）
2. 创建一个临时环境，其中包含 Python 虚拟环境和 devkitARM / libctru / citro3d 工具链（从 devkitPro 下载，约 100 MB）
3. 转换模型、动画、贴图、音频和游戏数据
4. 编译出 `output/3ds/megabonk3ds/megabonk3ds.3dsx`
5. 删除临时环境（加上 `--keep-env` 可以保留，用于调试）

整个过程需要一些时间（烘焙 21 个角色的动画最慢）。不会在系统中安装任何东西。

## 安装到 3DS

把 `output/3ds` 文件夹复制到 SD 卡根目录（这样程序位于 `sdmc:/3ds/megabonk3ds/megabonk3ds.3dsx`），然后从 Homebrew Launcher 启动。
声音需要 `sdmc:/3ds/dspfirm.cdc`（如果没有，请先运行一次 DSP1）。

`.3dsx` 文件里包含从你的游戏副本转换来的资源：请留给自己使用，不要上传或分享。

## 操作

| 按键 | 功能 |
|---|---|
| 摇杆（Circle Pad） | 移动 |
| A / B | 跳跃（Float 类角色按住可滑翔） |
| X | 互动（宝箱、神龛、Shady Guy 等） |
| L / R、C 摇杆、十字键左/右 | 旋转镜头 |
| 十字键上/下 | 调整镜头俯仰 |
| START | 暂停 |
| 触摸屏 / 十字键 + A | 菜单操作 |
| Y | 重新抽取升级选项 |

## 包含内容

- 森林、沙漠和墓地三个关卡，场景物件、光照和敌人出场时间线都来自游戏数据
- 全部 21 个角色，包含各自的动画、初始武器和被动能力
- 全部 30 把武器。基础伤害、击退、暴击率、弹丸数量、弹射次数、冷却时间以及升级时的属性条目，都直接读取自游戏本身的武器数据
- 22 种书（Tome），包括护盾、荆棘、血腥、击退、黄金、持续时间、诅咒和混沌
- 宝箱、陶罐、六种神龛、Shady Guy 的商店、微波炉、Boss 传送门，以及 82 件道具（稀有度与原版一致）
- 原版游戏的音乐、音效、字体、图标和头像

道具、书和被动能力中那些没有保存在游戏数据文件里的数值（它们写在编译后的代码中），在 [megabonk.org 数据库](https://megabonk.org/database/) 有记载的情况下以其为准，否则为估算值。移植版目前还没有：巫师帽（Wizard's Hat）、信标（Beacon）和南瓜（Pumpkin）（它们需要书的等级上限或后续关卡）、Monke 的爬墙、镰刀的蓄力攻击、敌人的特殊攻击、存档和解锁。

## 目录结构

```
build_megabonk3ds.py   构建工具（从这里开始）
source/                游戏代码（C，libctru/citro3d）
shaders/               PICA200 顶点着色器
tools/                 资源转换、工具链安装和编译步骤（由构建工具调用）
```

Megabonk 由 vedinad 制作。本项目与其开发者没有任何关联，也未获得其认可。
