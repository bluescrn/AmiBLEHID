
PREFIX=m68k-amigaos-
CC=$(PREFIX)gcc
CFLAGS=-O2 -noixemul
VASM=vasmm68k_mot
INCLUDE=/ndk/include_i/
VASMFLAGS=-quiet -Fhunk -phxass -I $(INCLUDE)

DEPS=

%.o: %.s $(DEPS)
	$(VASM) $(VASMFLAGS) -o $@ $<

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

AmiBLEHIDMouseDriver: mousedriver.o vbrserver.o
	$(CC) $(CFLAGS) -o $@ mousedriver.o vbrserver.o

archive: AmiBLEHIDMouseDriver
	mkdir AmiBLEHIDMouse
	cp -f AmiBLEHIDMouseDriver AmiBLEHIDMouse/
	rm -rf AmiBLEHIDMouse.lha
	lha a AmiBLEHIDMouse.lha AmiBLEHIDMouse
	rm -rf AmiBLEHIDMouse

clean:
	rm -rf *.lha *.o AmiBLEHIDMouseDriver
	rm -rf AmiBLEHIDMouse
