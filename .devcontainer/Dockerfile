FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG ARM_GNU_VERSION=15.2.rel1
ARG CMSIS_TOOLBOX_VERSION=2.14.1

ENV DEBIAN_FRONTEND=noninteractive

# --- Base packages -----------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential g++-14 ninja-build ccache \
    openocd gdb-multiarch stlink-tools usbutils \
    picocom python3 python3-serial udev \
    doxygen graphviz \
    libhdf5-dev libudev-dev pkg-config \
    git git-lfs curl wget gnupg ca-certificates xz-utils lsb-release \
    software-properties-common libncursesw6 \
    && rm -rf /var/lib/apt/lists/*

# --- CMake -------------
RUN curl -fsSL https://apt.kitware.com/keys/kitware-archive-latest.asc \
      | gpg --dearmor -o /usr/share/keyrings/kitware.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/kitware.gpg] https://apt.kitware.com/ubuntu/ noble main" \
      > /etc/apt/sources.list.d/kitware.list \
    && apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh \
    && bash /tmp/llvm.sh \
    && LLVM_VER="$(ls -1 /usr/bin/clang-[0-9]* | grep -oE '[0-9]+$' | sort -n | tail -n1)" \
    && apt-get update && apt-get install -y --no-install-recommends \
         "clang-format-${LLVM_VER}" "clang-tidy-${LLVM_VER}" "clangd-${LLVM_VER}" \
    && for t in clang-format clang-tidy clangd; do \
         ln -sf "/usr/bin/${t}-${LLVM_VER}" "/usr/local/bin/${t}"; \
       done \
    && rm /tmp/llvm.sh && rm -rf /var/lib/apt/lists/*

# --- Arm GNU toolchain (arm-none-eabi) ----------------------------------
RUN curl -fsSL -o /tmp/gcc.tar.xz \
    "https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_GNU_VERSION}/binrel/arm-gnu-toolchain-${ARM_GNU_VERSION}-x86_64-arm-none-eabi.tar.xz" \
    && mkdir -p /opt/arm-gnu-toolchain \
    && tar -xJf /tmp/gcc.tar.xz -C /opt/arm-gnu-toolchain --strip-components=1 \
    && rm /tmp/gcc.tar.xz

# --- CMSIS-Toolbox (cbuild, csolution, cpackget, ...) --------------------
RUN curl -fsSL -o /tmp/ctools.tar.gz \
    "https://github.com/Open-CMSIS-Pack/cmsis-toolbox/releases/download/${CMSIS_TOOLBOX_VERSION}/cmsis-toolbox-linux-amd64.tar.gz" \
    && mkdir -p /opt/cmsis-toolbox \
    && tar -xzf /tmp/ctools.tar.gz -C /opt/cmsis-toolbox --strip-components=1 \
    && rm /tmp/ctools.tar.gz

ENV PATH="/opt/cmsis-toolbox/bin:/opt/arm-gnu-toolchain/bin:${PATH}"

ENV GCC_TOOLCHAIN_15_2_1=/opt/arm-gnu-toolchain/bin

ENV CMSIS_PACK_ROOT=/opt/cmsis-packs

RUN mkdir -p /opt/cmsis-packs && chown vscode:vscode /opt/cmsis-packs
