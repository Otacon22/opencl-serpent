#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <CL/cl.h>

#define MEM_SIZE (128)
#define MAX_SOURCE_SIZE (0x100000)

int main()
{
    cl_device_id device_id = NULL;
    cl_context context = NULL;
    cl_command_queue command_queue = NULL;
    cl_mem memobj = NULL;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_platform_id platform_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;

    // serpent_encrypt(__global char *string, char *_key, char *_plaintext, char *_output)
    char string[MEM_SIZE];
    char key[MEM_SIZE];
    char plaintext[MEM_SIZE];
    char output[MEM_SIZE];

    FILE *fp;
    char fileName[] = "./serpent.cl";
    char *source_str;
    size_t source_size;
    size_t log_size;
    char *build_log;

    int k;

    /* Load the source code containing the kernel */
    fp = fopen(fileName, "r");
    if (!fp) {
	fprintf(stderr, "[ERR] Failed to load kernel from file %s\n", fileName);
	exit(1);
    } else {
        fprintf(stderr, "[INFO] Kernel file loaded from file %s\n", fileName);
    }
    source_str = (char *) malloc(MAX_SOURCE_SIZE);
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);

    /* Get Platform and Device Info */
    fprintf(stderr, "[INFO] Getting platform ID\n");
    ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Getting device ID and info\n");
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id,
		       &ret_num_devices);
    assert(ret == CL_SUCCESS);

    /* Create OpenCL context */
    fprintf(stderr, "[INFO] Creating OpenCL context\n");
    context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    assert(ret == CL_SUCCESS);

    /* Create Command Queue */
    fprintf(stderr, "[INFO] Creating command queue\n");
    command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
    assert(ret == CL_SUCCESS);

    /* Create Memory Buffer */
    fprintf(stderr, "[INFO] Creating memory buffer\n");
    memobj = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        MEM_SIZE * sizeof(char),
        NULL,
        &ret);
    assert(ret == CL_SUCCESS);

    /* Create Kernel Program from the source */
    fprintf(stderr, "[INFO] Creating kernel program from kernel source\n");
    program =
	clCreateProgramWithSource(context, 1, (const char **) &source_str,
				  (const size_t *) &source_size, &ret);
    assert(ret == CL_SUCCESS);

    /* Build Kernel Program */
    fprintf(stderr, "[INFO] Building kernel program\n");
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    assert(ret == CL_SUCCESS);

    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
    if((build_log = malloc(sizeof(char)*log_size)) == NULL){
        printf("Memory error\n");
        exit(1);
    }
    
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
    build_log[log_size]='\0';
    printf("[-- BUILD LOG START --] \n%s\n[-- BUILD LOG END --]\n", build_log);
    free(build_log);

    /* Create OpenCL Kernel */
    fprintf(stderr, "[INFO] Creating OpenCl kernel\n");
    kernel = clCreateKernel(program, "serpent_encrypt", &ret);
    assert(ret == CL_SUCCESS);

    /* Set OpenCL Kernel Parameters */
    fprintf(stderr, "[INFO] Setting kernel arguments\n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        0, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    /* Execute OpenCL Kernel */
    fprintf(stderr, "[INFO] Executing opencl kernel (enqueue task)\n");
    ret = clEnqueueTask(command_queue, kernel, 0, NULL, NULL);
    assert(ret == CL_SUCCESS);

    /* Copy results from the memory buffer */
    fprintf(stderr, "[INFO] Copying results from memory buffer\n");
    ret = clEnqueueReadBuffer(
        command_queue,
        memobj, 
        CL_TRUE,
        0,
        MEM_SIZE * sizeof(char),
        string, //output pointer
        0,
        NULL,
        NULL
        );
    assert(ret == CL_SUCCESS);

    /* Display Result 
    for (k=0;k<16;k++){
        printf("%02hhx", string[k]);
    }
    printf("\n");
    */

    printf("Version: %c\n", string[0]);

    /* Finalization */
    fprintf(stderr, "[INFO] Flushing and releasing buffers\n");
    ret = clFlush(command_queue);
    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseMemObject(memobj);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);

    free(source_str);

    return 0;
}

