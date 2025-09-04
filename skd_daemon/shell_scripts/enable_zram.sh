#!/usr/bin/env bash
# -----------------------------------------------------------------------------
# Copyright (C) 2020 Intel Corporation. All rights reserved.
#
# The information and source code contained herein is the exclusive
# property of Intel Corporation and may not be disclosed, examined
# or reproduced in whole or in part without explicit written authorization
# from the company.
#
# Intel Confidential and Proprietary.
# -----------------------------------------------------------------------------

if [ $EUID -ne 0 ]
  then echo "Must run as root or with sudo."
  exit
fi

    echo "Using ZRAM"
    # remove all swap devices
     swapoff -a
     if [[ -e "/dev/zram0" ]]; then
         echo "Removing zram"
         umount -f /dev/zram0 >& /dev/null
         echo 0 > /sys/class/zram-control/hot_remove
     fi

    echo 0 > /sys/module/zswap/parameters/enabled

    # # setup iax module
    # modprobe iax
    # echo 0 > /sys/kernel/debug/iax/comp_delay_us
    # echo 0 > /sys/kernel/debug/iax/decomp_delay_us
    # echo 0 > /sys/kernel/debug/iax/total_comp_calls
    # echo 0 > /sys/kernel/debug/iax/total_comp_delay_ns
    # echo 0 > /sys/kernel/debug/iax/total_comp_bytes
    # echo 0 > /sys/kernel/debug/iax/total_decomp_calls
    # echo 0 > /sys/kernel/debug/iax/total_decomp_delay_ns
    # echo ${iax_use_deflate} > /sys/kernel/debug/iax/use_deflate
    # echo 0 > /sys/kernel/debug/iax/user0
    # echo 0 > /sys/kernel/debug/iax/user1
    # echo 0 > /sys/kernel/debug/iax/user2


    # setup zram
        echo "Setting up zram"
        modprobe zram num_devices=1
        #  modprobe zram num_devices=2 numa_zram_node=0

        if [[ ! -e "/dev/zram0" ]]; then
            cat /sys/class/zram-control/hot_add > /dev/null
        fi

        echo 220000000000 > /sys/block/zram0/disksize

        mkswap /dev/zram0 > /dev/null
        swapon /dev/zram0

# disable thp
echo never > /sys/kernel/mm/transparent_hugepage/enabled

# enable overcommit
echo 1 > /proc/sys/vm/overcommit_memory
echo 1 > /sys/module/zswap/parameters/enabled
