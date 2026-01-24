slim:
	gcc -Os -s -static-libgcc -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -Wl,--gc-sections -Wl,-z,norelro edie.c -o ei
