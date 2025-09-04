**This file is a patch to the Linux kernel and is therefore licensed under the terms of the GNU General Public License version 2.**

## Instructions to apply the path and build kernel

### apply patch
The pathc applies to commit f443e374ae131c168a065ea1748feac6b2e76613 (tag: v5.17)

```
git clone https://github.com/torvalds/linux.git --depth 3 --branch v5.17 linux_5.17
cd linux_5.17
git am <path>/0001-tierscape-eurosys26.patch
git log
# should show tierscape ccommit  tierscape eurosys26
```

### Build
```
cp tierscape_config .config
make -j
# select default values
sudo make modules_install -j
sudo make install
```

Boot into the kernel.
