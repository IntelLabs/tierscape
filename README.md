# Tierscape EuroSys 2026
The repo contains artifact for the paper "TierScape: Harnessing Multiple Compressed Tiers to Tame Server Memory TCO" pubished at EuroSys 2026.


## General instructions
To evaluate the code can be evaluated with:
 - Multiple byte-addressable tiers with multiple compressed tiers, or
 - Multiple compressed tiers

The example used to demonstrate is Redis.


## Tierscape configuration

We show the usecase with 2 compressed tiers and 2 byte-addressable tiers.  
The two byte-addressable tiers show up as different NUMA nodes and can be checked using ``numactl -H`` command.   
For the multiple byte-addressable tiers, we need to use the support present in the kernel patch.


## Steps
1. Install the kernel present in the ``linux_patch`` dir. See the README file inside the dir for installation instructions.
2. Get the Tierscape userspace scripts.
3. Install Memcached.
4. Execute the test cases.
5. View the plots.


# il-opensource-template
![GitHub License](https://img.shields.io/github/license/IntelLabs/il-opensource-template)
[![OpenSSF Scorecard](https://api.scorecard.dev/projects/github.com/IntelLabs/il-opensource-template/badge)](https://scorecard.dev/viewer/?uri=github.com/IntelLabs/il-opensource-template)

