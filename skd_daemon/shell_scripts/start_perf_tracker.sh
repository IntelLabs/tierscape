PID=$(pidof masim)
FREQ=1000

${PERF_BIN} record -d -e cpu/event=0xd0,umask=0x81/ppu -e cpu/event=0xd0,umask=0x82/ppu -F {FREQ} --intr-regs=SI,DI,BP,SP,R8 -p ${PID} -o - | ${PERF_BIN} script -F trace: -F addr -i - | /data/sandeep/git_repo/masim/masim_src/sk_daemon/tracker_d.o ${PID}
