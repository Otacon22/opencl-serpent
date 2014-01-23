#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <CL/cl.h>

#define MEM_SIZE (128)
#define MAX_SOURCE_SIZE (0x100000)

#define BLOCK_SIZE 16

int main()
{
    cl_device_id device_id = NULL;
    cl_context context = NULL;
    cl_command_queue command_queue = NULL;
    cl_mem memobj = NULL;
    cl_mem memobj2 = NULL;
    cl_mem memobj3 = NULL;
    cl_mem memobj4 = NULL;

    cl_program program = NULL;
    cl_kernel kernel = NULL;
    cl_platform_id platform_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret;

    FILE *fp;
    char fileName[] = "./serpent.cl";
    char *source_str;
    size_t source_size;
    size_t log_size;
    char *build_log;

    unsigned char string[16];
    unsigned char correct[16] = {0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84,0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84};
    unsigned char plain[16] = {0xBE,0xB6,0xC0,0x69,0x39,0x38,0x22,0xD3,0xBE,0x73,0xFF,0x30,0x52,0x5E,0xC4,0x3E};
    unsigned char key[16] = {0x2B,0xD6,0x45,0x9F,0x82,0xC5,0xB3,0x00,0x95,0x2C,0x49,0x10,0x48,0x81,0xFF,0x48};
    unsigned char cipher[16] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    unsigned char cipher2[16] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

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

/*
__kernel void serpent_encrypt(__global char *string, char *_key, char *_plaintext, uint32_t ciphertext[4])
*/

    

    /* Create Memory Buffer */
    fprintf(stderr, "[INFO] Creating memory buffer (for string)\n");
    memobj = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        BLOCK_SIZE * sizeof(unsigned char),
        NULL,
        &ret);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Creating memory buffer (for key)\n");
    memobj2 = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        BLOCK_SIZE * sizeof(unsigned char),
        key,
        &ret);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Creating memory buffer (for plaintext)\n");
    memobj3 = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        BLOCK_SIZE * sizeof(unsigned char),
        plain,
        &ret);
    assert(ret == CL_SUCCESS);

    fprintf(stderr, "[INFO] Creating memory buffer (for ciphertext)\n");
    memobj4 = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // Memory in R/W mode
        BLOCK_SIZE * sizeof(unsigned char),
        cipher,
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
        (void *) &memobj); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    /* Set OpenCL Kernel Parameters */
    fprintf(stderr, "[INFO] Setting kernel arguments (1) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        1, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj2); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    /* Set OpenCL Kernel Parameters */
    fprintf(stderr, "[INFO] Setting kernel arguments (2) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        2, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj3); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    /* Set OpenCL Kernel Parameters */
    fprintf(stderr, "[INFO] Setting kernel arguments (3) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        3, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj4); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);


    /* Execute OpenCL Kernel */
    fprintf(stderr, "[INFO] Executing opencl kernel (enqueue task)\n");
    ret = clEnqueueTask(command_queue, kernel, 0, NULL, NULL);
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
        memobj4,
        CL_TRUE,
        3,
        BLOCK_SIZE * sizeof(unsigned char),
        cipher2, //output pointer
        0,
        NULL,
        NULL
        );
    assert(ret == CL_SUCCESS);

    if(memcmp(cipher2,correct,16) == 0)
        printf("OK\n");
    else
        printf("ERROR\n");

    /* Display Result 
    for (k=0;k<16;k++){
        printf("%02hhx", string[k]);
    }
    printf("\n");
    */

//    printf("Version: %c\n", string[0]);

    /* Finalization */
    fprintf(stderr, "[INFO] Flushing and releasing buffers\n");
    ret = clFlush(command_queue);
    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseMemObject(memobj);
    ret = clReleaseMemObject(memobj2);
    ret = clReleaseMemObject(memobj3);
    ret = clReleaseMemObject(memobj4);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);

    free(source_str);

    return 0;
}

