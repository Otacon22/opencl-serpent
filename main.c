
#define _BSD_SOURCE 2

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <CL/cl.h>
#include <stdint.h>
#include <getopt.h>


#define MAX_SOURCE_SIZE (0x100000)

#define MAX_BUILD_ARG_LENGTH 500

#define BLOCK_SIZE_IN_BYTES 16
#define BLOCK_SIZE_IN_32BIT_WORDS 4


typedef unsigned char   BYTE;


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

//size_t workGroupSize;

FILE *fp;
char fileName[] = "./serpent.cl";
//    char fileName[] = "./check.cl";

char *binary_dump_file = "binary-opencl.dump";

char *source_str;
size_t source_size = 0;
size_t log_size;
char *build_log;
char *build_args;
char *default_build_args = "-Werror";

int verbose = 0;
int csv_output = 0;

/* Optarg stuff */
static const struct option long_options[] = {
    { "help",         no_argument,         NULL, 'h' },
    { "verbose",        no_argument,         NULL, 'v' },
    { "num-work-items",        required_argument,        NULL, 'w' },
    { "num-blocks-for-work-item",        required_argument,        NULL, 'b' },
    { "csv-output",        no_argument,        NULL, 'c' },
    { "dump-binary-file",   required_argument,  NULL, 'd' },
    { "num-work-items-per-work-group",  required_argument, NULL, 'g'},
    { "unroll-main-work-cycle",        no_argument,        NULL, 'u' },
    { "do-not-unroll-subkey-operation",        no_argument,        NULL, 's' },
    { "ctr-mode", no_argument, NULL, 'r'},
    { 0, 0, 0, 0 }
};
char *args_string = "hvcw:b:d:g:us";
int option_index = 0;

float speed;


unsigned char _correct[BLOCK_SIZE_IN_BYTES] = {0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84,0xEA,0x02,0x47,0x14,0xAD,0x5C,0x4D,0x84};
unsigned char _plain[BLOCK_SIZE_IN_BYTES] = {0xBE,0xB6,0xC0,0x69,0x39,0x38,0x22,0xD3,0xBE,0x73,0xFF,0x30,0x52,0x5E,0xC4,0x3E};
unsigned char _key[BLOCK_SIZE_IN_BYTES] = {0x2B,0xD6,0x45,0x9F,0x82,0xC5,0xB3,0x00,0x95,0x2C,0x49,0x10,0x48,0x81,0xFF,0x48};
unsigned char _cipher[BLOCK_SIZE_IN_BYTES] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};


uint32_t *correct;
uint32_t *plain;
uint32_t key[BLOCK_SIZE_IN_32BIT_WORDS];
uint32_t *cipher;


uint32_t w[132];


cl_ulong localMemorySize;
cl_ulong globalMemorySize;
cl_ulong globalCacheSize;
cl_uint  computationUnits;
char deviceVendor[200];
cl_uint  maxWorkItemDim;
size_t   *maxWorkItem;
cl_uint  numberOfBits;
size_t   maxWorkGroupSize;

cl_ulong startTime, endTime;
float executionTime;
cl_event startEvent;

cl_uint workDimensions;
size_t global_work_size;
size_t local_work_size;

int unroll_main_work_cycle;
int unroll_subkey_operation;
int counter_mode;
uint32_t iv[4];

uint32_t keyLen = 128;

size_t num_work_items;
size_t num_work_items_in_work_group;
size_t num_encrypt_blocks_for_work_item;

size_t num_encrypt_blocks;

size_t total_blocks_size;   //4 32-bit words is the normal block. We want 10*32 blocks.
size_t mem_size;
size_t mem_size_key;

size_t program_size;
char **program_binary;

size_t num_instructions = 0;
float instructions_per_byte;


void verbose_printf(char *str){
    if(verbose){
        printf("%s", str);
    }
}

void csv_header_print(){
    printf("num_work_items,num_work_items_in_work_group,num_encrypt_blocks_for_work_item,total_blocks_size,instructions_per_byte,execution_time,encrypted_data,speed_byte_per_sec\n");
}

void print_usage(char **argv){
    printf("OpenCL serpent\nUsage: %s [-options]\n\n", argv[0]);
    printf("\t-h --help\t\t\t\t\t: Print this usage\n");
    printf("\t-v --verbose\t\t\t\t\t: Verbose version\n");
    printf("\t-d <file> --dump-binary-file <file>\t\t: Specify where to save the generated kernel binary code\n");
    printf("\t-w NUM --num-work-items NUM\t\t\t: Specify number of work-items (default: 2048)\n");
    printf("\t-g NUM --num-work-items-per-work-group NUM\t: Specify number of work-items that compose a work-group (default 64)\n");
    printf("\t-b NUM --num-blocks-for-work-item NUM\t\t: Specify number of blocks encrypted by each work-item (default: 20000)\n");
    printf("\t-u --unroll-main-work-cycle\t\t\t: Unroll 10 iterations of the main kernel cycle\n");
    printf("\t-s --do-not-unroll-subkey-operation\t\t: Do not unroll subkey operation\n");
    printf("\t-r --ctr-mode\t\t\t\t\t: Enable Counter Mode (default: ECB mode)\n");
    printf("\t-c --csv-output\t\t\t\t\t: Print CSV-like output on stdout\n");
    printf("\n");
    exit(0);
}

void parse_arguments(int argc, char **argv){
    int c;

    do {
        c = getopt_long(argc, argv, args_string, long_options, &option_index);
        switch(c) {
            case 'h':
                print_usage(argv);
                break;
            case 'v':
                verbose = 1;
                break;
            case 'w':
                num_work_items = atoi(optarg);
                break;
            case 'b':
                num_encrypt_blocks_for_work_item = atoi(optarg);
                break;
            case 'c':
                csv_output = 1;
                break;
            case 'd':
                binary_dump_file = optarg;
                break;
            case 'g':
                num_work_items_in_work_group = atoi(optarg);
                break;
            case 'u':
                unroll_main_work_cycle = 1;
                break;
            case 's':
                unroll_subkey_operation = 0;
                break;
            case 'r':
                counter_mode = 1;
                break;
        }
    } while (c != -1);

}


void experiment_size_default_declarations(){
    num_work_items = 2048;
    num_work_items_in_work_group = 64;
    num_encrypt_blocks_for_work_item = 20000;
    unroll_main_work_cycle = 0;
    unroll_subkey_operation = 1;
    counter_mode = 0;
}

void calculate_experiment_parameters(){
    num_encrypt_blocks = num_work_items * num_encrypt_blocks_for_work_item;
    total_blocks_size = BLOCK_SIZE_IN_32BIT_WORDS * num_encrypt_blocks;
    mem_size = sizeof(uint32_t) * total_blocks_size;
    mem_size_key = 132*sizeof(uint32_t);
}

void csv_print_experiment_size_parameters(){
    printf("%lu,%lu,%lu,%lu", num_work_items, num_work_items_in_work_group, num_encrypt_blocks_for_work_item, total_blocks_size);
}

void print_experiment_size_parameters(){
    if (!csv_output){
        printf("\n---------------- Experiment parameters ----------------\n\n");

        printf("[INFO] Key size: %d bit\n", keyLen);
        printf("[INFO] Block size: %d Byte\n", BLOCK_SIZE_IN_BYTES);
        printf("[INFO] Number of work-items: %lu\n", num_work_items);
        printf("[INFO] Number of work-items in a work-group: %lu\n", num_work_items_in_work_group);
        printf("[INFO] Resulting number of work-groups: %.2f\n", ((float) num_work_items)/num_work_items_in_work_group);
        printf("[INFO] Number of blocks to encrypt for work item: %lu\n", num_encrypt_blocks_for_work_item);
        if (verbose) printf("[INFO] Total number of blocks to encrypt with given parameters: %lu blocks\n", num_encrypt_blocks);
    }
}


void check_needed_size(){
    size_t required_on_video_card = (mem_size) + mem_size_key;
    size_t required_on_host = required_on_video_card + (mem_size * 2);

    if(verbose){
        printf("[INFO] Memory required on video card: %lu Bytes (%.2f MiB) \n", required_on_video_card, required_on_video_card/1048576.0);
    }
    if (required_on_video_card >= globalMemorySize){
        printf("[ERROR] Requested memory is larger than the avalable memory on the video card!\n");
        exit(1);
    }
    if(verbose){
        printf("[INFO] Memory occupancy on video card: %.2f %%\n",(((float) required_on_video_card)/(globalMemorySize))*100.0);
        printf("[INFO] Memory required on the host: %ld Bytes (%.2f MiB)\n", required_on_host, required_on_host/1048576.0);
    }

}

void allocate_data_buffers(){
    verbose_printf("[INFO] Allocation of buffers on host memory");
    if ((plain = malloc(mem_size)) == NULL){
        printf("Memory error\n");
        exit(1);
    }
    if ((correct = malloc(mem_size)) == NULL){
        printf("Memory error\n");
        exit(1);
    }
}

void replicate_original_data_to_new_buffers(){
    int k;

    verbose_printf("Replicating the original data over all the generated buffers\n");
    memcpy(key, _key, BLOCK_SIZE_IN_BYTES);
    for(k=0; k<(num_encrypt_blocks);k++){
        memcpy(&(plain[k*BLOCK_SIZE_IN_32BIT_WORDS]), _plain, BLOCK_SIZE_IN_BYTES);
        //memcpy(&(cipher[k*BLOCK_SIZE_IN_32BIT_WORDS]), _cipher, BLOCK_SIZE_IN_BYTES); // Not used anymore
        memcpy(&(correct[k*BLOCK_SIZE_IN_32BIT_WORDS]), _correct, BLOCK_SIZE_IN_BYTES);
    }

}

void load_kernel_source_code(){

    if (!(fp = fopen(fileName, "r"))) {
	    printf("[ERR] Failed to load kernel from file %s\n", fileName);
	    exit(1);
    } else {
        if(verbose){printf( "[INFO] Kernel file loaded from file %s\n", fileName);}
    }
    if ((source_str = (char *) malloc(MAX_SOURCE_SIZE)) == NULL) {
        printf("Memory error\n");
        exit(1);
    }

    source_size += fread(source_str, 1, MAX_SOURCE_SIZE, fp);

    fclose(fp);

}

void get_and_print_device_info(){

    int i;

    verbose_printf( "\n---------------- Device Informations ----------------\n\n");

    /* Get Platform and Device Info */
    verbose_printf( "[INFO] Getting platform ID\n");
    ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    assert(ret == CL_SUCCESS);

    verbose_printf( "[INFO] Getting device ID and info\n");
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id,
		       &ret_num_devices);
    assert(ret == CL_SUCCESS);

    /* Create OpenCL context */
    verbose_printf( "[INFO] Creating OpenCL context\n");
    context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
    assert(ret == CL_SUCCESS);

    /* Create Command Queue */
    verbose_printf( "[INFO] Creating command queue\n");
    command_queue = clCreateCommandQueue(context, device_id,
        CL_QUEUE_PROFILING_ENABLE, // Enable profiling
        &ret);
    assert(ret == CL_SUCCESS);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_VENDOR, sizeof(deviceVendor), deviceVendor, 0);    
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Device vendor: %s\n", deviceVendor);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_ADDRESS_BITS, sizeof(numberOfBits), &numberOfBits, 0);    
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Device address bits: %u\n", numberOfBits);

    /* Print Local memory size */
    ret = clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(cl_ulong), &localMemorySize, 0);    
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Local Memory size: %lu Bytes (%.2f kiB)\n", (long unsigned int) localMemorySize, ((float) localMemorySize)/1024.0);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(cl_ulong), &globalMemorySize, 0);
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Global Memory size: %lu Bytes (%.2f MiB)\n", (long unsigned int) globalMemorySize, ((float) globalMemorySize)/1048576.0);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, sizeof(cl_ulong), &globalCacheSize, 0);
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Global Cache size: %lu Bytes\n", (long unsigned int) globalCacheSize);

    ret = clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &computationUnits, 0);
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Total number of parallel compute cores: %u \n", (unsigned int) computationUnits);

    ret = clGetDeviceInfo(device_id,  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(cl_uint), &maxWorkItemDim, 0);
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Max work item dimensions: %u \n", (unsigned int) maxWorkItemDim);

    if ((maxWorkItem = malloc(sizeof(size_t)*maxWorkItemDim)) == NULL){
        printf("Memory error");
        exit(1);
    }

    ret = clGetDeviceInfo(device_id,  CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(size_t)*maxWorkItemDim, maxWorkItem, 0);
    assert(ret == CL_SUCCESS);
    if (verbose) { 
        for(i=0; i<maxWorkItemDim; i++){
            printf("[INFO] Max number of work items for dimension %d: %lu \n", i, maxWorkItem[i]);
        }
    }

    ret = clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxWorkGroupSize), &maxWorkGroupSize, 0);
    assert(ret == CL_SUCCESS);
    if (verbose) printf("[INFO] Max work group size: %u \n", (unsigned int) maxWorkGroupSize);
    
}

void create_opencl_memory_buffers(){

    verbose_printf( "\n---------------- Buffer creation ----------------\n\n");

    verbose_printf( "[INFO] Creating memory buffer (for key)\n");
    memobj0 = clCreateBuffer(
        context,
        CL_MEM_READ_ONLY, // Memory in Read only mode
        mem_size_key,
        NULL,
        &ret);
    assert(ret == CL_SUCCESS);

    verbose_printf( "[INFO] Creating memory buffer (for plaintext)\n");
    memobj1 = clCreateBuffer(
        context,
        CL_MEM_READ_WRITE, // R/W because we use this buffer both for reading plaintext and for writing ciphertext.
        mem_size,
        NULL,
        &ret);
    if (ret != CL_SUCCESS){
        switch (ret) {
            case CL_INVALID_CONTEXT:
                printf("[ERROR] The context is not valid\n");
                break;
            case CL_INVALID_VALUE:
                printf("[ERROR] The value in cl_mem_flag is not valid\n");
                break;
            case CL_INVALID_BUFFER_SIZE:
                printf("[ERROR] The buffer size is 0 (zero) or exceeds the range supported by the compute devices associated with the context.\n");
                break;
            case CL_INVALID_HOST_PTR:
                printf("[ERROR] The host_ptr is NULL, but CL_MEM_USE_HOST_PTR, CL_MEM_COPY_HOST_PTR, and CL_MEM_ALLOC_HOST_PTR are set; or host_ptr is not NULL, but the CL_MEM_USE_HOST_PTR, CL_MEM_COPY_HOST_PTR, and CL_MEM_ALLOC_HOST_PTR are not set.\n");
                break;
            /*case CL_INVALID_OBJECT_ALLOCATION_FAILURE:
                printf("[ERROR] Unable to allocate memory for the memory object\n");
                break;*/
            case CL_OUT_OF_HOST_MEMORY:
                printf("[ERROR] The host is unable to allocate OpenCL resources\n");
                break;
            default:
                printf("[ERROR] Unknown clCreateBuffer error: %d\n", ret);
                break;
        }
    }
    assert(ret == CL_SUCCESS);

}

void pre_compute_key(){
    int i;

    verbose_printf( "\n---------------- Key pre-computation ----------------\n\n");

    verbose_printf("[INFO] Pre-computation of key\n");


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

    verbose_printf("[INFO] Pre-computation of key completed\n"); 
}


void copy_data_to_opencl_buffers() {

    verbose_printf( "\n---------------- Buffer copy ----------------\n\n");

    verbose_printf( "[INFO] Copying into memory buffer (key)\n");
    ret = clEnqueueWriteBuffer(
        command_queue,
        memobj0,
        CL_TRUE,
        0,
        mem_size_key,
        w,
        0,
        NULL,
        NULL);
    assert(ret == CL_SUCCESS);

    if(verbose) printf( "[INFO] Copying into memory buffer (plain) - Size: %lu Bytes \n", mem_size);
    ret = clEnqueueWriteBuffer(
        command_queue,
        memobj1,
        CL_TRUE,
        0,
        mem_size,
        plain,
        0,
        NULL,
        NULL);
    assert(ret == CL_SUCCESS);

}

void create_opencl_program_from_source(){
    verbose_printf( "[INFO] Creating kernel program from kernel source\n");

    program = clCreateProgramWithSource(context, 1, (const char **) &source_str, (const size_t *) &source_size, &ret);
    assert(ret == CL_SUCCESS);
}


void build_opencl_program(){

    int wrote = 0;

    verbose_printf( "\n---------------- Kernel build ----------------\n\n");

    verbose_printf( "[INFO] Building kernel program\n");

    if ((build_args = malloc(sizeof(char)*MAX_BUILD_ARG_LENGTH)) == NULL){ //MAX_BUILD_ARG_LENGTH is the max length of build arguments
        printf("Memory error\n");
        exit(1);
    }

    wrote += sprintf(build_args, "%s -DNUM_ENCRYPT_BLOCKS_FOR_WORK_ITEM=%lu", default_build_args,
            num_encrypt_blocks_for_work_item);

    if (unroll_main_work_cycle) {
        wrote += sprintf(build_args+wrote, " -DUNROLL_MAIN_LOOP");
    }
    
    if (unroll_subkey_operation) {
        wrote += sprintf(build_args+wrote, " -DUNROLL_GENSUBKEY");
    }

    if (counter_mode) {
        wrote += sprintf(build_args+wrote, " -DCTR_MODE -DIV_0=%d -DIV_1=%d -DIV_2=%d -DIV_3=%d",
                         iv[0], iv[1], iv[2], iv[3]);
    }

    
    if(verbose) { printf("[INFO] Building using the following build arguments: %s\n", build_args); }
    ret = clBuildProgram(program, 1, &device_id, build_args, NULL, NULL);
    if (ret != CL_SUCCESS){

        printf("[ERROR] Build error:\n");

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
    } else {
        verbose_printf( "[INFO] Build successful\n");
    }

}

void save_opencl_binaries() {
    cl_uint program_count;
    FILE *fd, *fd_popen;
    size_t written = 0;
    char *commandstr;
    char *cmdout;
    int i = 0;

    verbose_printf("[INFO] Getting number of programs generated\n");
    ret = clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof(cl_uint), &program_count, NULL);
    assert(ret == CL_SUCCESS);
    if (program_count != 1){
        printf("[ERROR] The number of generated binary programs is different from 1. Was it expected?\n");
        exit(1);
    }

    if ((program_binary = (char**) malloc(sizeof(char*) * program_count)) == NULL){
        printf("Memory error\n");
    }



    verbose_printf("[INFO] Getting size of generated binary code\n");
    ret = clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &program_size, NULL);
    assert(ret == CL_SUCCESS);

    if ((program_binary[0] = (char*) malloc(sizeof(char) * (program_size+1))) == NULL) {
        printf("Memory error\n");
        exit(1);
    }
    
    verbose_printf("[INFO] Getting program binary code\n");
    ret = clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(char *), program_binary, NULL);
    assert(ret == CL_SUCCESS);

    if ((fd = fopen(binary_dump_file, "w")) == NULL) {
        printf("[ERROR] Impossible to write binary dump file to %s file.\n", binary_dump_file);
        exit(1);
    }
    
    written = fwrite(program_binary[0], sizeof(char), program_size, fd);
    if (written != program_size)
    {
        printf("[ERROR] Couldn't write the dump on file %s\n", binary_dump_file);
        exit(1);
    }
    fclose(fd);

    if ((commandstr = malloc(sizeof(char)*(300))) == NULL){ //TODO change 300 to define value
        printf("Memory error\n");
        exit(1);
    }
    
    sprintf(commandstr, "python -c \"print len(open('%s','r').read().split(' bra')[0].split('BB0_1:')[1].split('\\n'))\" | tr -d '\\n'", binary_dump_file);
    
    //if (verbose) {printf("[INFO] Executing command: %s\n", commandstr);}    

    fd_popen = (FILE *) popen(commandstr,"r");

    if ((cmdout = malloc(sizeof(char)*200)) == NULL){ //TODO change 200 to define value
        printf("Memory error\n");
        exit(1);
    }

    written = fread(cmdout, sizeof(char), 200, fd_popen); //TODO
    cmdout[written] = '\0';

    pclose(fd_popen);

    for(i=0; cmdout[i] != '\0'; i++){
        if (!(cmdout[i]>='0' && cmdout[i]<='9')){
            verbose_printf("[SOFT-ERROR] It's not possibile to establish the number of instructions for this card!\n");
            i=-1;
            num_instructions = 0;
            break;
        }
    }


    if (i != -1){
        num_instructions = atoi(cmdout);

        if (verbose) {printf("[INFO] Number of instructions of the main loop body: %lu\n", num_instructions);}
    }


}

void create_opencl_kernel() {
    verbose_printf( "[INFO] Creating OpenCl kernel\n");
    kernel = clCreateKernel(program, "serpent_encrypt", &ret);
    assert(ret == CL_SUCCESS);
}


void set_kernel_parameters() {

    verbose_printf( "\n---------------- Kernel parameters ----------------\n\n");
    
    verbose_printf( "[INFO] Setting kernel arguments (0) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        0, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj0); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);

    verbose_printf( "[INFO] Setting kernel arguments (1) \n");
    ret = clSetKernelArg(
        kernel, //Kernel object
        1, // kernel parameter index
        sizeof(cl_mem), //size of argument data
        (void *) &memobj1); // pointer of data used as the argument
    assert(ret == CL_SUCCESS);


}

void enqueue_opencl_kernel(){

    verbose_printf( "\n---------------- Enqueue kernel task ----------------\n\n");

    verbose_printf( "[INFO] Executing opencl kernel (enqueue task)\n");
    ret = clEnqueueNDRangeKernel(
        command_queue,
        kernel,
        workDimensions, //Number of dimensions (max = 3)
        NULL,
        &global_work_size, //array che indica il numero di work-items che eseguiranno questo kernel, per ciascuna dimensione dichiarata
        &local_work_size, //Numero di work-items che creano un work-group. Il totale e' calcolato come il prodotto degli elementi dell'array. Il singolo elemento dell'array fa riferimento al numero di work-item che crea un gruppo nella dimensione n. Il numero totale deve essere minore di CL_DEVICE_MAX_WORK_GROUP_SIZE.
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

    verbose_printf( "\n---------------- Reading output buffers ----------------\n\n");
    
    /* Copy results from the memory buffer */
    verbose_printf( "[INFO] Copying results from memory buffer (encrypted cleartext)\n");
    ret = clEnqueueReadBuffer(
        command_queue,
        memobj1,
        CL_TRUE, //Blocking operation (wait finish)
        0,
        mem_size,
        plain, //output pointer
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
    if (!csv_output){
        printf( "\n---------------- Performance evaluation ----------------\n\n");
    }
    clWaitForEvents(1 , &startEvent);

    verbose_printf("[INFO] Getting start time\n");
    ret = clGetEventProfilingInfo(
        startEvent,                              // the event object to get info for
        CL_PROFILING_COMMAND_START,         // the profiling data to query
        sizeof(cl_ulong),                   // the size of memory pointed by param_value
        &startTime,                          // pointer to memory in which the query result is returned
        NULL                    // actual number of bytes copied to param_value
    );
    assert(ret == CL_SUCCESS);

    verbose_printf("[INFO] Getting end time\n");
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
    
    verbose_printf( "\n[INFO] Flushing and releasing buffers\n");
    ret = clFlush(command_queue);
    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseMemObject(memobj0);
    ret = clReleaseMemObject(memobj1);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);

    free(source_str);
    free(program_binary);
}

void print_performance_speed(){
    speed = (((float) (BLOCK_SIZE_IN_BYTES*num_encrypt_blocks))/executionTime);

    if (!csv_output){
        printf("[PERF] Speed: %.2f B/s  ", speed);

        if (speed/1073741824.0 >= 1.0){
            printf("( %.2f GiB/s )\n", speed/1073741824.0);
        } else if (speed/1048576.0 >= 1.0){ //it's Over 9000!!!!
            printf("( %.2f MiB/s )\n", speed/1048576.0);
        } else if (speed/1024.0 >= 1.0){
            printf("( %.2f kiB/s )\n", speed/1024.0);
        } 

        printf("\n");
    }
}

void print_performance_time() {
    if (verbose) printf("[PERF] Start time: %lu\n", (long unsigned int) startTime);
    if (verbose) printf("[PERF] End time  : %lu\n", (long unsigned int) endTime);

    executionTime = ((float) (endTime - startTime))*0.000000001;

    if (!csv_output){
        printf("[PERF] Execution time: %e seconds\n", executionTime);

        printf("[PERF] Encrypted data: %lu Bytes\n", BLOCK_SIZE_IN_BYTES*num_encrypt_blocks);
    }
}

void csv_print_performance() {
    printf(",%.4f,%lu,%.4f\n", executionTime, BLOCK_SIZE_IN_BYTES*num_encrypt_blocks, speed);
}

void print_instructions_per_byte() {

    instructions_per_byte = ((float) num_instructions*num_work_items*num_encrypt_blocks_for_work_item) / ((num_work_items*mem_size_key)+(num_work_items*num_encrypt_blocks_for_work_item*2*BLOCK_SIZE_IN_BYTES));

    if (verbose) { printf("[PERF] Instructions per byte: %.2f [Instructions/Byte]\n", instructions_per_byte); }
}

void csv_print_instructions_per_byte(){
    printf(",%.2f", instructions_per_byte);
}
































int main(int argc, char **argv){

    int k;

    /*Initialize work value (number of work items, number of blocks for work-item... and so on)*/
    experiment_size_default_declarations();

    parse_arguments(argc, argv);

    calculate_experiment_parameters();

    if (csv_output) csv_header_print();

    print_experiment_size_parameters();

    if (csv_output) csv_print_experiment_size_parameters();

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

    /* Save binaries of the program */
    save_opencl_binaries();

    /* Create OpenCL Kernel */
    create_opencl_kernel();

    /* Set OpenCL Kernel Parameters */
    set_kernel_parameters();

    /* Execute OpenCL Kernel */
    workDimensions      = 1;
    global_work_size    = num_work_items;
    local_work_size     = num_work_items_in_work_group; //????
    //local_work_size = NULL;
    enqueue_opencl_kernel();

    /* Wait for gracefully termination of running operations */
    wait_opencl_finish_exec();

    /* Copy results from output buffer (ciphertext) */
    copy_results_from_opencl_buffer();
    cipher = plain; //We write the output again in the same array used for the plaintext
                    // (the size is the same). This assignement is just to be clear in next instructions

    if(memcmp(cipher, correct, BLOCK_SIZE_IN_BYTES*num_encrypt_blocks) == 0)
        verbose_printf("\n[SUCCESS] Output ciphertext is correct!\n\n");
    else {
        /* Check if ciphertext is correct */
        for(k=0; k<(num_encrypt_blocks); k++){
            if(!(memcmp(
                    &(cipher[k*BLOCK_SIZE_IN_32BIT_WORDS]),
                    &(correct[k*BLOCK_SIZE_IN_32BIT_WORDS]),
                    BLOCK_SIZE_IN_BYTES
                    ) == 0)){
                printf("\n[ERROR] Output ciphertext is not correct! (block number %d, couting from 0)\n\n", k);
                exit(1); 
            }
        }
        printf("[CHECK] Strange behaviour detected. Check me\n");
    }



    get_opencl_performance_time();

    print_performance_time();

    print_instructions_per_byte();

    /* Print calculated speed */
    print_performance_speed();

    if (csv_output) {
        csv_print_instructions_per_byte();
        csv_print_performance();
    }



    /* Release buffers and stuff */
    release_opencl_resources();

    return 0;
}

