#!/bin/bash
set -euo pipefail

ETHERCAT_DIR="/home/linux/rk3506/external/ethercat_igh"
cd "$ETHERCAT_DIR"

BUILD_DIR="${ETHERCAT_DIR}/out"
DEST="/home/linux/rk3506/buildroot/output/rockchip_rk3506/target"
KERNEL_DIR="/home/linux/rk3506/kernel"
KVER="6.1.118-rt36"
TOOLCHAIN2="/home/linux/rk3506/prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-"

# ==================== 前置检查 ====================

if [ ! -d "$DEST" ]; then
    echo "Error: 目标目录不存在: $DEST"
    echo "请先编译 buildroot"
    exit 1
fi

echo "目标目录: $DEST"
echo "内核版本: $KVER"

# ==================== 编译 ====================

echo "=== 1/5 配置 ==="
if ! ./bootstrap 2>/dev/null; then
    echo "Warning: bootstrap 失败，尝试继续（可能 configure 已存在）"
fi

TC_PREFIX="${DEST}/../host/bin/arm-buildroot-linux-gnueabihf-"

./configure --prefix="${BUILD_DIR}" \
            --with-linux-dir="${KERNEL_DIR}" \
            --enable-generic=yes \
            --enable-stmmac=yes \
            --with-stmmac-kernel=6.1 \
            --enable-8139too=no \
            --enable-e100=no \
            --enable-e1000=no \
            --enable-e1000e=no \
            --enable-r8169=no \
            --enable-ccat=no \
            --enable-igb=no \
            --enable-igc=no \
            --with-systemdsystemunitdir=no \
            --build=x86_64-pc-linux-gnu \
            --host=arm-buildroot-linux-gnueabihf \
            CC="${TC_PREFIX}gcc" \
            CXX="${TC_PREFIX}g++" \
            AR="${TC_PREFIX}ar" \
            LD="${TC_PREFIX}ld" \
            RANLIB="${TC_PREFIX}ranlib" \
            STRIP="${TC_PREFIX}strip"

echo "=== 2/5 编译用户空间 ==="
make -j"$(nproc)"

echo "=== 3/5 编译内核模块 ==="
# EtherCAT modules use non-stable kernel-internal networking interfaces.
# Force their rebuild so a newly built kernel is never paired with stale .ko files
# that still share the same vermagic string.
make -B ARCH=arm CROSS_COMPILE="${TOOLCHAIN2}" modules

echo "=== 4/5 安装 ==="
make install

# ==================== 复制到目标 ====================

echo "=== 5/5 复制到目标 ==="

MODULES_DIR="${DEST}/lib/modules/${KVER}"
ETHERCAT_MODULES_DIR="${MODULES_DIR}/kernel/drivers/ethercat"
mkdir -p \
    "${MODULES_DIR}" \
    "${ETHERCAT_MODULES_DIR}" \
    "${DEST}/usr/bin" \
    "${DEST}/usr/sbin" \
    "${DEST}/usr/lib" \
    "${DEST}/etc/sysconfig" \
    "${DEST}/etc/init.d" \
    "${DEST}/usr/share/bash-completion/completions"

# 内核驱动模块
cp -vf master/ec_master.ko      "${MODULES_DIR}/"
cp -vf devices/ec_generic.ko    "${MODULES_DIR}/"
[ -f devices/stmmac/ec_stmmac.ko ] && cp -vf devices/stmmac/ec_stmmac.ko "${MODULES_DIR}/"
cp -vf examples/mini/ec_mini.ko "${MODULES_DIR}/"
cp -vf master/ec_master.ko      "${ETHERCAT_MODULES_DIR}/"
cp -vf devices/ec_generic.ko    "${ETHERCAT_MODULES_DIR}/"
[ -f devices/stmmac/ec_stmmac.ko ] && cp -vf devices/stmmac/ec_stmmac.ko "${ETHERCAT_MODULES_DIR}/"

# 生成模块依赖（depmod -b 专为交叉编译场景设计，直接使用宿主机 depmod 即可）
depmod -b "${DEST}" "${KVER}"

# 应用程序
cp -vf "${BUILD_DIR}/bin/ethercat"     "${DEST}/usr/bin/"
cp -vf "${BUILD_DIR}/sbin/ethercatctl" "${DEST}/usr/sbin/"

relocate_paths() {
    sed -i \
        -e "s|${BUILD_DIR}/bin/|/usr/bin/|g" \
        -e "s|${BUILD_DIR}/sbin/|/usr/sbin/|g" \
        -e "s|${BUILD_DIR}/etc/|/etc/|g" \
        "$1"
}

relocate_paths "${DEST}/usr/sbin/ethercatctl"

# 共享库
cp -vf "${BUILD_DIR}/lib/libethercat.so.1.1.0" "${DEST}/usr/lib/"
ln -sf libethercat.so.1.1.0 "${DEST}/usr/lib/libethercat.so.1"
ln -sf libethercat.so.1.1.0 "${DEST}/usr/lib/libethercat.so"

# 配置文件
cp -vf "${BUILD_DIR}/etc/ethercat.conf"       "${DEST}/etc/"
cp -vf "${BUILD_DIR}/etc/init.d/ethercat"     "${DEST}/etc/init.d/"
cp -vf "${BUILD_DIR}/etc/sysconfig/ethercat" "${DEST}/etc/sysconfig/"

configure_ethercat_config() {
    local cfg="$1"

    [ -f "${cfg}" ] || return 0

    sed -i 's/^MASTER0_DEVICE=.*/MASTER0_DEVICE="ff:ff:ff:ff:ff:ff"/' "${cfg}"
    sed -i 's/^DEVICE_MODULES=.*/DEVICE_MODULES="stmmac"/' "${cfg}"
    sed -i 's/^UPDOWN_INTERFACES=.*/UPDOWN_INTERFACES=""/' "${cfg}"

    if grep -q '^MASTER_MODULE_OPTIONS=' "${cfg}"; then
        sed -i 's/^MASTER_MODULE_OPTIONS=.*/MASTER_MODULE_OPTIONS="thread_cpu=2 idle_thread_rt_prio=15 op_thread_rt_prio=42"/' "${cfg}"
    else
        printf '\nMASTER_MODULE_OPTIONS="thread_cpu=2 idle_thread_rt_prio=15 op_thread_rt_prio=42"\n' >> "${cfg}"
    fi
}

# 设置默认 EtherCAT 配置：
# - MASTER0_DEVICE=ff:ff:ff:ff:ff:ff: 接受 ec_stmmac 提供的第一个设备
# - DEVICE_MODULES=stmmac: 使用专用 stmmac EtherCAT 驱动，绕开 ec_generic/PF_PACKET
# - UPDOWN_INTERFACES 为空：ec_stmmac 接管硬件时不依赖普通 eth0
# - MASTER_MODULE_OPTIONS=...: 将主站内核线程固定到 CPU2，OP 线程优先级高于应用周期线程
configure_ethercat_config "${DEST}/etc/ethercat.conf"
configure_ethercat_config "${DEST}/etc/sysconfig/ethercat"

# 修正 init 脚本中写死的路径
relocate_paths "${DEST}/etc/init.d/ethercat"

# bash 命令补全
cp -vf "${BUILD_DIR}/share/bash-completion/completions/ethercat" \
    "${DEST}/usr/share/bash-completion/completions/"

echo "=== 完成 ==="
