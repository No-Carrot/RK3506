#!/bin/sh
# Keep the ML307 4G module in reset.
echo 0 > /sys/class/leds/work-4g-rst/brightness
