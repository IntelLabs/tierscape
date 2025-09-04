# parl_memory_masim


## Build

````
make
````

## Run

``
./masim configs/<config file>
``

## Coverage analysis
We are going to use the config file ``stairs_1TB_100g`` for this purpose.

``
./masim configs/stairs_1TB_100g
``
We verified using Intel PIN that total memory accesses are 20,971,520,000.

### Explanation on \#of acccess
Our microbenchmark allocates approximately 900 GB of memory. Out of this 900GB, the benchmark sequentially reads 400GB of data in a loop. We set the loop count value to 200, so as the benchmark executes for a minute or so.
The application reads a single byte from each page (instead of reading the whole 4KB page).

Here, total access in one loop is 104,857,600 (#of pages in 400GB). Total number of = 104,857,600*200 (as it executes 200 times) = 20,971,520,000.


## Coverage using PEBS

````
/usr/bin/perf record -d -e cpu/event=0xd0,umask=0x83/ppu -c -- ./masim configs/stairs_1TB_100g

````
The event ''0xd0,umask=0x83/ppu'' corresponds to MEM_INST_RETIRED.ANY on an SPR machine.

### Results for different values of c

|     Value    	|     Events   captured    	|     Coverage    	|     Perf.   Overhead    	|
|---	|---	|---	|---	|
|     10    	|     67,452,719    	|     0.3216%    	|     159.49%    	|
|     100    	|     57,410,586    	|     0.2738%    	|     156.79%    	|
|     1000    	|     58,904,213    	|     0.2809%    	|     160.96%    	|
|     2000    	|     55,973,632    	|     0.2669%    	|     153.08%    	|
|     5000    	|     53,566,036    	|     0.2554%    	|     147.33%    	|
|     10000    	|     40,616,668    	|     0.1937%    	|     139.72%    	|
|     100000    	|     2,939,584    	|     0.0140%    	|     109.96%    	|

