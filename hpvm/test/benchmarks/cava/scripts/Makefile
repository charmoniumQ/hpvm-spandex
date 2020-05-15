PROGRAM = gamut_map
LIBRARY = cmodule
IMAGE = reference.png
IMAGE_NAME = $(basename $(IMAGE))

CXX  = g++
CFLAGS  = -c -fPIC -O2

# set local paths below
IFLAGS  = -I/usr/include/python2.7/ -I/path/to/local/numpy/include

all: ${PROGRAM}

${PROGRAM}: ${PROGRAM}.cc
	swig -python -o ${PROGRAM}_wrap.cc ${PROGRAM}.i
	$(CXX) ${CFLAGS} ${PROGRAM}.cc -o ${PROGRAM}.o
	$(CXX) ${CFLAGS} ${PROGRAM}_wrap.cc -o ${PROGRAM}_wrap.o ${IFLAGS}
	$(CXX) -shared -o _${LIBRARY}.so *.o

run:
	./convert_image.py --backward $(IMAGE)
	./convert_image.py --raw raw_$(IMAGE)
	../build/cam-vision-native raw_$(IMAGE_NAME).bin result.bin test.conf
	./convert_image.py --binary result.bin

clean:
	rm -rf *.o *.mod *.so ${PROGRAM}_wrap.c __pycache__
