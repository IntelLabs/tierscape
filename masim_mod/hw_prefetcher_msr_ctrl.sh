#!/bin/bash
set -euo pipefail

MSR_ADDR="0x1a4"
CMD="${1:-status}"

# load MSR moduel
if ! lsmod | grep -q msr; then
    sudo modprobe msr
fi

# Function to read value from CPU 0, below the MSR is setting for all CPUs
get_msr_val() {
    sudo rdmsr -p 0 "$MSR_ADDR"
}

# Current value (decimal)
val_hex="$(get_msr_val)"
val=$((16#$val_hex))

case "$CMD" in
    disable)
        # Set bits 0-3 into 1 (Disable prefetchers)
        # OR with 0xF (1111)
        new_val=$((val | 0xf))
        printf "Disabling prefetchers. Old: 0x%x -> New: 0x%x\n" "$val" "$new_val"
        sudo wrmsr -a "$MSR_ADDR" "$new_val"
        ;;
    enable)
        # Set bits 0-3 to 0 (Enable prefetchers)
        new_val=$((val & ~15))
        printf "Enabling prefetchers. Old: 0x%x -> New: 0x%x\n" "$val" "$new_val"
        sudo wrmsr -a "$MSR_ADDR" "$new_val"
        ;;
    status)
        printf "MSR_1A4=0x%x\n" "$val"
        echo "Prefetcher Status (0=Enabled, 1=Disabled):"
        echo "  [Bit 0] L2 Hardware Prefetcher:       $(((val >> 0) & 1))"
        echo "  [Bit 1] L2 Adjacent Line Prefetcher:  $(((val >> 1) & 1))"
        echo "  [Bit 2] DCU L1 HW Prefetcher:         $(((val >> 2) & 1))"
        echo "  [Bit 3] DCU L1 IP Prefetcher:         $(((val >> 3) & 1))"
        echo ""
        echo "  [Bit 6] (Unknown/Rsrv):               $(((val >> 6) & 1))"
        ;;
    *)
        echo "Usage: $0 [status|enable|disable]"
        exit 1
        ;;
esac
 