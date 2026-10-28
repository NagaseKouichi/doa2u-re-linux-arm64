# Linux ARM64 移植说明

[English](linux-arm64.md)

本分支补齐 x86-64 主机交叉构建 Linux AArch64 的工具链、依赖环境和构建/启动参数。
游戏光盘尚未提供，因此不宣称游戏已经在 ARM64 掌机运行。

## 无镜像阶段

可构建公开运行时、SDL3 图形/音频后端、内核和设置界面，使用 QEMU 执行 ARM64
测试。完整命令见英文说明。Docker 环境中的软件包不会写入宿主或掌机系统。

```sh
docker build --network=host -t doa2u-arm64-cross:local tools/arm64
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD:/work" \
  doa2u-arm64-cross:local sh -c '
    RECOMP_QEMU_AARCH64=/usr/bin/qemu-aarch64 cmake \
      -S recomp-runtime -B private/arm64-build/runtime -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/work/cmake/linux-aarch64.cmake \
      -DRECOMP_FETCH_SDL3=ON &&
    cmake --build private/arm64-build/runtime --parallel 4 &&
    ctest --test-dir private/arm64-build/runtime --output-on-failure -E d3d-presenter'
```

这里不运行图形 presenter 测试；它可使用 ARM64 Mesa 软件 Vulkan 单独执行，但
QEMU 下可能超过上游 60 秒时限。无论结果如何，都不等于实体 GPU/声音/手柄测试。
不要把仅含 dummy 后端的 SDL 测试构建当作可玩版本交付。

## 镜像提供之后

仍只接受上游认证的美版 Disc 2。先用上游提取及 `--lift-only` 工作流生成和验证
C 源码，再在容器中运行：

```sh
python3 tools/build_game.py --build-only \
  --cmake-toolchain cmake/linux-aarch64.cmake \
  --build-dir build/recomp-program-arm64
```

以上命令须在构建容器内执行；完整 Docker 调用见英文文档。容器已包含固定版本的
Capstone/Pillow。真实游戏构建仍待镜像验证，不能用合成文件冒充游戏结果。

在目标 ARM64 机器准备好对应依赖后：

```sh
./launcher.sh --build-dir build/recomp-program-arm64
```

原有原生构建与启动参数不变。未指定目录的交叉构建使用
`build/recomp-program-cross`，启动时也应传入该路径。
上游 receipt 只记录最近一次构建的 runner 哈希，切换架构需恢复对应 receipt 或重建，
不能跳过校验。

## 后续验收

- ELF 架构及动态库均确认是 AArch64，不混入 x86-64 库。
- 在真机验证 Vulkan、BC1/2/3 纹理和深度模板支持。
- 菜单、演示、实际对战、视频、存档、软重置。
- gamescope 下实体屏幕、音频、手柄和热插拔。
- 先以 480p、MSAA/SMAA 关闭建立性能基线，再提高画质。

Fedora/Armada 采用用户目录依赖，不修改 `/usr`、`/lib`，保留现有
`armada-game-launch` 启动包装器。不添加固定 CPU 核编号绑核。
ARM64 的倒数/倒平方根与 Intel 近似指令存在结果差异，需做游戏级回归。
