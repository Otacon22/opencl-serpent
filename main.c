#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <CL/cl.h>
#include "serpentdefs.h"




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

unsigned char _correct[BLOCK_SIZE_IN_BYTES] = {0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84,0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84};
unsigned char _plain[BLOCK_SIZE_IN_BYTES] = {0xBE,0xB6,0xC0,0x69,0x39,0x38,0x22,0xD3,0xBE,0x73,0xFF,0x30,0x52,0x5E,0xC4,0x3E};
unsigned char _key[BLOCK_SIZE_IN_BYTES] = {0x2B,0xD6,0x45,0x9F,0x82,0xC5,0xB3,0x00,0x95,0x2C,0x49,0x10,0x48,0x81,0xFF,0x48};
unsigned char _cipher[BLOCK_SIZE_IN_BYTES] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

/*
uint32_t correct[TOTAL_BLOCKS_SIZE];
uint32_t plain[TOTAL_BLOCKS_SIZE];
uint32_t key[TOTAL_BLOCKS_SIZE];
uint32_t cipher[TOTAL_BLOCKS_SIZE];
uint32_t cipher2[TOTAL_BLOCKS_SIZE];
*/

uint32_t *correct;
uint32_t *plain;
uint32_t key[BLOCK_SIZE_IN_32BIT_WORDS];
uint32_t *cipher;
uint32_t *cipher2;



uint32_t w[132];


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

cl_uint workDimensions;
size_t global_work_size;
size_t local_work_size[1] = {1};


void check_needed_size(){
    size_t required_on_video_card = (MEM_SIZE * 2)+(132*4);
    size_t required_on_host = required_on_video_card + (MEM_SIZE * 2);

    printf("[INFO] Memory required on video card: %d Bytes (%.2f MiB) \n", required_on_video_card, required_on_video_card/1048576.0);
    if (required_on_video_card >= globalMemorySize){
        printf("[ERROR] Requested memory is larger than the avalable memory on the video card!\n");
        exit(1);
    }
    printf("[INFO] Memory required on the host: %d Bytes (%.2f MiB)\n", required_on_host, required_on_host/1048576.0);
}

void allocate_data_buffers(){
    if ((plain = malloc(MEM_SIZE)) == NULL){
        printf("Memory error\n");
        exit(1);
    }
    if ((correct = malloc(MEM_SIZE)) == NULL){
        printf("Memory error\n");
        exit(1);
    }
    if ((cipher = malloc(MEM_SIZE)) == NULL){
        printf("Memory error\n");
        exit(1);
    }
    if ((cipher2 = malloc(MEM_SIZE)) == NULL){
        printf("Memory error\n");
        exit(1);
    }
}

void replicate_original_data_to_new_buffers(){
    int k;

    memcpy(key, _key, BLOCK_SIZE_IN_BYTES);
    for(k=0; k<(NUM_ENCRYPT_BLOCKS);k++){
        memcpy(&(plain[k*BLOCK_SIZE_IN_32BIT_WORDS]), _plain, BLOCK_SIZE_IN_BYTES);
        memcpy(&(cipher[k*BLOCK_SIZE_IN_32BIT_WORDS]), _cipher, BLOCK_SIZE_IN_BYTES);
        memcpy(&(correct[k*BLOCK_SIZE_IN_32BIT_WORDS]), _correct, BLOCK_SIZE_IN_BYTES);
    }

}

void load_kernel_source_code(){

    if (!(fp = fopen(fileName, "r"))) {
	fprintf(stderr, "[ERR] Failed to load kernel from file %s\n", fileName);
	exit(1);
    } else {
        fprintf(stderr, "[INFO] Kernel file loaded from file %s\n", fileName);
    }
    source_str = (char *) malloc(MAX_SOURCE_SIZE+100); //+100 is for appending DEFINE of num work items

    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
}

void get_and_print_device_info(){
    int i;

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
    
}

void create_opencl_memory_buffers(){

    fprintf(stderr, "[INFO] Creating memory buffer (for key)\n");
    memobj0 = clCreateBuffer(
        context,
        CL_MEM_READ_ONLY, // Memory in Read only mode
        MEM_SIZE_KEY,
        NULL,
        &ret);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Creating memory buffer (for plaintext)\n");
    memobj1 = clCreateBuffer(
        context,
        CL_MEM_READ_ONLY, // Memory in Read only mode
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

}

void pre_compute_key(){
    int i;

    printf("[INFO] Pre-computation of key\n");


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
}


void copy_data_to_opencl_buffers() {

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

    fprintf(stderr, "[INFO] Copying into memory buffer (plain) - Size: %d Bytes \n", MEM_SIZE);
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

    fprintf(stderr, "[INFO] Copying into memory buffer (clean cipher (full of zeroes)) - Size: %d Bytes \n", MEM_SIZE);
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

}

void create_opencl_program_from_source(){
    fprintf(stderr, "[INFO] Creating kernel program from kernel source\n");

    program = clCreateProgramWithSource(context, 1, (const char **) &source_str, (const size_t *) &source_size, &ret);
    assert(ret == CL_SUCCESS);
}


void build_opencl_program(){
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

}

void create_opencl_kernel() {
    fprintf(stderr, "[INFO] Creating OpenCl kernel\n");
    kernel = clCreateKernel(program, "serpent_encrypt", &ret);
    assert(ret == CL_SUCCESS);
}


void set_kernel_parameters() {
    
    fprintf(stderr, "[INFO] Setting kernel arguments (0) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        0, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj0); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Setting kernel arguments (1) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        1, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj1); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Setting kernel arguments (2) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        2, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj2); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

}

void enqueue_opencl_kernel(){

    fprintf(stderr, "[INFO] Executing opencl kernel (enqueue task)\n");
    ret = clEnqueueNDRangeKernel(
        command_queue,
        kernel,
        workDimensions, //Number of dimensions (max = 3)
        NULL,
        &global_work_size, //array che indica il numero di work-items che eseguiranno questo kernel, per ciascuna dimensione dichiarata
        NULL, //Numero di work-items che creano un work-group. Il totale e' calcolato come il prodotto degli elementi dell'array. Il singolo elemento dell'array fa riferimento al numero di work-item che crea un gruppo nella dimensione n. Il numero totale deve essere minore di CL_DEVICE_MAX_WORK_GROUP_SIZE.
//Inoltre i singoli elementi dell'array devono essere minori di CL_DEVICE_MAX_WORK_ITEM_SIZES[0]...[1]... Se e' NULL se ne occupa OpenCL di sceglierlo.
        0,
        NULL,
        &startEvent
    );
    assert(ret == CL_SUCCESS);

}


void wait_opencl_finish_exec(){
    clFinish(command_queue);
}

void copy_results_from_opencl_buffer(){
    /* Copy results from the memory buffer */
    fprintf(stderr, "[INFO] Copying results from memory buffer (ciphertext)\n");
    ret = clEnqueueReadBuffer(
        command_queue,
        memobj2,
        CL_TRUE, //Blocking operation (wait finish)
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

}

void get_opencl_performance_time() {

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

}


void release_opencl_resources() {
    
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
}

void print_perf_speed(){
    float speed = (((float) (BLOCK_SIZE_IN_BYTES*NUM_ENCRYPT_BLOCKS))/executionTime);

    printf("[PERF] Speed: %.2f B/s\n", speed);

    if (speed/1073741824.0 >= 1.0){
        printf("[PERF]     * Conversion: %.2f GiB/s\n", speed/1073741824.0);
    } else if (speed/1048576.0 >= 1.0){ //it's Over 9000!!!!
        printf("[PERF]     * Conversion: %.2f MiB/s\n", speed/1048576.0);
    } else if (speed/1024.0 >= 1.0){
        printf("[PERF]     * Conversion: %.2f kiB/s\n", speed/1024.0);
    } 
}



































int main()
{
    int k;

    /* Print and save some device-specific informations about this video card */
    get_and_print_device_info();

    check_needed_size();

    allocate_data_buffers();

    replicate_original_data_to_new_buffers();

    /* Load the source code containing the kernel */
    load_kernel_source_code();
    
    

    /* Create Memory Buffer */
    create_opencl_memory_buffers();


    /* Pre-computing the Serpent key */
    pre_compute_key();
    

    /* Copy key, clear and ciphertext (zeroes) to opencl memory buffers */
    copy_data_to_opencl_buffers();
    

    /* Create Kernel Program from the source */
    create_opencl_program_from_source();    

    /* Build Kernel Program */
    build_opencl_program();

    /* Create OpenCL Kernel */
    create_opencl_kernel();

    /* Set OpenCL Kernel Parameters */
    set_kernel_parameters();

    /* Execute OpenCL Kernel */
    workDimensions      = 1;
    global_work_size    = NUM_WORK_ITEMS;
    //local_work_size[1]  = 1; //????
    //local_work_size = NULL;
    enqueue_opencl_kernel();

    /* Wait for gracefully termination of running operations */
    wait_opencl_finish_exec();

    /* Copy results from output buffer (ciphertext) */
    copy_results_from_opencl_buffer();

    if(memcmp(cipher2, correct, BLOCK_SIZE_IN_BYTES*NUM_ENCRYPT_BLOCKS) == 0)
        printf("\n[SUCCESS] Output ciphertext is correct!\n\n");
    else {
        /* Check if ciphertext is correct */
        for(k=0; k<(NUM_ENCRYPT_BLOCKS); k++){
            if(!(memcmp(
                    &(cipher2[k*BLOCK_SIZE_IN_32BIT_WORDS]),
                    &(correct[k*BLOCK_SIZE_IN_32BIT_WORDS]),
                    BLOCK_SIZE_IN_BYTES
                    ) == 0)){
                printf("\n[ERROR] Output ciphertext is not correct! (block number %d, couting from 0)\n\n", k);
                exit(1); 
            }
        }
        printf("[CHECK] Strange behaviour detected. Check me\n");
    }




    /* Print result 
    printf("[OUTPUT] Result: ");
 
    for (k=BLOCK_SIZE_IN_BYTES*9; k < BLOCK_SIZE_IN_BYTES*12; k++){
        printf("%02hhx", ((char *) cipher2)[k]);
    }
    printf("\n");*/

    get_opencl_performance_time();
    
    printf("[PERF] Start time: %lu\n", (long unsigned int) startTime);
    printf("[PERF] End time  : %lu\n", (long unsigned int) endTime);

    executionTime = ((float) (endTime-startTime))*0.000000001;

    printf("[PERF] Execution time: %e\n", executionTime);

    printf("[PERF] Encrypted data: %d Bytes\n", BLOCK_SIZE_IN_BYTES*NUM_ENCRYPT_BLOCKS);

    /* Print calculated speed */
    print_perf_speed();

    /* Release buffers and stuff */
    release_opencl_resources();

    return 0;
}

