CXX = gcc
CFLAGS = -std=c99 -Wall --pedantic -I/home/otacon22/opencl/NVIDIA_GPU_Computing_SDK/OpenCL/common/inc -L/home/otacon22/opencl/NVIDIA_GPU_Computing_SDK/OpenCL/common/lib/Linux32
LDFLAGS = -lOpenCL 

all:
	$(CXX) $(CFLAGS) -o main main.c $(LDFLAGS)

clean:
	rm -rf main
