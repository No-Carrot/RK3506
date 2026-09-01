#!/bin/sh -e
find "/home/linux/rk3506/output/extra-parts/userdata" -user 1000 			-exec chown -ch 0:0 {} \;
"/home/linux/rk3506/device/rockchip/common/scripts/mk-image.sh" 			-t "ubi-ubifs" -s "2048M" -l "userdata" 			"/home/linux/rk3506/output/extra-parts/userdata" "/home/linux/rk3506/output/extra-parts/userdata.img"
