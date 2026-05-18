.PHONY: all clean

all:
	@mkdir -p src/build
	@cd src/build && cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null 2>&1 && make -j$$(nproc)

debug:
	@mkdir -p src/build
	@cd src/build && cmake .. -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1 && make -j$$(nproc)

clean:
	@rm -rf src/build
