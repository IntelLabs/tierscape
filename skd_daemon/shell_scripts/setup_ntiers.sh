# scipt dir
HOME_DIR=$(dirname $(realpath $0))

source ${HOME_DIR}/sanity_checks.sh


${HOME_DIR}/enable_zram.sh

# swapoff -a

echo "Disabling the prefetching"
echo 0 > /proc/sys/vm/page-cluster
echo 0 > /sys/kernel/mm/swap/vma_ra_enabled
echo 0 > /sys/module/zswap/parameters/same_filled_pages_enabled
echo Y >  /sys/module/zswap/parameters/ntier_enabled


# ------------- DEFLATE TIERS -------------------


echo zsmalloc > /sys/module/zswap/parameters/zpool
echo zstd > /sys/module/zswap/parameters/compressor
echo 2 > /sys/module/zswap/parameters/backing_store

echo zsmalloc > /sys/module/zswap/parameters/zpool
echo zstd > /sys/module/zswap/parameters/compressor
echo 0 > /sys/module/zswap/parameters/backing_store

echo zsmalloc > /sys/module/zswap/parameters/zpool
echo lzo > /sys/module/zswap/parameters/compressor
echo 0 > /sys/module/zswap/parameters/backing_store

# echo zbud > /sys/module/zswap/parameters/zpool
# echo lz4 > /sys/module/zswap/parameters/compressor
# echo 0 > /sys/module/zswap/parameters/backing_store


# echo deflate > /sys/module/zswap/parameters/compressor
# echo zstd > /sys/module/zswap/parameters/compressor
# echo zbud > /sys/module/zswap/parameters/zpool
# echo lz4 > /sys/module/zswap/parameters/compressor
# echoe zsmalloc > /sys/module/zswap/parameters/zpool




echo 0 > /sys/module/zswap/parameters/same_filled_pages_enabled
sysctl kernel.zswap_print_stat=1
# dmesg |tail -n 20


# =============
# Google tier
# echo zsmalloc > /sys/module/zswap/parameters/zpool
# echo lzo > /sys/module/zswap/parameters/compressor
# echo 0 > /sys/module/zswap/parameters/backing_store

# # TMO tier
# echo zsmalloc > /sys/module/zswap/parameters/zpool
# echo zstd > /sys/module/zswap/parameters/compressor
# echo 0 > /sys/module/zswap/parameters/backing_store

# echo zsmalloc > /sys/module/zswap/parameters/zpool
# echo lz4 > /sys/module/zswap/parameters/compressor
# echo 2 > /sys/module/zswap/parameters/backing_store

# echo zbud > /sys/module/zswap/parameters/zpool
# echo lz4 > /sys/module/zswap/parameters/compressor
# echo 2 > /sys/module/zswap/parameters/backing_store