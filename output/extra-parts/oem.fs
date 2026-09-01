#!/bin/sh -e
find "/home/linux/rk3506/output/extra-parts/oem" -user 1000 			-exec chown -ch 0:0 {} \;
"/home/linux/rk3506/device/rockchip/common/scripts/mk-image.sh" 			-t "ubi-ubifs" -s "16384K" -l "oem" 			"/home/linux/rk3506/output/extra-parts/oem" "/home/linux/rk3506/output/extra-parts/oem.img"
