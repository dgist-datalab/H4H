#!/bin/bash

sudo rm /media/H4H/fio

sudo fio --randrepeat=1 \
	--ioengine=libaio \
	--name=fio\
	--filename=/media/H4H/fio \
	--bs=4k \
	--iodepth=1 \
	--size=1500M \
	--readwrite=write \
	--rwmixread=0 \
	--overwrite=0 \
	--numjobs=8 \
	--direct=0 \
	--buffered=0

