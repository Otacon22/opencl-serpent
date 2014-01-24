#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <CL/cl.h>

#define MAX_SOURCE_SIZE (0x100000)

typedef unsigned int    uint32_t;

typedef unsigned char   BYTE;

#define BLOCK_SIZE 16

#define MEM_SIZE  4*sizeof(uint32_t)
#define MEM_SIZE_KEY 132*sizeof(uint32_t)

int main()
{
    cl_device_id device_id = NULL;
    cl_context context = NULL;
    cl_command_queue command_queue = NULL;
    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_platform_id platform_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;

    cl_mem memobj0 = NULL;
    cl_mem memobj1 = NULL;
    cl_mem memobj2 = NULL;
    
    //size_t workGroupSize;

    FILE *fp;
    char fileName[] = "./serpent.cl";
//    char fileName[] = "./check.cl";
    char *source_str;
    size_t source_size;
    size_t log_size;
    char *build_log;

    unsigned char correct[BLOCK_SIZE] = {0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84,0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84};
    unsigned char _plain[BLOCK_SIZE] = {0xBE,0xB6,0xC0,0x69,0x39,0x38,0x22,0xD3,0xBE,0x73,0xFF,0x30,0x52,0x5E,0xC4,0x3E};
    unsigned char _key[BLOCK_SIZE] = {0x2B,0xD6,0x45,0x9F,0x82,0xC5,0xB3,0x00,0x95,0x2C,0x49,0x10,0x48,0x81,0xFF,0x48};
    unsigned char _cipher[BLOCK_SIZE] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

    //uint32_t correct[4];
    uint32_t plain[4];
    uint32_t key[4];
    uint32_t cipher[4];
    uint32_t cipher2[4];

    uint32_t w[132];
    int i;

    cl_ulong localMemorySize;
    cl_ulong globalMemorySize;
    cl_ulong globalCacheSize;
    cl_uint  computationUnits;
    char deviceVendor[200];
    cl_uint  maxWorkItemDim;
    size_t   maxWorkItem[10];
    cl_uint  numberOfBits;
    size_t   maxWorkGroupSize;

    cl_ulong startTime, endTime;
    float executionTime;
    cl_event startEvent;

    size_t global_work_size[1] = {1};
    size_t local_work_size[1] = {1};

    int k;


    memcpy(plain, _plain, 16);
    memcpy(key, _key, 16);
    memcpy(cipher, _cipher, 16);


    /* Load the source code containing the kernel */
    if (!(fp = fopen(fileName, "r"))) {
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
    command_queue = clCreateCommandQueue(context, device_id,
        CL_QUEUE_PROFILING_ENABLE, // Enable profiling
        &ret);
    assert(ret == CL_SUCCESS);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_VENDOR, sizeof(deviceVendor), deviceVendor, 0);    
    assert(ret == CL_SUCCESS);
    printf("[INFO] Device vendor: %s\n", deviceVendor);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_ADDRESS_BITS, sizeof(numberOfBits), &numberOfBits, 0);    
    assert(ret == CL_SUCCESS);
    printf("[INFO] Device address bits: %u\n", numberOfBits);

    /* Print Local memory size */
    ret = clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &localMemorySize, 0);    
    assert(ret == CL_SUCCESS);
    printf("[INFO] Local Memory size: %lu Bytes\n", (long unsigned int) localMemorySize);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &globalMemorySize, 0);
    assert(ret == CL_SUCCESS);
    printf("[INFO] Global Memory size: %lu Bytes\n", (long unsigned int) globalMemorySize);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(cl_ulong), &globalCacheSize, 0);
    assert(ret == CL_SUCCESS);
    printf("[INFO] Global Cache size: %lu Bytes\n", (long unsigned int) globalCacheSize);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &computationUnits, 0);
    assert(ret == CL_SUCCESS);
    printf("[INFO] Total number of parallel compute cores: %u \n", (unsigned int) computationUnits);

    ret = clGetDeviceInfo(device_id,  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &maxWorkItemDim, 0);
    assert(ret == CL_SUCCESS);
    printf("[INFO] Max work item dimensions: %u \n", (unsigned int) maxWorkItemDim);

    ret = clGetDeviceInfo(device_id,  CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(maxWorkItem), maxWorkItem, 0);
    assert(ret == CL_SUCCESS);
    for(i=1; i<=maxWorkItemDim; i++){
        printf("[INFO] Max number of work items for dimension %d: %u \n", i, maxWorkItem[i]);
    }

    ret = clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, 0);
    assert(ret == CL_SUCCESS);
    printf("[INFO] Max work group size: %u \n", (unsigned int) maxWorkGroupSize);
    

    /* Create Memory Buffer */

    fprintf(stderr, "[INFO] Creating memory buffer (for key)\n");
    memobj0 = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        MEM_SIZE_KEY,
        NULL,
        &ret);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Creating memory buffer (for plaintext)\n");
    memobj1 = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        MEM_SIZE,
        NULL,
        &ret);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Creating memory buffer (for ciphertext)\n");
    memobj2 = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        MEM_SIZE,
        NULL,
        &ret);
    assert(ret == CL_SUCCESS);

/* Pre-computing the key roll */

    printf("[INFO] Pre-computation of key\n");


#define keyLen 128
#define PHI 0x9e3779b9L
#define ROL(x,n) ((((uint32_t)(x))<<(n))| \
                  (((uint32_t)(x))>>(32-(n))))

#define INIT  w[i]=key[i]; i++

    i=0;

    INIT;INIT;INIT;INIT;       
    w[i]=(key[i]&((1L<<((keyLen&31)))-1))|(1L<<((keyLen&31)));
    i++;

#define SETZERO  w[i]=0;i++

    SETZERO; SETZERO;SETZERO; SETZERO;
    i=8;

#define ITER_1    w[i]=ROL(w[i-8]^w[i-5]^w[i-3]^w[i-1]^PHI^(i-8),11);i++

    ITER_1;ITER_1;ITER_1;ITER_1;ITER_1;ITER_1;ITER_1;ITER_1;

#define ITER_2    w[i]=w[i+8];i++    

    i=0;
    ITER_2;ITER_2;ITER_2;ITER_2;ITER_2;ITER_2;ITER_2;ITER_2;
    i=8;

#define ITER_3    w[i]=ROL(w[i-8]^w[i-5]^w[i-3]^w[i-1]^PHI^i,11); i++
#define ITER_3_10times ITER_3;ITER_3;ITER_3;ITER_3;ITER_3;ITER_3;ITER_3;ITER_3;ITER_3;ITER_3
#define ITER_3_30times ITER_3_10times;ITER_3_10times;ITER_3_10times

    ITER_3_30times;
    ITER_3_30times;
    ITER_3_30times;
    ITER_3_30times;
    ITER_3;ITER_3;ITER_3;ITER_3;

    printf("[INFO] Pre-computation of key completed\n");
    




    fprintf(stderr, "[INFO] Copying into memory buffer (key)\n");
    ret = clEnqueueWriteBuffer(
        command_queue,
        memobj0,
        CL_TRUE,
        0,
        MEM_SIZE_KEY,
        w,
        0,
        NULL,
        NULL);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Copying into memory buffer (plain)\n");
    ret = clEnqueueWriteBuffer(
        command_queue,
        memobj1,
        CL_TRUE,
        0,
        MEM_SIZE,
        plain,
        0,
        NULL,
        NULL);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Copying into memory buffer (clean cipher)\n");
    ret = clEnqueueWriteBuffer(
        command_queue,
        memobj2,
        CL_TRUE,
        0,
        MEM_SIZE,
        cipher,
        0,
        NULL,
        NULL);
    assert(ret == CL_SUCCESS);



    /* Create Kernel Program from the source */
    fprintf(stderr, "[INFO] Creating kernel program from kernel source\n");
    program = clCreateProgramWithSource(context, 1, (const char **) &source_str, (const size_t *) &source_size, &ret);
    assert(ret == CL_SUCCESS);

    /* Build Kernel Program */
    fprintf(stderr, "[INFO] Building kernel program\n");
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    if (ret != CL_SUCCESS){

        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        if((build_log = malloc(sizeof(char)*log_size)) == NULL){
            printf("Memory error\n");
            exit(1);
        }
    
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, build_log, NULL);
        build_log[log_size]='\0';
        printf("[-- BUILD LOG START --] \n%s\n[-- BUILD LOG END --]\n", build_log);

        free(build_log);
        exit(1);
    }

    /* Create OpenCL Kernel */
    fprintf(stderr, "[INFO] Creating OpenCl kernel\n");
    kernel = clCreateKernel(program, "serpent_encrypt", &ret);
    assert(ret == CL_SUCCESS);

/*
__kernel void serpent_encrypt(__global char *string, char *_key, char *_plaintext, uint32_t ciphertext[4])
*/

    /* Set OpenCL Kernel Parameters */
    fprintf(stderr, "[INFO] Setting kernel arguments (0) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        0, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj0); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    /* Set OpenCL Kernel Parameters */
    fprintf(stderr, "[INFO] Setting kernel arguments (1) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        1, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj1); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    /* Set OpenCL Kernel Parameters */
    fprintf(stderr, "[INFO] Setting kernel arguments (2) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        2, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj2); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);


    /* Execute OpenCL Kernel */
    global_work_size[1] = 1;
    local_work_size[1] = 1;
    fprintf(stderr, "[INFO] Executing opencl kernel (enqueue task)\n");
    ret = clEnqueueNDRangeKernel(command_queue,
        kernel,
        1, //Number of dimensions (max = 3)
        NULL,
        global_work_size, //array che indica il numero di work-items che eseguiranno questo kernel, per ciascuna dimensione dichiarata
        local_work_size, //Numero di work-items che creano un work-group
        0,
        NULL,
        &startEvent
    );
    assert(ret == CL_SUCCESS);


    /* Copy results from the memory buffer */
/*    fprintf(stderr, "[INFO] Copying results from memory buffer (string)\n");
    ret = clEnqueueReadBuffer(
        command_queue,
        memobj, 
        CL_TRUE,
        0,
        BLOCK_SIZE * sizeof(unsigned char),
        string, //output pointer
        0,
        NULL,
        NULL
        );
    assert(ret == CL_SUCCESS);*/

    /* Copy results from the memory buffer */
    fprintf(stderr, "[INFO] Copying results from memory buffer (ciphertext)\n");
    ret = clEnqueueReadBuffer(
        command_queue,
        memobj2,
        CL_TRUE,
        0,
        MEM_SIZE,
        cipher2, //output pointer
        0,
        NULL,
        NULL
        );
    if(ret != CL_SUCCESS){
        if(ret == CL_INVALID_COMMAND_QUEUE) printf("Invalid command queue\n");
        else if(ret == CL_INVALID_CONTEXT) printf("Invalid context\n");
        else if(ret == CL_INVALID_VALUE) printf("Invalid value\n");
        else if(ret == CL_INVALID_EVENT_WAIT_LIST) printf("Invalid event wait list\n");
        else if(ret == CL_OUT_OF_HOST_MEMORY) printf("Out of host memory\n");
        else printf("Unknown error: %d\n", ret);
    }
    assert(ret == CL_SUCCESS);

    if(memcmp(cipher2,correct,16) == 0)
        printf("[SUCCESS] Output ciphertext is correct!\n");
    else
        printf("[ERROR] Output ciphertext is not correct\n");

    printf("[OUTPUT] Result: ");
    //Display Result 
    for (k=0; k < BLOCK_SIZE; k++){
        printf("%02hhx", cipher2[k]);
    }
    printf("\n");

    printf("[INFO] Getting start time\n");
    ret = clGetEventProfilingInfo(
        startEvent,                              // the event object to get info for
        CL_PROFILING_COMMAND_START,         // the profiling data to query
        sizeof(cl_ulong),                   // the size of memory pointed by param_value
        &startTime,                          // pointer to memory in which the query result is returned
        NULL                    // actual number of bytes copied to param_value
    );
    assert(ret == CL_SUCCESS);

    printf("[INFO] Getting end time\n");
    ret = clGetEventProfilingInfo(
        startEvent,                              // the event object to get info for
        CL_PROFILING_COMMAND_END,         // the profiling data to query
        sizeof(cl_ulong),                   // the size of memory pointed by param_value
        &endTime,                          // pointer to memory in which the query result is returned
        NULL                    // actual number of bytes copied to param_value
    );
    assert(ret == CL_SUCCESS);

    
    printf("[PERF] Start time: %lu\n", (long unsigned int) startTime);
    printf("[PERF] End time  : %lu\n", (long unsigned int) endTime);

    executionTime = ((float) (endTime-startTime))*0.000000001;

    printf("[PERF] Execution time: %e\n", executionTime);

    /* Finalization */
    fprintf(stderr, "[INFO] Flushing and releasing buffers\n");
    ret = clFlush(command_queue);
    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseMemObject(memobj0);
    ret = clReleaseMemObject(memobj1);
    ret = clReleaseMemObject(memobj2);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);

    free(source_str);

    return 0;
}

