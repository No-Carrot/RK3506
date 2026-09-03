#!/bin/sh
#
# Modules init
#


start() {
	# Built-in platform drivers need no action here. Loading every module by its
	# filename made modprobe look for names such as "vfat.ko" and produced
	# misleading startup failures. EtherCAT loads only its own modules later.
	return 0
}

stop() {
#	find /lib/modules/$(uname -r)/kernel/ -name "*.ko" | xargs -I {} sh -c 'modprobe -r $(basename {})'
	echo "skip rmmmod *.ko"
}

case "$1" in
	start)
		start
		;;
	stop)
		stop
		;;
	*)
		echo "Usage: $0 {start|stop|restart}"
		exit 1
esac

exit $?
