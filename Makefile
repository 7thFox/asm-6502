

build: bin/
	gcc src/*.c -o bin/asm-6502

build-debug: bin/
	gcc src/*.c -D DEBUG -o bin/asm-6502-debug

build-trace: bin/
	gcc src/*.c -D TRACE -D DEBUG -o bin/asm-6502-trace

bin/:
	mkdir bin

run-example: build
	bin/asm-6502 ./example.asm6502

debug-example: build-debug
	bin/asm-6502-debug ./example.asm6502

trace-example: build-trace
	bin/asm-6502-trace ./example.asm6502

