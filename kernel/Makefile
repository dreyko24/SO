CC      = gcc
KFLAGS  = -m32 -std=gnu11 -O2 -ffreestanding -fno-pie -fno-stack-protector -fno-asynchronous-unwind-tables -fno-builtin -fno-tree-loop-distribute-patterns -mno-sse -mno-mmx -Wall -Wextra -nostdlib
LDFLAGS = -m32 -T linker.ld -nostdlib -static -Wl,-z,noexecstack -Wl,--build-id=none -Wl,-m,elf_i386
ULDFLAGS= -m32 -T user/user.ld -nostdlib -static -Wl,-z,noexecstack -Wl,--build-id=none -Wl,-m,elf_i386 -Wl,-z,max-page-size=4096 -Wl,-z,noseparate-code
PROGS   = utest crash spin ush forktest
UELF    = $(PROGS:%=user/%.elf)
OBJS    = kernel.o gfx.o fs.o html.o gui.o net.o disk.o kbd.o image.o usb.o zip.o media_logo.o blob.o

# vortex-kernel.elf      : pide modo grafico 1024x768x32 a GRUB (se usa en la ISO)
# vortex-kernel-text.elf : sin peticion de video (para QEMU -kernel y las autopruebas)
all: vortex-kernel.elf vortex-kernel-text.elf

vortex-kernel.elf: boot.o $(OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ boot.o $(OBJS)
vortex-kernel-text.elf: boot-text.o $(OBJS) linker.ld
	$(CC) $(LDFLAGS) -o $@ boot-text.o $(OBJS)
boot.o: boot.S
	$(CC) -m32 -c $< -o $@
boot-text.o: boot.S
	$(CC) -m32 -DNOVIDEO -c $< -o $@
%.o: %.c vortex.h gfx.h gui.h fs.h html.h net.h disk.h net_pcnet_virtio.h
	$(CC) $(KFLAGS) -c $< -o $@
media_logo.o: media_logo.S ../docs/vortex-logo.png
	$(CC) -m32 -c $< -o $@
blob.o: blob.S $(UELF)
	$(CC) -m32 -c blob.S -o $@
user/%.elf: user/%.c user/ulib.h user/user.ld
	$(CC) $(KFLAGS) $(ULDFLAGS) -o $@ $<

run: vortex-kernel-text.elf
	qemu-system-i386 -kernel vortex-kernel-text.elf -serial stdio -m 256M
test: vortex-kernel-text.elf
	sh test.sh
hosttest: $(UELF)
	python3 hosttest.py
hostgui:
	python3 ../tools/hostgui.py
clean:
	rm -f *.o vortex-kernel.elf vortex-kernel-text.elf user/*.elf
