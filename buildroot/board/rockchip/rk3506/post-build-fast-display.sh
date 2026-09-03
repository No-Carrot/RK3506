#!/bin/bash -e

TARGET=$1
SDK_DIR=$(cd "$(dirname "$0")/../../../.." && pwd)
SYSROOT_LIB="$SDK_DIR/buildroot/output/rockchip_rk3506/host/arm-buildroot-linux-gnueabihf/sysroot/usr/lib"

SH_LV_DEMO=$(ls $TARGET/etc/init.d/S* | grep lv_demo || true)
SH_SERVO_BACKEND=$(find "$TARGET/etc/init.d" -maxdepth 1 -type f -name 'S*servo_backend' ! -name 'S70servo_backend' -print | head -n 1 || true)
SH_ASYNC_COMMIT=$(ls $TARGET/etc/init.d/S* | grep sync-commit || true)

mkdir -p $TARGET/etc/init.d/pre_init

for i in "${SH_LV_DEMO}" "${SH_SERVO_BACKEND}" "${SH_ASYNC_COMMIT}" ; do
	if [ -n "$i" ]; then
		mv $i $TARGET/etc/init.d/pre_init/.
	fi
done

# async-commit need first
if [ -e "$TARGET/etc/init.d/pre_init/S05async-commit.sh" ]; then
	mv "$TARGET/etc/init.d/pre_init/S05async-commit.sh" \
		"$TARGET/etc/init.d/pre_init/S00async-commit.sh"
fi

# Do not skip filesystem checks by default. A stale marker from an incremental
# build would otherwise hide errors after an unexpected shutdown.
rm -f "$TARGET/.skip_fsck"

# This product has OTG, Wi-Fi/BT, and 4G disabled. Remove services and
# modules that may remain when rebuilding from an earlier wireless-enabled tree.
rm -f "$TARGET/etc/init.d/S40bluetoothd" \
	"$TARGET/etc/init.d/S50usbdevice.sh" \
	"$TARGET/etc/init.d/S89ML37init" \
	"$TARGET/etc/init.d/S89WIFI_EN" \
	"$TARGET/etc/init.d/S90rkwifibt"
rm -f "$TARGET/usr/bin/rkwifibt.sh" \
	"$TARGET/usr/bin/wifi_en.sh" \
	"$TARGET/etc/ws73_cfg.ini" \
	"$TARGET/etc/udev/hwdb.d/20-bluetooth-vendor-product.hwdb"
rm -rf "$TARGET/usr/lib/modules/ws73" \
	"$TARGET/etc/ws73" \
	"$TARGET/rockchip-test"
rm -f "$TARGET/etc/init.d/S70servo_backend" \
	"$TARGET/etc/init.d/S03modules_init.sh" \
	"$TARGET/etc/init.d/S13irqbalance" \
	"$TARGET/etc/init.d/S35iptables" \
	"$TARGET/etc/init.d/S40network" \
	"$TARGET/etc/init.d/S49ntp" \
	"$TARGET/etc/init.d/S99-auto-reboot" \
	"$TARGET/etc/init.d/S99appinit" \
	"$TARGET/etc/init.d/S99fstrim" \
	"$TARGET/etc/init.d/S99input-event-daemon" \
	"$TARGET/etc/init.d/pre_init/S08servo_backend" \
	"$TARGET/etc/init.d/pre_init/S09servo_backend"
rm -f "$TARGET/etc/init.d/pre_init/S12servo_backend"
rm -f "$TARGET/usr/sbin/irqbalance" \
	"$TARGET/usr/share/man/man1/irqbalance.1" \
	"$TARGET/etc/iptables.conf" \
	"$TARGET/usr/bin/iptables-xml" \
	"$TARGET/usr/sbin/iptables" \
	"$TARGET/usr/sbin/iptables-apply" \
	"$TARGET/usr/sbin/iptables-legacy" \
	"$TARGET/usr/sbin/iptables-legacy-restore" \
	"$TARGET/usr/sbin/iptables-legacy-save" \
	"$TARGET/usr/sbin/iptables-restore" \
	"$TARGET/usr/sbin/iptables-save" \
	"$TARGET/usr/sbin/ip6tables" \
	"$TARGET/usr/sbin/ip6tables-apply" \
	"$TARGET/usr/sbin/ip6tables-legacy" \
	"$TARGET/usr/sbin/ip6tables-legacy-restore" \
	"$TARGET/usr/sbin/ip6tables-legacy-save" \
	"$TARGET/usr/sbin/ip6tables-restore" \
	"$TARGET/usr/sbin/ip6tables-save" \
	"$TARGET/usr/sbin/xtables-legacy-multi" \
	"$TARGET/usr/lib/libxtables.so" \
	"$TARGET/usr/lib/libxtables.so.12" \
	"$TARGET/usr/lib/libxtables.so.12.7.0" \
	"$TARGET/usr/sbin/ntpd" \
	"$TARGET/usr/bin/ntpdate" \
	"$TARGET/etc/ntp.conf"
rm -rf "$TARGET/etc/irqbalance.d" \
	"$TARGET/usr/lib/xtables" \
	"$TARGET/usr/share/xtables"

rm -f "$TARGET/etc/init.d/S10udev" \
	"$TARGET/usr/bin/udevadm" \
	"$TARGET/usr/sbin/udevadm" \
	"$TARGET/usr/sbin/udevd" \
	"$TARGET/usr/lib/libudev.so"*
rm -rf "$TARGET/etc/udev" \
	"$TARGET/usr/lib/udev"

ln -sf busybox "$TARGET/usr/bin/sh"
sed -i 's|:/bin/bash$|:/bin/sh|' "$TARGET/etc/passwd"
sed -i 's|^/bin/bash$|/bin/sh|' "$TARGET/etc/shells" 2>/dev/null || true

# The motor controller uses LVGL DRM/evdev and EtherCAT only. Drop stale
# RKADK/Rockit multimedia artifacts that can remain after incremental builds
# from the Rockchip UI demo and otherwise probe missing VENC/AVS/IPC devices.
rm -f "$TARGET/usr/bin/rkadk_disp_test" \
	"$TARGET/usr/bin/rkadk_player_test" \
	"$TARGET/usr/bin/rkadk_ui_test" \
	"$TARGET/usr/lib/librkadk.so" \
	"$TARGET/usr/lib/librockit.so" \
	"$TARGET/usr/lib/librkdemuxer.so" \
	"$TARGET/usr/lib/librkmuxer.so" \
	"$TARGET/usr/lib/librkaudio.so" \
	"$TARGET/usr/lib/librkaudio_common.so" \
	"$TARGET/usr/lib/librkaudio_vqe.so"
rm -rf "$TARGET/usr/include/rkadk" \
	"$TARGET/usr/include/lvgl/lv_drivers/rkadk" \
	"$TARGET/etc/rkadk" \
	"$TARGET/oem/etc/rkadk"

# This controller image has no audio hardware in use. Prevent stale packages
# from probing the absent ES8328 codec or emitting ALSA/PulseAudio errors.
rm -f "$TARGET/etc/init.d/S11alsa-utils" \
	"$TARGET/usr/sbin/alsactl" \
	"$TARGET/usr/sbin/alsaconf" \
	"$TARGET/usr/bin/alsamixer" \
	"$TARGET/usr/bin/alsaucm" \
	"$TARGET/usr/lib/udev/rules.d/60-persistent-alsa.rules" \
	"$TARGET/usr/lib/udev/rules.d/78-sound-card.rules" \
	"$TARGET/usr/lib/libasound.so"*
rm -rf "$TARGET/etc/alsa" \
	"$TARGET/etc/pulse" \
	"$TARGET/usr/lib/alsa-lib" \
	"$TARGET/usr/share/alsa" \
	"$TARGET/var/lib/alsa"

# Keep the production motor image focused on LVGL DRM and EtherCAT. These
# tools are useful during board bring-up, but they are not runtime
# dependencies and incremental builds otherwise leave them in the rootfs.
rm -f "$TARGET/usr/bin/asc2log" \
	"$TARGET/usr/bin/bcmserver" \
	"$TARGET/usr/bin/brctl" \
	"$TARGET/usr/sbin/brctl" \
	"$TARGET/usr/bin/can-calc-bit-timing" \
	"$TARGET/usr/bin/canbusload" \
	"$TARGET/usr/bin/candump" \
	"$TARGET/usr/bin/canfdtest" \
	"$TARGET/usr/bin/cangen" \
	"$TARGET/usr/bin/cangw" \
	"$TARGET/usr/bin/canlogserver" \
	"$TARGET/usr/bin/canplayer" \
	"$TARGET/usr/bin/cansend" \
	"$TARGET/usr/bin/cansequence" \
	"$TARGET/usr/bin/cansniffer" \
	"$TARGET/usr/bin/cyclictest" \
	"$TARGET/usr/bin/deadline_test" \
	"$TARGET/usr/bin/evtest" \
	"$TARGET/usr/bin/gcore" \
	"$TARGET/usr/bin/gdb" \
	"$TARGET/usr/bin/gdb-add-index" \
	"$TARGET/usr/bin/gdbserver" \
	"$TARGET/usr/bin/gpiodetect" \
	"$TARGET/usr/bin/gpiofind" \
	"$TARGET/usr/bin/gpioget" \
	"$TARGET/usr/bin/gpioinfo" \
	"$TARGET/usr/bin/gpiomon" \
	"$TARGET/usr/bin/gpioset" \
	"$TARGET/usr/bin/hackbench" \
	"$TARGET/usr/bin/inotifywait" \
	"$TARGET/usr/bin/input-event-daemon" \
	"$TARGET/usr/bin/iperf" \
	"$TARGET/usr/bin/iperf3" \
	"$TARGET/usr/bin/isotpdump" \
	"$TARGET/usr/bin/isotpperf" \
	"$TARGET/usr/bin/isotprecv" \
	"$TARGET/usr/bin/isotpsend" \
	"$TARGET/usr/bin/isotpserver" \
	"$TARGET/usr/bin/isotpsniffer" \
	"$TARGET/usr/bin/isotptun" \
	"$TARGET/usr/bin/j1939acd" \
	"$TARGET/usr/bin/j1939cat" \
	"$TARGET/usr/bin/j1939spy" \
	"$TARGET/usr/bin/j1939sr" \
	"$TARGET/usr/bin/log2asc" \
	"$TARGET/usr/bin/log2long" \
	"$TARGET/usr/bin/kmsgrab" \
	"$TARGET/usr/bin/mhz" \
	"$TARGET/usr/bin/modetest" \
	"$TARGET/usr/bin/pi_stress" \
	"$TARGET/usr/bin/pip_stress" \
	"$TARGET/usr/bin/pmqtest" \
	"$TARGET/usr/bin/ptsematest" \
	"$TARGET/usr/bin/qml" \
	"$TARGET/usr/bin/qmlpreview" \
	"$TARGET/usr/bin/qmlscene" \
	"$TARGET/usr/bin/qmltestrunner" \
	"$TARGET/usr/bin/qmltime" \
	"$TARGET/usr/bin/queuelat" \
	"$TARGET/usr/bin/rg" \
	"$TARGET/usr/bin/rt-migrate-test" \
	"$TARGET/usr/bin/signaltest" \
	"$TARGET/usr/bin/sigwaittest" \
	"$TARGET/usr/bin/slcan_attach" \
	"$TARGET/usr/bin/slcand" \
	"$TARGET/usr/bin/slcanpty" \
	"$TARGET/usr/bin/ssdd" \
	"$TARGET/usr/bin/stress" \
	"$TARGET/usr/bin/stressapptest" \
	"$TARGET/usr/bin/svsematest" \
	"$TARGET/usr/lib/libgpiod.so"* \
	"$TARGET/usr/lib/libgpiodcxx.so"* \
	"$TARGET/usr/lib/libQt5"*
rm -rf "$TARGET/usr/lib/metatypes" \
	"$TARGET/usr/lib/qt" \
	"$TARGET/usr/qml" \
	"$TARGET/usr/share/qt5" \
	"$TARGET/etc/generate_logs.d" \
	"$TARGET/etc/input-event-daemon.conf.d" \
	"$TARGET/info"

# Remove non-runtime bring-up tools and filesystems not used by the NAND/UBIFS
# motor image. Keep busybox utilities, LVGL DRM/evdev, and EtherCAT runtime
# only.
rm -f "$TARGET/usr/bin/adbd" \
	"$TARGET/usr/bin/amixer" \
	"$TARGET/usr/bin/aplay" \
	"$TARGET/usr/bin/arecord" \
	"$TARGET/usr/bin/aserver" \
	"$TARGET/usr/bin/bash" \
	"$TARGET/usr/bin/ble_client_sample" \
	"$TARGET/usr/bin/ble_server_sample" \
	"$TARGET/usr/bin/bunzip2" \
	"$TARGET/usr/bin/bzip2" \
	"$TARGET/usr/bin/bzip2recover" \
	"$TARGET/usr/bin/bzcat" \
	"$TARGET/usr/bin/bzdiff" \
	"$TARGET/usr/bin/bzgrep" \
	"$TARGET/usr/bin/bzmore" \
	"$TARGET/usr/bin/chattr" \
	"$TARGET/usr/bin/compile_et" \
	"$TARGET/usr/bin/cyclicdeadline" \
	"$TARGET/usr/bin/determine_maximum_mpps.sh" \
	"$TARGET/usr/bin/dhrystone" \
	"$TARGET/usr/bin/fc-cache" \
	"$TARGET/usr/bin/fc-cat" \
	"$TARGET/usr/bin/fc-conflist" \
	"$TARGET/usr/bin/fc-list" \
	"$TARGET/usr/bin/fc-match" \
	"$TARGET/usr/bin/fc-pattern" \
	"$TARGET/usr/bin/fc-query" \
	"$TARGET/usr/bin/fc-scan" \
	"$TARGET/usr/bin/fc-validate" \
	"$TARGET/usr/bin/generate_logs" \
	"$TARGET/usr/bin/io" \
	"$TARGET/usr/bin/libevdev-tweak-device" \
	"$TARGET/usr/bin/logcat" \
	"$TARGET/usr/bin/lowntfs-3g" \
	"$TARGET/usr/bin/lsattr" \
	"$TARGET/usr/bin/lsusb" \
	"$TARGET/usr/bin/mcp251xfd-dump" \
	"$TARGET/usr/bin/memhog" \
	"$TARGET/usr/bin/memtester" \
	"$TARGET/usr/bin/migspeed" \
	"$TARGET/usr/bin/migratepages" \
	"$TARGET/usr/bin/mk_cmds" \
	"$TARGET/usr/bin/ml307.sh" \
	"$TARGET/usr/bin/mouse-dpi-tool" \
	"$TARGET/usr/bin/ntfs-3g" \
	"$TARGET/usr/bin/ntfs-3g.probe" \
	"$TARGET/usr/bin/ntfscat" \
	"$TARGET/usr/bin/ntfscluster" \
	"$TARGET/usr/bin/ntfscmp" \
	"$TARGET/usr/bin/ntfsfix" \
	"$TARGET/usr/bin/ntfsinfo" \
	"$TARGET/usr/bin/ntfsls" \
	"$TARGET/usr/bin/numactl" \
	"$TARGET/usr/bin/numademo" \
	"$TARGET/usr/bin/numastat" \
	"$TARGET/usr/bin/on_ac_power" \
	"$TARGET/usr/bin/oslat" \
	"$TARGET/usr/bin/pm-is-supported" \
	"$TARGET/usr/bin/play" \
	"$TARGET/usr/bin/power-key.sh" \
	"$TARGET/usr/bin/rec" \
	"$TARGET/usr/bin/scp" \
	"$TARGET/usr/bin/sftp" \
	"$TARGET/usr/bin/sle_client_sample" \
	"$TARGET/usr/bin/sle_server_sample" \
	"$TARGET/usr/bin/sox" \
	"$TARGET/usr/bin/soxi" \
	"$TARGET/usr/bin/testj1939" \
	"$TARGET/usr/bin/touchpad-edge-detector" \
	"$TARGET/usr/bin/ts_calibrate" \
	"$TARGET/usr/bin/ts_conf" \
	"$TARGET/usr/bin/ts_finddev" \
	"$TARGET/usr/bin/ts_harvest" \
	"$TARGET/usr/bin/ts_print" \
	"$TARGET/usr/bin/ts_print_mt" \
	"$TARGET/usr/bin/ts_print_raw" \
	"$TARGET/usr/bin/ts_test" \
	"$TARGET/usr/bin/ts_test_mt" \
	"$TARGET/usr/bin/ts_uinput" \
	"$TARGET/usr/bin/ts_verify" \
	"$TARGET/usr/bin/update" \
	"$TARGET/usr/bin/updateEngine" \
	"$TARGET/usr/bin/usbhid-dump" \
	"$TARGET/usr/bin/usb-devices" \
	"$TARGET/usr/bin/vendor_storage" \
	"$TARGET/usr/sbin/badblocks" \
	"$TARGET/usr/sbin/bridge" \
	"$TARGET/usr/sbin/ctstat" \
	"$TARGET/usr/sbin/dosfsck" \
	"$TARGET/usr/sbin/dosfslabel" \
	"$TARGET/usr/sbin/dumpe2fs" \
	"$TARGET/usr/sbin/e2freefrag" \
	"$TARGET/usr/sbin/e2fsck" \
	"$TARGET/usr/sbin/e2label" \
	"$TARGET/usr/sbin/e2mmpstatus" \
	"$TARGET/usr/sbin/e2undo" \
	"$TARGET/usr/sbin/e4crypt" \
	"$TARGET/usr/sbin/fatlabel" \
	"$TARGET/usr/sbin/fatresize" \
	"$TARGET/usr/sbin/filefrag" \
	"$TARGET/usr/sbin/fsck" \
	"$TARGET/usr/sbin/fsck.ext2" \
	"$TARGET/usr/sbin/fsck.ext3" \
	"$TARGET/usr/sbin/fsck.ext4" \
	"$TARGET/usr/sbin/fsck.fat" \
	"$TARGET/usr/sbin/fsck.msdos" \
	"$TARGET/usr/sbin/fsck.ntfs" \
	"$TARGET/usr/sbin/fsck.vfat" \
	"$TARGET/usr/sbin/fstrim" \
	"$TARGET/usr/sbin/genl" \
	"$TARGET/usr/sbin/ifstat" \
	"$TARGET/usr/sbin/ip" \
	"$TARGET/usr/sbin/lnstat" \
	"$TARGET/usr/sbin/logsave" \
	"$TARGET/usr/sbin/mke2fs" \
	"$TARGET/usr/sbin/mkfs.ext2" \
	"$TARGET/usr/sbin/mkfs.ext3" \
	"$TARGET/usr/sbin/mkfs.ext4" \
	"$TARGET/usr/sbin/mkfs.fat" \
	"$TARGET/usr/sbin/mkfs.msdos" \
	"$TARGET/usr/sbin/mkfs.ntfs" \
	"$TARGET/usr/sbin/mkfs.vfat" \
	"$TARGET/usr/sbin/mklost+found" \
	"$TARGET/usr/sbin/mkntfs" \
	"$TARGET/usr/sbin/mount.lowntfs-3g" \
	"$TARGET/usr/sbin/mount.ntfs" \
	"$TARGET/usr/sbin/mount.ntfs-3g" \
	"$TARGET/usr/sbin/nstat" \
	"$TARGET/usr/sbin/ntfsclone" \
	"$TARGET/usr/sbin/ntfscp" \
	"$TARGET/usr/sbin/ntfslabel" \
	"$TARGET/usr/sbin/ntfsresize" \
	"$TARGET/usr/sbin/ntfsundelete" \
	"$TARGET/usr/sbin/parted" \
	"$TARGET/usr/sbin/partprobe" \
	"$TARGET/usr/sbin/pm-hibernate" \
	"$TARGET/usr/sbin/pm-is-supported" \
	"$TARGET/usr/sbin/pm-powersave" \
	"$TARGET/usr/sbin/pm-suspend" \
	"$TARGET/usr/sbin/pm-suspend-hybrid" \
	"$TARGET/usr/sbin/resize2fs" \
	"$TARGET/usr/sbin/routel" \
	"$TARGET/usr/sbin/rtacct" \
	"$TARGET/usr/sbin/rtmon" \
	"$TARGET/usr/sbin/rtstat" \
	"$TARGET/usr/sbin/ss" \
	"$TARGET/usr/sbin/tc" \
	"$TARGET/usr/sbin/tune2fs"
rm -f "$TARGET/usr/lib/libatopology.so"* \
	"$TARGET/usr/lib/libcom_err.so"* \
	"$TARGET/usr/lib/libcap.so"* \
	"$TARGET/usr/lib/libcrypto.so"* \
	"$TARGET/usr/lib/libcurl.so"* \
	"$TARGET/usr/lib/libe2p.so"* \
	"$TARGET/usr/lib/libevent"* \
	"$TARGET/usr/lib/libexpat.so"* \
	"$TARGET/usr/lib/libext2fs.so"* \
	"$TARGET/usr/lib/libfontconfig.so"* \
	"$TARGET/usr/lib/libform.so"* \
	"$TARGET/usr/lib/libgmp.so"* \
	"$TARGET/usr/lib/libgmpxx.so"* \
	"$TARGET/usr/lib/libhistory.so"* \
	"$TARGET/usr/lib/libip4tc.so"* \
	"$TARGET/usr/lib/libip6tc.so"* \
	"$TARGET/usr/lib/libiperf.so"* \
	"$TARGET/usr/lib/libjpeg.so"* \
	"$TARGET/usr/lib/libmad.so"* \
	"$TARGET/usr/lib/libmenu.so"* \
	"$TARGET/usr/lib/libncurses.so"* \
	"$TARGET/usr/lib/libntfs-3g.so"* \
	"$TARGET/usr/lib/libnuma.so"* \
	"$TARGET/usr/lib/libpanel.so"* \
	"$TARGET/usr/lib/libparted.so"* \
	"$TARGET/usr/lib/libparted-fs-resize.so"* \
	"$TARGET/usr/lib/libpcre2"* \
	"$TARGET/usr/lib/libpsx.so"* \
	"$TARGET/usr/lib/libreadline.so"* \
	"$TARGET/usr/lib/librga.so"* \
	"$TARGET/usr/lib/librkyuvscaler.so" \
	"$TARGET/usr/lib/libsocketcan.so"* \
	"$TARGET/usr/lib/libsox.so"* \
	"$TARGET/usr/lib/libss.so"* \
	"$TARGET/usr/lib/libssl.so"* \
	"$TARGET/usr/lib/libstdc++.so.6.0.30-gdb.py" \
	"$TARGET/usr/lib/libthread_db.so"* \
	"$TARGET/usr/lib/libts.so"* \
	"$TARGET/usr/lib/libturbojpeg.so"* \
	"$TARGET/usr/lib/libusb-1.0.so"* \
	"$TARGET/usr/lib/libvendor_storage.so"
rm -rf "$TARGET/etc/fonts" \
	"$TARGET/etc/bash.bashrc" \
	"$TARGET/etc/mke2fs.conf" \
	"$TARGET/etc/network" \
	"$TARGET/etc/pm" \
	"$TARGET/etc/ssl" \
	"$TARGET/etc/ts.conf" \
	"$TARGET/etc/usbmount" \
	"$TARGET/usr/include" \
	"$TARGET/usr/lib/engines-3" \
	"$TARGET/usr/lib/ntfs-3g" \
	"$TARGET/usr/lib/ossl-modules" \
	"$TARGET/usr/lib/pm-utils" \
	"$TARGET/usr/lib/pkgconfig" \
	"$TARGET/usr/lib/tc" \
	"$TARGET/usr/lib/ts" \
	"$TARGET/usr/libexec/sftp-server" \
	"$TARGET/usr/share/bash-completion" \
	"$TARGET/usr/share/et" \
	"$TARGET/usr/share/fontconfig" \
	"$TARGET/usr/share/gettext" \
	"$TARGET/usr/share/iproute2" \
	"$TARGET/usr/share/ss" \
	"$TARGET/usr/share/udhcpc" \
	"$TARGET/usr/share/usbmount" \
	"$TARGET/usr/share/xml" \
	"$TARGET/tmp/fontconfig" \
	"$TARGET/usr/.crates.toml" \
	"$TARGET/usr/.crates2.json" \
	"$TARGET/var/lib/arpd" \
	"$TARGET/usr/vqefiles"

# Drop target-side flash/network tools left by incremental builds. Keep
# SmileySans, FreeType, libpng, libbz2 and needle.png for the UI.
for tool in \
	disk-helper ethtool flash_erase flash_lock flash_unlock flashcp \
	mtd_debug mtdinfo nanddump nandtest nandwrite ubiattach ubiblock \
	ubicrc32 ubidetach ubihealthd ubiformat ubimkvol ubinfo ubinize \
	ubirename ubirmvol ubirsvol ubiupdatevol; do
	rm -f "$TARGET/usr/bin/$tool" "$TARGET/usr/sbin/$tool"
done

install_runtime_lib_chain() {
	local name=$1
	local src="$SYSROOT_LIB/$name"
	local target_name

	if [ ! -e "$src" ]; then
		echo "Missing runtime library in sysroot: $name" >&2
		exit 1
	fi

	if [ -L "$src" ]; then
		target_name=$(readlink "$src")
		ln -sf "$target_name" "$TARGET/usr/lib/$name"
		install_runtime_lib_chain "${target_name##*/}"
	else
		install -m 0644 "$src" "$TARGET/usr/lib/$name"
	fi
}

install_runtime_lib_chain libfreetype.so.6
install_runtime_lib_chain libpng.so
install_runtime_lib_chain libpng16.so.16
install_runtime_lib_chain libbz2.so.1.0

# post-rootfs installs the kernel module tree after package hooks. Install the
# EtherCAT runtime here so the final UBI always contains modules matching the
# kernel that was just installed.
ETHERCAT_DIR="$SDK_DIR/external/ethercat_igh"
ETHERCAT_OUT="$ETHERCAT_DIR/out"
KERNEL_RELEASE=$(make -s -C "$SDK_DIR/kernel" kernelrelease)
MODULE_DIR="$TARGET/lib/modules/$KERNEL_RELEASE/kernel/drivers/ethercat"

needs_ethercat_rebuild() {
	for module in "$ETHERCAT_DIR/master/ec_master.ko" \
		"$ETHERCAT_DIR/devices/ec_generic.ko" \
		"$ETHERCAT_DIR/devices/stmmac/ec_stmmac.ko"; do
		[ -f "$module" ] || return 0
	done

	for dep in "$SDK_DIR/kernel/include/generated/autoconf.h" \
		"$SDK_DIR/kernel/Module.symvers" \
		"$ETHERCAT_DIR/build.sh" \
		"$ETHERCAT_DIR/master/device.c" \
		"$ETHERCAT_DIR/master/device.h" \
		"$ETHERCAT_DIR/master/module.c" \
		"$ETHERCAT_DIR/devices/generic.c" \
		"$ETHERCAT_DIR/devices/stmmac/Kbuild.in" \
		"$ETHERCAT_DIR/devices/stmmac/stmmac_main-6.1-ethercat.c" \
		"$ETHERCAT_DIR/devices/stmmac/dwmac-rk-6.1-ethercat.c"; do
		[ -e "$dep" ] || continue
		[ "$dep" -nt "$ETHERCAT_DIR/master/ec_master.ko" ] && return 0
		[ "$dep" -nt "$ETHERCAT_DIR/devices/ec_generic.ko" ] && return 0
		[ "$dep" -nt "$ETHERCAT_DIR/devices/stmmac/ec_stmmac.ko" ] && return 0
	done

	return 1
}

if needs_ethercat_rebuild; then
	echo "Rebuilding EtherCAT modules for kernel $KERNEL_RELEASE"
	"$ETHERCAT_DIR/build.sh"
fi

install -d -m 0755 "$MODULE_DIR" "$TARGET/etc/sysconfig" "$TARGET/etc/init.d/pre_init"
install -m 0644 "$SDK_DIR/buildroot/board/rockchip/rk3506/fs-overlay/etc/profile" \
	"$TARGET/etc/profile"
install -m 0755 "$SDK_DIR/buildroot/board/rockchip/rk3506/fast-display-overlay/etc/init.d/pre_init/S01realtime-tune" \
	"$TARGET/etc/init.d/pre_init/S01realtime-tune"
install -m 0755 "$SDK_DIR/app/lvgl_demo/motor_demo/S09servo_backend" \
	"$TARGET/etc/init.d/pre_init/S08servo_backend"
install -m 0755 "$SDK_DIR/app/lvgl_demo/motor_demo/S10lv_demo" \
	"$TARGET/etc/init.d/pre_init/S02lv_demo"
install -m 0755 "$SDK_DIR/buildroot/board/rockchip/rk3506/fs-overlay/usr/bin/servo-rt-check" \
	"$TARGET/usr/bin/servo-rt-check"
ln -sf servo-rt-check "$TARGET/usr/bin/servo_rt-check"
rm -f "$TARGET/etc/init.d/S10lv_demo" \
	"$TARGET/etc/init.d/S12servo_backend" \
	"$TARGET/etc/init.d/pre_init/S10lv_demo"
rm -f "$TARGET/lib/modules/$KERNEL_RELEASE/ec_master.ko" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/ec_generic.ko" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/ec_stmmac.ko" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/ec_mini.ko" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.dep" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.dep.bin" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.alias" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.alias.bin" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.symbols" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.symbols.bin" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.softdep" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/modules.devname"
rm -rf "$TARGET/lib/modules/$KERNEL_RELEASE/kernel/drivers/mmc" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/drivers/net/can" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/drivers/net/ppp" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/drivers/scsi" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/drivers/usb" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/fs/exfat" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/fs/ext4" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/fs/fat" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/fs/jbd2" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/fs/mbcache.ko" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/fs/nls" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/fs/ntfs3" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/net/bridge" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/net/can" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/net/ipv4/netfilter" \
	"$TARGET/lib/modules/$KERNEL_RELEASE/kernel/net/netfilter"
install -m 0644 "$ETHERCAT_DIR/master/ec_master.ko" "$MODULE_DIR/ec_master.ko"
install -m 0644 "$ETHERCAT_DIR/devices/ec_generic.ko" "$MODULE_DIR/ec_generic.ko"
install -m 0644 "$ETHERCAT_DIR/devices/stmmac/ec_stmmac.ko" "$MODULE_DIR/ec_stmmac.ko"
install -D -m 0755 "$ETHERCAT_OUT/bin/ethercat" "$TARGET/usr/bin/ethercat"
install -D -m 0755 "$ETHERCAT_OUT/sbin/ethercatctl" "$TARGET/usr/sbin/ethercatctl"
install -D -m 0644 "$ETHERCAT_OUT/etc/ethercat.conf" "$TARGET/etc/ethercat.conf"
install -D -m 0644 "$ETHERCAT_OUT/etc/sysconfig/ethercat" "$TARGET/etc/sysconfig/ethercat"
install -D -m 0755 "$ETHERCAT_OUT/etc/init.d/ethercat" "$TARGET/etc/init.d/ethercat"

STRIP_TOOL="$SDK_DIR/buildroot/output/rockchip_rk3506/host/bin/arm-buildroot-linux-gnueabihf-strip"
if [ -x "$STRIP_TOOL" ]; then
	"$STRIP_TOOL" --strip-unneeded "$TARGET/usr/bin/ethercat" 2>/dev/null || true
	"$STRIP_TOOL" --strip-debug "$MODULE_DIR/ec_master.ko" "$MODULE_DIR/ec_generic.ko" "$MODULE_DIR/ec_stmmac.ko" 2>/dev/null || true
fi

sed -i \
	-e "s|$ETHERCAT_OUT/bin/|/usr/bin/|g" \
	-e "s|$ETHERCAT_OUT/sbin/|/usr/sbin/|g" \
	-e "s|$ETHERCAT_OUT/etc/|/etc/|g" \
	"$TARGET/usr/sbin/ethercatctl" "$TARGET/etc/init.d/ethercat"

for config in "$TARGET/etc/ethercat.conf" "$TARGET/etc/sysconfig/ethercat"; do
	sed -i \
		-e 's/^MASTER0_DEVICE=.*/MASTER0_DEVICE="ff:ff:ff:ff:ff:ff"/' \
		-e 's/^DEVICE_MODULES=.*/DEVICE_MODULES="stmmac"/' \
		-e 's/^UPDOWN_INTERFACES=.*/UPDOWN_INTERFACES=""/' \
		-e 's/^MASTER_MODULE_OPTIONS=.*/MASTER_MODULE_OPTIONS="thread_cpu=2 idle_thread_rt_prio=15 op_thread_rt_prio=42"/' \
		"$config"
	grep -q '^MASTER_MODULE_OPTIONS=' "$config" || \
		printf '\nMASTER_MODULE_OPTIONS="thread_cpu=2 idle_thread_rt_prio=15 op_thread_rt_prio=42"\n' >> "$config"
done

rm -f "$TARGET/etc/ld.so.cache"
LDCONFIG_TOOL="$SDK_DIR/device/rockchip/common/tools/x86_64/ldconfig"
if [ -x "$LDCONFIG_TOOL" ] && grep -q glibc-ld.so.cache "$TARGET"/lib/ld-linux* 2>/dev/null; then
	"$LDCONFIG_TOOL" -r "$TARGET" || rm -f "$TARGET/etc/ld.so.cache"
fi

depmod -a -b "$TARGET" "$KERNEL_RELEASE"
