// last check : 28.01 - 14:40
// vs code version 1.60.2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>


#define MAX_LINE_MEMIN 1048576
#define LINE_LENGTH 9  //line is a string
#define MAX_LINES 1024
#define NUMBER_CORES 4
#define NUMBER_BLOCKS 64
#define DSRAM_SIZE 256
#define NUMBER_REGS 16
#define BusRd 1
#define BusRdX 2
#define Flush 3

void fetch(int core_num);
void decode(int core_num);
void alu(int core_num);
void mem(int core_num);
void wb(int core_num);
void MESI_bus();
int data_in_dsram(int core_num, int address);
int is_branch(char *command);
void int_to_hex(int x, char * hex);
int hex_to_int(char * hex);
void bin_to_hex(char *bin, char *hex);
void hex_to_bin(char *bin,char*hex);
int arithmetic_shift_right(int num, int shift);
void slice_inst(char * input, int output[]);
void read_file_into_array_memin(const char *filename, char ***array, int *line_count);
int read_file(const char *filename, char lines[MAX_LINES][LINE_LENGTH]);

typedef struct {
    int pc_pipe;
    char inst[LINE_LENGTH];
    int op;
    int rd;
    int rs;
    int rt;
    int imm;
    int dist; //the index of the dist register(rd)
    int ALU_pipe; //outp of the alu
    int data; // data from memory 
}temp_reg;

typedef struct {
    int arr[NUMBER_CORES];  // Array to store queue elements
    int front;          // Index of the front of the queue
    int rear;           // Index of the rear of the queue
    int size;           // Current size of the queue
} Queue;

// Function to initialize the queue
void initializeQueue(Queue* q) {
    q->front = 0;
    q->rear = -1;
    q->size = 0;
}

// Function to check if the queue is full
int isFull(Queue* q) {
    return q->size == NUMBER_CORES;
}

// Function to check if the queue is empty
int isEmpty(Queue* q) {
    return q->size == 0;
}

// Function to add an element to the queue (enqueue)
void enqueue(Queue* q, int value) {
    if (isFull(q)) {
    } else {
        // Circularly update rear
        q->rear = (q->rear + 1) % NUMBER_CORES;
        q->arr[q->rear] = value;
        q->size++;
    }
}

// Function to remove an element from the queue (dequeue)
int dequeue(Queue* q) {
    if (isEmpty(q)) {
        return -1;  // Indicating empty queue
    } else {
        int dequeuedValue = q->arr[q->front];
        // Circularly update front
        q->front = (q->front + 1) % NUMBER_CORES;
        q->size--;
        return dequeuedValue;
    }
}

int bus_counter;
char Imem0[MAX_LINES][LINE_LENGTH];
char Imem1[MAX_LINES][LINE_LENGTH];
char Imem2[MAX_LINES][LINE_LENGTH];
char Imem3[MAX_LINES][LINE_LENGTH];
char** memout;
int regs[NUMBER_CORES][NUMBER_REGS];
temp_reg pipe_regs[NUMBER_CORES][8]; /*the registers of the pipeline (old and new for all the cores)*/
char Dsram[NUMBER_CORES][DSRAM_SIZE][LINE_LENGTH];
char Tsram[NUMBER_CORES][NUMBER_BLOCKS][15];
int cycles[NUMBER_CORES];       /* cycles[i] is the number of the current cycle in core[i] */
int instructions[NUMBER_CORES]; /* instructions[i] is the number of the instructions done in core[i] */
int read_hit[NUMBER_CORES];     /* read_hit[i] is the number of read_hit in the Dsram of core[i] */
int write_hit[NUMBER_CORES];    /* write_hit[i] is the number of write_hit in the Dsram of core[i] */
int read_miss[NUMBER_CORES];    /* read_miss[i] is the number of read_miss in the Dsram of core[i] */
int write_miss[NUMBER_CORES];   /* write_miss[i] is the number of write_miss in the Dsram of core[i] */
int decode_stall[NUMBER_CORES]; /* decode_stall[i] is the number of decode stalls in the pipeline of core[i] */
int mem_stall[NUMBER_CORES];    /* mem_stall[i] is the number of mem stalls in the pipeline of core[i] */
int pc[NUMBER_CORES];
int pre_branch_pc[NUMBER_CORES];
/*putting in a queues the parameters of the bus*/
Queue core_bus_requests;
Queue bus_origid;
Queue bus_cmd;
Queue bus_address;
int bus_shared[NUMBER_CORES]; 
int bus_busy;
int core_ready[NUMBER_CORES]; /*core_ready[i] is 1 if the bus gave him the block he wanted*/
int need_flush[NUMBER_CORES]; /*need_flush[i] is 1 if the tag is wrong and we need to flush the block before reading the other block*/
int alredy_enqueued[NUMBER_CORES]; /*alredy_enqueued[i] is 1 if the request is alredy in the round robin*/
int who_flush ; // indicates which core will flush {0,1,2,3,4}
int who_needs; // indicates which core (alternatively the mem) needs the data {0,1,2,3,4}
int sharing_block[NUMBER_CORES];    //sharing_block[i] is 1 if the block is shared with core i else it is 0
int reg_is_available[NUMBER_CORES][NUMBER_REGS];    //reg_is_available[i][j]
int flag_decode_stall[NUMBER_CORES];
int flag_mem_stall[NUMBER_CORES];
FILE* core_trace_files[NUMBER_CORES];
FILE* bus_trace_file;
int the_end[NUMBER_CORES];
int we_finished;
int last_cycle[NUMBER_CORES];



// char* filename,   char ***array,   int*;
//int main(int argc, char *argv[]){
int main(){
    FILE* memout_file;
    FILE* regout_files[NUMBER_CORES];
    FILE* dsram_files[NUMBER_CORES];
    FILE* tsram_files[NUMBER_CORES];
    FILE* stats_files[NUMBER_CORES];

    int i, c, k, address;
    /*
    if (argc != 28){
        printf("invalid input");
        exit(0);
    }
    char* imem0_txt = argv[1];
    char* imem1_txt = argv[2];
    char* imem2_txt = argv[3];
    char* imem3_txt = argv[4];
    char* memin_txt = argv[5];
    char* memout_txt = argv[6];
    char* regout0_txt = argv[7];
    char* regout1_txt = argv[8];
    char* regout2_txt = argv[9];
    char* regout3_txt = argv[10];
    char* core0trace_txt = argv[11];
    char* core1trace_txt = argv[12];
    char* core2trace_txt = argv[13];
    char* core3trace_txt = argv[14];
    char* bustrace_txt = argv[15];
    char* dsram0_txt = argv[16];
    char* dsram1_txt = argv[17];
    char* dsram2_txt = argv[18];
    char* dsram3_txt = argv[19];
    char* tsram0_txt = argv[20];
    char* tsram1_txt = argv[21];
    char* tsram2_txt = argv[22];
    char* tsram3_txt = argv[23];
    char* stats0_txt = argv[24];
    char* stats1_txt = argv[25];
    char* stats2_txt = argv[26];
    char* stats3_txt = argv[27];
    */
    

    char* imem0_txt = "imem0.txt";
    char* imem1_txt = "imem1.txt";
    char* imem2_txt = "imem2.txt";
    char* imem3_txt = "imem3.txt";
    char* memin_txt = "memin.txt";
    char* memout_txt = "memout.txt";
    char* regout0_txt = "regout0.txt";
    char* regout1_txt = "regout1.txt";
    char* regout2_txt = "regout2.txt";
    char* regout3_txt = "regout3.txt";
    char* core0trace_txt = "core0trace.txt";
    char* core1trace_txt = "core1trace.txt";
    char* core2trace_txt = "core2trace.txt";
    char* core3trace_txt = "core3trace.txt";
    char* bustrace_txt = "bustrace.txt";
    char* dsram0_txt = "dsram0.txt";
    char* dsram1_txt = "dsram1.txt";
    char* dsram2_txt = "dsram2.txt";
    char* dsram3_txt = "dsram3.txt";
    char* tsram0_txt = "tsram0.txt";
    char* tsram1_txt = "tsram1.txt";
    char* tsram2_txt = "tsram2.txt";
    char* tsram3_txt = "tsram3.txt";
    char* stats0_txt = "stats0.txt";
    char* stats1_txt = "stats1.txt";
    char* stats2_txt = "stats2.txt";
    char* stats3_txt = "stats3.txt";


    int memin_lines;
    int line_count;
    line_count = read_file(imem0_txt, Imem0);
    if (line_count < 0) {
            printf("Failed to read file: %s\n", imem0_txt);
        }

    line_count = read_file(imem1_txt, Imem1);
    if (line_count < 0) {
            printf("Failed to read file: %s\n", imem1_txt);
        }

    line_count = read_file(imem2_txt, Imem2);
    if (line_count < 0) {
            printf("Failed to read file: %s\n", imem2_txt);
        }

    line_count = read_file(imem3_txt, Imem3);
    if (line_count < 0) {
            printf("Failed to read file: %s\n", imem3_txt);
        }

    read_file_into_array_memin(memin_txt, &memout, &memin_lines);
    
    for (i = memin_lines; i < MAX_LINE_MEMIN; i++) {
        memout[i] = strdup("00000000\0");
        if (!memout[i]) {
            perror("Memory allocation failed while adding padding to memout");
            return 0;
        }
    }


    core_trace_files[0] = fopen(core0trace_txt, "w");
    if (!core_trace_files[0]) {
        perror("Error opening file");
        return 0;
    }
    core_trace_files[1] = fopen(core1trace_txt, "w");
    if (!core_trace_files[1]) {
        perror("Error opening file");
        return 0;
    }
    core_trace_files[2] = fopen(core2trace_txt, "w");
    if (!core_trace_files[2]) {
        perror("Error opening file");
        return 0;
    }
    core_trace_files[3] = fopen(core3trace_txt, "w");
    if (!core_trace_files[3]) {
        perror("Error opening file");
        return 0;
    }
    bus_trace_file = fopen(bustrace_txt, "w");
    if (!bus_trace_file) {
        perror("Error opening file");
        return 0;
    }



    for(i=0; i<NUMBER_CORES; i++){
        for(c=0; c<8; c++){
            memset(&pipe_regs[i][c], 0, sizeof(pipe_regs[i][c]));
            pipe_regs[i][c].pc_pipe = -1;
        }
        for(k=0; k<NUMBER_REGS; k++){
            regs[i][k] = 0;
            reg_is_available[i][k] = 1;
        }
        
        for(c=0; c<DSRAM_SIZE; c++){
            for(k=0; k<LINE_LENGTH; k++){
                if (k == (LINE_LENGTH-1)){
                    Dsram[i][c][k] = '\0';
                }
                else{
                    Dsram[i][c][k] = '0';
                }
            }
        }

        for(c=0; c<NUMBER_BLOCKS; c++){
            for(k=0; k<15; k++){ 
                if (k == 14){
                    Tsram[i][c][k] = '\0';
                }
                else{
                    Tsram[i][c][k] = '0';
                }
            }
        }

        cycles[i] = 0;
        instructions[i] = 0;
        read_hit[i] = 0;
        write_hit[i] = 0;
        read_miss[i] = 0;
        write_miss[i] = 0;
        decode_stall[i] = 0;
        mem_stall[i] = 0;
        pc[i] = 0;
        core_ready[i] = 1;
        need_flush[i] = 0;
        sharing_block[i] = 0;
        bus_shared[i] = 0;
        flag_decode_stall[i] = 0;
        flag_mem_stall[i] = 0;
        the_end[i] = 0;
        last_cycle[i] = 0;
        pre_branch_pc[i] = 0;
    }
    we_finished = 0;
    bus_counter = 0;
    initializeQueue(&core_bus_requests);
    initializeQueue(&bus_origid);
    initializeQueue(&bus_cmd);
    initializeQueue(&bus_address);
    bus_busy = 0;
    who_flush = 4;
    who_needs = 4; 



    // here we start after init all the global variables and the file names
    // start with if busy then MESI()


    while(1){
        /*if(strcmp(memout[0], "00000001")){
            printf("aa");
        }*/
        /*if(cycles[1] == 10000){
            printf("debug");
        }*/
        MESI_bus();
        for(i=0; i<4; i++){

            if(pc[i] == -1 && pipe_regs[i][6].pc_pipe == -2){
                // here we can write the total cycles in states file
                if(cycles[i] != -1){
                    last_cycle[i] = cycles[i];
                }
                cycles[i] = -1;
                continue;
            }

            fprintf(core_trace_files[i],"%d ",cycles[i]);

            fetch(i);
            decode(i);
            alu(i);
            mem(i);
            wb(i);

            
            pipe_regs[i][6] = pipe_regs[i][7]; // write back continues
            if(flag_decode_stall[i] || flag_mem_stall[i]){ // if stall
                if(pipe_regs[i][3].op >=9 && pipe_regs[i][3].op <= 15){
                    pc[i] = pre_branch_pc[i];
                }
                else{
                    if(pc[i] >= 0){
                        pc[i] --; //fetch the same pc
                    }
                }
                if(flag_decode_stall[i] && (!flag_mem_stall[i])){ // if just decode stall
                    pipe_regs[i][4] = pipe_regs[i][5]; // mem continues
                    pipe_regs[i][2] = pipe_regs[i][3]; // alu continues
                    decode_stall[i]++;
                }
                if(flag_mem_stall[i] && (!flag_decode_stall[i])){ // if just mem stall
                    mem_stall[i]++;
                }
                if(flag_mem_stall[i] && flag_decode_stall[i]){
                    //decode_stall[i]++;
                    mem_stall[i]++;
                }
            }
            else{
                pipe_regs[i][6] = pipe_regs[i][7];
                pipe_regs[i][4] = pipe_regs[i][5];
                //strcpy
                pipe_regs[i][2] = pipe_regs[i][3];
                pipe_regs[i][0] = pipe_regs[i][1];
            }
            if(cycles[i] != -1){
                cycles[i]++;
            }
            

            fflush(core_trace_files[i]);

        }
        fflush(bus_trace_file);

        if(cycles[0] == -1 && cycles[1] == -1 && cycles[2] == -1 && cycles[3] == -1){
            break;
        }

    }

    we_finished = 1;

    for(i=0; i<NUMBER_CORES; i++){
        fclose(core_trace_files[i]);
    }

    for(i=0; i<NUMBER_CORES; i++){
        for(c=0; c<NUMBER_BLOCKS; c++){
            if(Tsram[i][c][0] == '1' && Tsram[i][c][1] == '1'){
                char *tr = Tsram[i][c];
                char *bin = tr+2;
                address = ((int)strtol(bin,NULL,2))*pow(2,8) + c*4;

                for(k=0; k<4; k++){
                    strcpy(memout[address+k], Dsram[i][4*c+k]); 
                }                        
            }
        }
    }

    for(i=0; i<20; i++){
        MESI_bus();
    }


    regout_files[0] = fopen(regout0_txt, "w");
    if (!regout_files[0]) {
        perror("Error opening file");
        return 0;
    }
    regout_files[1] = fopen(regout1_txt, "w");
    if (!regout_files[1]) {
        perror("Error opening file");
        return 0;
    }
    regout_files[2] = fopen(regout2_txt, "w");
    if (!regout_files[2]) {
        perror("Error opening file");
        return 0;
    }
    regout_files[3] = fopen(regout3_txt, "w");
    if (!regout_files[3]) {
        perror("Error opening file");
        return 0;
    }



    for(int h =0;h<=13;h++){
        fprintf(regout_files[0],"%08X\n",regs[0][h+2]);
        fprintf(regout_files[1],"%08X\n",regs[1][h+2]);
        fprintf(regout_files[2],"%08X\n",regs[2][h+2]);
        fprintf(regout_files[3],"%08X\n",regs[3][h+2]);
    }
    fclose(regout_files[0]);
    fclose(regout_files[1]);
    fclose(regout_files[2]);
    fclose(regout_files[3]);

    dsram_files[0] = fopen(dsram0_txt, "w");
    if (!dsram_files[0]) {
        perror("Error opening file");
        return 0;
    }
    dsram_files[1] = fopen(dsram1_txt, "w");
    if (!dsram_files[1]) {
        perror("Error opening file");
        return 0;
    }
    dsram_files[2] = fopen(dsram2_txt, "w");
    if (!dsram_files[2]) {
        perror("Error opening file");
        return 0;
    }
    dsram_files[3] = fopen(dsram3_txt, "w");
    if (!dsram_files[3]) {
        perror("Error opening file");
        return 0;
    }
    tsram_files[0] = fopen(tsram0_txt, "w");
    if (!tsram_files[0]) {
        perror("Error opening file");
        return 0;
    }
    tsram_files[1] = fopen(tsram1_txt, "w");
    if (!tsram_files[1]) {
        perror("Error opening file");
        return 0;
    }
    tsram_files[2] = fopen(tsram2_txt, "w");
    if (!tsram_files[2]) {
        perror("Error opening file");
        return 0;
    }
    tsram_files[3] = fopen(tsram3_txt, "w");
    if (!tsram_files[3]) {
        perror("Error opening file");
        return 0;
    }
    stats_files[0] = fopen(stats0_txt, "w");
    if (!stats_files[0]) {
        perror("Error opening file");
        return 0;
    }
    stats_files[1] = fopen(stats1_txt, "w");
    if (!stats_files[1]) {
        perror("Error opening file");
        return 0;
    }
    stats_files[2] = fopen(stats2_txt, "w");
    if (!stats_files[2]) {
        perror("Error opening file");
        return 0;
    }
    stats_files[3] = fopen(stats3_txt, "w");
    if (!stats_files[3]) {
        perror("Error opening file");
        return 0;
    }
    
    for(i=0;i<=3;i++){
        fprintf(stats_files[i],"Cycles %d\n instructions %d\n read_hit %d\n write_hit %d\n read_miss %d\n write_miss %d\n decode_stall %d\n mem_stall %d\n",
        last_cycle[i], instructions[i], read_hit[i], write_hit[i], read_miss[i], write_miss[i], decode_stall[i], mem_stall[i] );
        fflush(stats_files[i]);

        for(c=0; c<DSRAM_SIZE; c++){
            for(k=0; k< 8; k++){
                fprintf(dsram_files[i],"%c" ,Dsram[i][c][k]);
            }
            fprintf(dsram_files[i], "\n");
        }
        fflush(dsram_files[i]);

        for(c=0 ; c<NUMBER_BLOCKS; c++){
            char hex[9];                     
            bin_to_hex(Tsram[i][c], hex);
            for(k=0; k<8; k++){
                fprintf(tsram_files[i],"%c" , hex[k]);
            }
            fprintf(tsram_files[i], "\n");
        }
        fflush(tsram_files[i]);
        
    }

    for(i=0; i<NUMBER_CORES; i++){
        fclose(stats_files[i]);
        fclose(tsram_files[i]);
        fclose(dsram_files[i]);
    }


    
    memout_file = fopen(memout_txt, "w");
    if (!memout_file) {
        perror("Error opening file");
        return 0;
    }
    for(i=0; i<MAX_LINE_MEMIN; i++){
        for(c=0; c<8; c++){
            fprintf(memout_file,"%c", memout[i][c]); //  may need to put diffrent indexes
        }
        fprintf(memout_file, "\n");
    }
    for(i=0; i<MAX_LINE_MEMIN; i++){
        free(memout[i]);
    }
    free(memout);
    fclose(memout_file);
    fclose(bus_trace_file);

    return 0;
    
}


void read_file_into_array_memin(const char *filename, char ***array, int *line_count) {
    int even = 0;
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for the array of strings
    *array = malloc(MAX_LINE_MEMIN * sizeof(char *));
    if (!*array) {
        perror("Memory allocation failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    char buffer[LINE_LENGTH]; // Buffer to store each line
    *line_count = 0;

    // Read each line from the file
    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove the newline character if present
        buffer[strcspn(buffer, "\n")] = '\0';

        // Allocate memory for the line and store it
        if(!even){
            (*array)[*line_count] = strdup(buffer);
            if (!(*array)[*line_count]) {
                perror("Memory allocation failed");
                fclose(file);
                exit(EXIT_FAILURE);
            }
        (*line_count)++;
        }
        even = 1 - even;
    }

    fclose(file);
}


int read_file(const char *filename, char lines[MAX_LINES][LINE_LENGTH]) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        return -1; // Indicate error
    }

int line_count = 0;
    while (fgets(lines[line_count], LINE_LENGTH + 1, file) && line_count < MAX_LINES) {
        // Remove newline character if present
        lines[line_count][strcspn(lines[line_count], "\n")] = '\0';
        line_count++;
    }

    fclose(file);
    return line_count; // Return the number of lines read
}

void mem(int core_num){  
    if(pipe_regs[core_num][4].op == 20){
        fprintf(core_trace_files[core_num], "%03X ", pipe_regs[core_num][4].pc_pipe);
        pipe_regs[core_num][7].op = pipe_regs[core_num][4].op;
        pipe_regs[core_num][7].pc_pipe = pipe_regs[core_num][4].pc_pipe;
        return;
    }
    if(pc[core_num] == -1 && pipe_regs[core_num][4].pc_pipe == -2){ // reached halt
        fprintf(core_trace_files[core_num], "--- ");
        pipe_regs[core_num][7].pc_pipe = -2;
        return;
    }
    if(pipe_regs[core_num][4].pc_pipe == -1){
        fprintf(core_trace_files[core_num], "--- ");
        pipe_regs[core_num][7].pc_pipe = -1;
        return;
    }                              
    int data = 0;
    int op = pipe_regs[core_num][4].op;
    int address = pipe_regs[core_num][4].ALU_pipe;                    
    int wanted_tag = address / pow(2,8); // assuming 20 bit address   // tag is the first 8 bits address 
    int index = address % 256;
    int block = (index / 4);
    char *tr = Tsram[core_num][block];            //[TAG][MESI] block as in the assignment 
    char *tag = tr+2;                                // to ditch first two 
    char tag_hex[4]; // may need to make it 3
    bin_to_hex(tag, tag_hex);
    int tag_num =  hex_to_int(tag_hex);                 // same tag but in int
    int mesi_state = -1;
    if(*tr == '0'){ // calculating messi state
        if(*(tr+1) == '0'){
            mesi_state = 0;
        }
        else{
            mesi_state = 1;
        }
    }
    else{
        if(*(tr+1) == '0'){
            mesi_state = 2;
        }
        else{
            mesi_state = 3;
        }
    }

    if(!alredy_enqueued[core_num]){                    // fills the round robin
        if(op == 16){ //lw
            if (tag_num == wanted_tag){ // tag matched
                if(mesi_state != 0){ // not invalid
                    data = hex_to_int(Dsram[core_num][index]);
                    read_hit[core_num]++;
                }
                else{ // invalid
                    enqueue(&core_bus_requests, core_num);
                    enqueue(&bus_origid, core_num);
                    enqueue(&bus_cmd, BusRd);
                    enqueue(&bus_address, address);
                    core_ready[core_num] = 0; // my data is not ready
                    alredy_enqueued[core_num] = 1;
                    flag_mem_stall[core_num] = 1;
                    read_miss[core_num]++;
                }
            }
            else{ // tag is not matched
                if(mesi_state == 0 || mesi_state == 2){//invalid or exclusive
                    enqueue(&core_bus_requests, core_num);
                    enqueue(&bus_origid, core_num);
                    enqueue(&bus_cmd, BusRd);
                    enqueue(&bus_address, address);
                    core_ready[core_num] = 0; // my data is not ready
                    alredy_enqueued[core_num] = 1;
                    flag_mem_stall[core_num] = 1;
                }
                else{ // modified or 
                    int address_to_flush = (tag_num * pow(2,8)) + (block * 4);
                    need_flush[core_num] = 1;
                    enqueue(&core_bus_requests, core_num);
                    enqueue(&bus_origid, core_num);
                    enqueue(&bus_cmd, Flush);
                    enqueue(&bus_address, address_to_flush);
                    core_ready[core_num] = 0; // my data is not ready
                    alredy_enqueued[core_num] = 1;
                    flag_mem_stall[core_num] = 1;
                }
            }

        }

        if(op == 17){ //sw
            if(tag_num == wanted_tag){ //tag matched
                if(mesi_state == 0){
                    write_miss[core_num]++;
                }
                else{
                    write_hit[core_num]++;
                }
                if(mesi_state == 0 || mesi_state == 1){ //invalid or shared
                    enqueue(&core_bus_requests, core_num);
                    enqueue(&bus_origid, core_num);
                    enqueue(&bus_cmd, BusRdX);
                    enqueue(&bus_address, address);
                    core_ready[core_num] = 0; // my data is not ready
                    alredy_enqueued[core_num] = 1;
                    flag_mem_stall[core_num] = 1;
                }
                else{ // modified or exclusive
                    Tsram[core_num][block][1] = '1'; // modified
                    int_to_hex(pipe_regs[core_num][4].rd, Dsram[core_num][index]);

                }
            }
            else{//tag not matched
                if(mesi_state == 0 || mesi_state == 2){//invalid or exclusive
                    enqueue(&core_bus_requests, core_num);
                    enqueue(&bus_origid, core_num);
                    enqueue(&bus_cmd, BusRdX);
                    enqueue(&bus_address, address);
                    core_ready[core_num] = 0; // my data is not ready
                    alredy_enqueued[core_num] = 1;
                    flag_mem_stall[core_num] = 1;
                }
                else{ // modified or shared
                    need_flush[core_num] = 1;
                    enqueue(&core_bus_requests, core_num);
                    enqueue(&bus_origid, core_num);
                    enqueue(&bus_cmd, Flush);
                    enqueue(&bus_address, address);
                    core_ready[core_num] = 0; // my data is not ready
                    alredy_enqueued[core_num] = 1;
                    flag_mem_stall[core_num] = 1;
                }
            }
        }
    }
    
    if(core_ready[core_num] == 1){
        alredy_enqueued[core_num] = 0;
        if(op == 16){ // lw
            data = hex_to_int(Dsram[core_num][index]);
        }
        if(op == 17){ // sw
            if(mesi_state == 2){
                Tsram[core_num][block][0] = '1'; // modified
                Tsram[core_num][block][1] = '1';
            }
            int_to_hex(pipe_regs[core_num][4].rd, Dsram[core_num][index]);
        }

        // sending all the data to the next pipe register
        pipe_regs[core_num][7].ALU_pipe = pipe_regs[core_num][4].ALU_pipe;
        pipe_regs[core_num][7].data = data;
        pipe_regs[core_num][7].dist = pipe_regs[core_num][4].dist;
        pipe_regs[core_num][7].imm = pipe_regs[core_num][4].imm;
        strcpy(pipe_regs[core_num][4].inst, pipe_regs[core_num][7].inst);
        pipe_regs[core_num][7].op = pipe_regs[core_num][4].op;
        pipe_regs[core_num][7].pc_pipe = pipe_regs[core_num][4].pc_pipe;
        pipe_regs[core_num][7].rd = pipe_regs[core_num][4].rd;
        pipe_regs[core_num][7].rs = pipe_regs[core_num][4].rs;
        pipe_regs[core_num][7].rt = pipe_regs[core_num][4].rt;
    }
    else{
        pipe_regs[core_num][7].pc_pipe = -1;
    }
    fprintf(core_trace_files[core_num], "%03X ", pipe_regs[core_num][4].pc_pipe);
}

// function to check if the data in the dsram (return the core number)
int data_in_dsram(int core_num, int address){
    int i;
    int to_ret;
    int block = (address / 4) % NUMBER_BLOCKS;
    int wanted_tag = address / pow(2,8);
    to_ret = 4;
    for(i=0; i<NUMBER_CORES; i++){
        if(i != core_num){
            char *tr = Tsram[i][block];
            char *tag = tr + 2;
            char tag_hex[4];
            bin_to_hex(tag, tag_hex);
            int tag_num =  hex_to_int(tag_hex);
            //int mesi_state;
            if(*tr == '0'){
                if(*(tr+1) == '0'){
                    //mesi_state = 0;
                    continue;
                }
            }
            if(wanted_tag == tag_num){
                sharing_block[i] = 1;
                to_ret = i;
            }
            
            
        }
    }
    return to_ret;
}

void MESI_bus(){
    static int core_number = -1;
    static int bus_origid_mesi = -1;
    static int bus_cmd_mesi = -1;
    static int bus_address_mesi = -1;
    static int bus_sharing = -1;
    int source_data;
    int i,j, not_equals, tag_num, tag_size, read_address;
    char hex[9];
    char bin[13];
    //int mesi_state;
    int block_number;
    not_equals = 0;
    if(!bus_busy){
        core_number = dequeue(&core_bus_requests);
        bus_origid_mesi =  dequeue(&bus_origid);
        bus_cmd_mesi = dequeue(&bus_cmd);
        bus_address_mesi = dequeue(&bus_address);
        if(core_number == -1){
            return;
        }

        block_number = (bus_address_mesi/4)%NUMBER_BLOCKS;
        
        // check if the data is in another dsram , if so the bus_shared = 1
        source_data = data_in_dsram(core_number, bus_address_mesi);
        if (source_data != 4){
            bus_sharing = 1;
        }
        if(bus_cmd_mesi == BusRd || bus_cmd_mesi == BusRdX){
            fprintf(bus_trace_file,"%d ",cycles[bus_origid_mesi]);
            fprintf(bus_trace_file,"%X ",bus_origid_mesi);
            fprintf(bus_trace_file,"%X ",bus_cmd_mesi);
            fprintf(bus_trace_file,"%05X ",bus_address_mesi);
            fprintf(bus_trace_file,"00000000 0\n");
            who_needs = bus_origid_mesi;
            for(i=0;i<=3;i++){
                if((bus_origid_mesi == i) && bus_sharing != 1){
                    if(bus_cmd_mesi == BusRd){
                        Tsram[i][block_number][0] = '1'; // exclusive
                        Tsram[i][block_number][1] = '0';
                        continue;
                    }
                    if(bus_cmd_mesi == BusRdX){
                        Tsram[i][block_number][0] = '1'; // modified
                        Tsram[i][block_number][1] = '1';
                        continue;
                    }
                }
                if((bus_origid_mesi == i) && bus_sharing == 1){
                    continue;
                }

                for(j=2;j<15;j++){ //check if both tags are equal.
                    if(Tsram[i][block_number][j] != Tsram[bus_origid_mesi][block_number][j]){
                        not_equals = 1; 
                        break;
                    }
                }
                if(not_equals){
                    continue;
                }
                if(sharing_block[i] == 1){
                    
                    if((Tsram[i][block_number][0] == '1')  && (Tsram[i][block_number][1] == '1')){ // if the messi_state is modified
                        who_flush = i;
                        if(bus_cmd_mesi == BusRd){ 
                            Tsram[i][block_number][0] = '0'; // modified --> shared
                            Tsram[bus_origid_mesi][block_number][0] = '0'; // change state to shared of the origid core
                            Tsram[bus_origid_mesi][block_number][1] = '1';
                            
                        }
                        if(bus_cmd_mesi == BusRdX){
                            Tsram[i][block_number][0] = '0'; // modified --> invalid
                            Tsram[i][block_number][1] = '0';
                            Tsram[bus_origid_mesi][block_number][0] = '1';// change state to modified of the origid core
                            Tsram[bus_origid_mesi][block_number][1] = '1';
                        }
                        bus_origid_mesi = who_flush;
                        bus_cmd_mesi = Flush;
                        bus_busy = 1;
                        return;
                    }
                    else{ //bus_shared and not modified then the block is exclusive or shared
                        if(bus_cmd_mesi == BusRd){// changing state to shared
                            Tsram[i][block_number][0] = '0'; // exclusive or shared --> shared
                            Tsram[i][block_number][1] = '1';
                            Tsram[bus_origid_mesi][block_number][0] = '0'; // change state to shared of the origid core
                            Tsram[bus_origid_mesi][block_number][1] = '1';
                        }
                        if(bus_cmd_mesi == BusRdX){// changing state to invalid
                            Tsram[i][block_number][0] = '0';// exclusive or shared --> invalid
                            Tsram[i][block_number][1] = '0';
                            Tsram[bus_origid_mesi][block_number][0] = '1';// change state to modified of the origid core
                            Tsram[bus_origid_mesi][block_number][1] = '1';
                        }
                    }
                }
                
            }
            tag_num = bus_address_mesi / pow(2,8);
            itoa(tag_num, bin, 2);
            for(i=0; i<12; i++){
                if(bin[i] == '\0'){
                    tag_size = i;
                    break;
                }
            }

            for(i=0; i<12; i++){
                if(i<12-tag_size){
                    Tsram[bus_origid_mesi][block_number][i+2] = '0';
                }
                else{
                    Tsram[bus_origid_mesi][block_number][i+2] = bin[i-(12-tag_size)];
                }
            }

            Tsram[bus_origid_mesi][block_number][i+2] = '\0';
            bus_origid_mesi = who_flush;
            bus_cmd_mesi = Flush;
            bus_busy = 1;
            return;
        }
        
        
    }
    bus_address_mesi = (bus_address_mesi >> 2) << 2;
    block_number = (bus_address_mesi/4)%NUMBER_BLOCKS;
    if(bus_cmd_mesi == Flush){
        bus_busy = 1;
        who_flush = bus_origid_mesi;
        bus_counter++;
        if(bus_counter>0 && bus_counter<5){ // the data is shared ,then we start giving the words from the first cycle
            if(bus_sharing == 1 && who_flush != 4){
                strcpy(Dsram[who_needs][block_number*4 + bus_counter - 1], Dsram[who_flush][block_number*4 + bus_counter - 1]);
                fprintf(bus_trace_file,"%d ",cycles[who_needs]);
                fprintf(bus_trace_file,"%X ",bus_origid_mesi);
                fprintf(bus_trace_file,"%X ",bus_cmd_mesi);
                fprintf(bus_trace_file,"%05X ",bus_address_mesi + bus_counter - 1);
                fprintf(bus_trace_file,"%s ",Dsram[who_needs][bus_address_mesi + bus_counter - 1]);
                fprintf(bus_trace_file,"1\n");
                
            }
        }
        if(bus_counter >= 16 && bus_counter < 20){ // the data was not shared (giving the core)
            
            if(who_flush == 4 && who_needs != 4 && bus_counter < 20){ // the data was not shared (giving the core)
                strcpy(Dsram[who_needs][block_number*4 + bus_counter - 16], memout[bus_address_mesi + bus_counter - 16]);
                fprintf(bus_trace_file,"%d ",cycles[who_needs]);
                fprintf(bus_trace_file,"%X ",bus_origid_mesi);
                fprintf(bus_trace_file,"%X ",bus_cmd_mesi);
                fprintf(bus_trace_file,"%05X ",bus_address_mesi + bus_counter - 16);
                fprintf(bus_trace_file,"%s ",Dsram[who_needs][block_number*4 + bus_counter - 16]);
                if(bus_sharing == 1){
                    fprintf(bus_trace_file,"1\n");
                }
                else{
                    fprintf(bus_trace_file,"0\n");
                }
            }
            if(who_flush != 4){ // main memory not flushing then we give it data
                strcpy(memout[bus_address_mesi + bus_counter - 16], Dsram[who_flush][block_number*4 + bus_counter - 16]);
                if(who_needs == 4 && we_finished == 0){
                    fprintf(bus_trace_file,"%d ",cycles[who_flush]);
                    fprintf(bus_trace_file,"%X ",bus_origid_mesi);
                    fprintf(bus_trace_file,"%X ",bus_cmd_mesi);
                    fprintf(bus_trace_file,"%05X ",bus_address_mesi + bus_counter - 16);
                    fprintf(bus_trace_file,"%s ",Dsram[who_flush][bus_address_mesi + bus_counter - 16]);
                    if(bus_sharing == 1){
                        fprintf(bus_trace_file,"1\n");
                    }
                    else{
                        fprintf(bus_trace_file,"0\n");
                    }
                }
                if(need_flush[who_flush] == 1){ // converting the mesi state to invalid
                    need_flush[who_flush] = 0;
                    Tsram[who_flush][block_number][0] = '0';
                    Tsram[who_flush][block_number][1] = '0';
                    read_address = pipe_regs[who_flush][4].ALU_pipe;
                    tag_num = read_address / pow(2,8);
                    itoa(tag_num, bin, 2);
                    for(i=0; i<12; i++){
                        if(bin[i] == '\0'){
                            tag_size = i;
                            break;
                        }
                    }

                    for(i=0; i<12; i++){
                        if(i<12-tag_size){
                            Tsram[bus_origid_mesi][block_number][i+2] = '0';
                        }
                        else{
                            Tsram[bus_origid_mesi][block_number][i+2] = bin[i-(12-tag_size)];
                        }  
                    }

                    Tsram[bus_origid_mesi][block_number][i+2] = '\0';
                }
            }

        }

        if((bus_counter == 20) || (bus_counter == 5 && bus_sharing == 1)){ //we gave all the words needed and returning to the default settings
                    if(who_needs != 4){
                        core_ready[who_needs] = 1;
                        flag_mem_stall[who_needs] = 0;
                    }
                    else{
                        alredy_enqueued[who_flush] = 0;
                    }
                    if(bus_counter == 20){
                        bus_counter = 0;
                        bus_busy = 0;
                        bus_sharing = -1;
                        who_flush = 4;
                    }
                    bus_shared[0] = 0;
                    bus_shared[1] = 0;
                    bus_shared[2] = 0;
                    bus_shared[3] = 0;
                    sharing_block[0] = 0;
                    sharing_block[1] = 0;
                    sharing_block[2] = 0;
                    sharing_block[3] = 0;
                    who_needs = 4;
                    
        }
    }
}

int is_branch(char *command){
    if(command[0] == '0' && (command[1]=='9' || command[1]=='a' || command[1]=='A' || command[1]=='b'|| command[1]=='B' || command[1]=='c' || command[1]=='C' || command[1]=='d' || command[1]=='D' || command[1]=='e' || command[1]=='E' || command[1]=='f' || command[1]=='F'))
        return 1;
    else
        return 0;
}
void int_to_hex(int x, char * hex){
    snprintf(hex, 9, "%08X", x);
}
int hex_to_int(char * hex){
    return ((int)strtol(hex, NULL, 16));
}
void bin_to_hex(char *bin, char *hex) {
    int binLen = strlen(bin);

    // Step 1: Convert binary string to an integer
    int number = 0;
    for (int i = 0; i < binLen; i++) {
        if (bin[i] == '1') {
            number = (number << 1) | 1; // Shift left and set the least significant bit
        } else if (bin[i] == '0') {
            number = number << 1; // Shift left
        } else {
            printf("Invalid binary input.\n");
            return;
        }
    }

    // Step 2: Convert integer to hexadecimal, ensuring 8-character length
    snprintf(hex, 9, "%08X", number);
}

void hex_to_bin(char *bin,char*hex)    { // Lookup table to convert hex digits to binary strings
    int i;
    char *bin_lookup[] = {
        "0000", "0001", "0010", "0011", 
        "0100", "0101", "0110", "0111", 
        "1000", "1001", "1010", "1011", 
        "1100", "1101", "1110", "1111"
    };

    int binIndex = 0;

    // Iterate over each hex digit and map to binary
    for (i = 0; i < (int)strlen(hex); i++) {
        char hexChar = hex[i];
        int value;

        // Determine the value of the hex character
        if (hexChar >= '0' && hexChar <= '9') {
            value = hexChar - '0';
        } else if (hexChar >= 'A' && hexChar <= 'F') {
            value = hexChar - 'A' + 10;
        } else if (hexChar >= 'a' && hexChar <= 'f') {
            value = hexChar - 'a' + 10;
        } else {
            printf("Invalid hexadecimal character: %c\n", hexChar);
            bin[0] = '\0'; // Set the binary output to an empty string
            return;
        }

        // Append the binary representation of the hex value
        strcpy(&bin[binIndex], bin_lookup[value]);
        binIndex += 4; // Each hex digit corresponds to 4 binary digits
    }

    // Null-terminate the binary string
    bin[binIndex] = '\0';
}

void fetch (int i){ // Takes pc[i] and the instruction of the pc and put pc++ and the instruction in pipe_regs[i][1] .
    if(the_end[i] == 1){
        fprintf(core_trace_files[i], "--- ");
        pipe_regs[i][1].pc_pipe = -2;
        return;

    }
    if(the_end[i] == 2){
        fprintf(core_trace_files[i], "--- ");
        pipe_regs[i][1].pc_pipe = -2;
        return;
    }
    int h = pc[i];
    if(h == -1){
        fprintf(core_trace_files[i], "--- ");
        pipe_regs[i][1].pc_pipe = -2; // -2 = halt
        return;
    }
    
    pipe_regs[i][1].pc_pipe=h;
    switch (i) {
        case 0:
            for (int k=0;k<LINE_LENGTH;k++){
                pipe_regs[i][1].inst[k]=Imem0[pc[i]][k];
                if(pipe_regs[i][1].inst[0] == '1' && pipe_regs[i][1].inst[1] == '4'){
                    pipe_regs[i][1].pc_pipe = h;
                }

            }
            break;
        case 1: // must deal with halt
            for (int k=0;k<LINE_LENGTH;k++){
                pipe_regs[i][1].inst[k]=Imem1[pc[i]][k];

            }
            break;
        case 2:
            for (int k=0;k<LINE_LENGTH;k++){
                pipe_regs[i][1].inst[k]=Imem2[pc[i]][k];

            }
            break;
        case 3:
            for (int k=0;k<LINE_LENGTH;k++){
                pipe_regs[i][1].inst[k]=Imem3[pc[i]][k];

            }
            break;
    }   
    fprintf(core_trace_files[i], "%03X ", h); //printing the pc in fetch
    pre_branch_pc[i] = pc[i];
    pc[i]++;     
}
//helper func for alu that do an arithmetic shift right
int arithmetic_shift_right(int num, int shift)
{
	// Check if the shift amount is greater than or equal to 32
	if (shift >= 32) {
		// If the number's sign bit is set, return all 1s (sign-extended)
		// Otherwise, return all 0s
		return (num & 0x80000000) ? 0xFFFFFFFF : 0;
	}
	else if (num & 0x80000000) {
		// If the number's sign bit is set, perform an arithmetic right shift with sign extension
		return (num >> shift) | ~( 0xFFFFFFFF >> shift);
	}
	else {
		// If the number is positive, perform a logical right shift
		return num >> shift;
	}
}
void alu (int i){ // Takes opcode , rs and rd (from pipe_regs[i][2]) and implement the nine options of the alu instruction according ti the opcode (according to the table given in the project page 4) .
                //And the only change that save is the output of the alu (ALU_PIPE) in pipe_regs[i][5] .
    if(pipe_regs[i][2].op == 20){
        fprintf(core_trace_files[i], "%03X ", pipe_regs[i][2].pc_pipe);
        pipe_regs[i][5].op = pipe_regs[i][2].op;
        pipe_regs[i][5].pc_pipe = pipe_regs[i][2].pc_pipe;
        return;
    }
    if(pc[i] == -1 && pipe_regs[i][2].pc_pipe == -2){ // reached halt
        fprintf(core_trace_files[i], "--- ");
        pipe_regs[i][5].op = -1;
        pipe_regs[i][5].pc_pipe = -2;
        return;
    }
    if(pipe_regs[i][2].pc_pipe == -1){
        fprintf(core_trace_files[i], "--- ");
        pipe_regs[i][5].pc_pipe = -1;
        return;
    }
    
    int optag = pipe_regs[i][2].op;
    int rstag = pipe_regs[i][2].rs;
    int rttag = pipe_regs[i][2].rt;
    switch (optag){
        case 0:
            pipe_regs[i][5].ALU_pipe= rstag + rttag;
            break;
        case 1:
            pipe_regs[i][5].ALU_pipe = rstag - rttag;
            break;
        case 2:
            pipe_regs[i][5].ALU_pipe = rstag & rttag;
            break;
        case 3:
            pipe_regs[i][5].ALU_pipe = rstag | rttag;
            break;
        case 4:
            pipe_regs[i][5].ALU_pipe = rstag ^ rttag;
            break;
        case 5:
            pipe_regs[i][5].ALU_pipe = rstag * rttag;
            break;
        case 6:
            if(rttag>=32){
                pipe_regs[i][5].ALU_pipe = 0;
            }
            else{
                pipe_regs[i][5].ALU_pipe = rstag << rttag;
            }    
            break;
        case 7:
            pipe_regs[i][5].ALU_pipe=arithmetic_shift_right(rstag, rttag);
            break;
        case 8:
            if(rttag>=32){
                pipe_regs[i][5].ALU_pipe = 0;
            }
            else{
                pipe_regs[i][5].ALU_pipe=rstag >> rttag;
            }    
            break;
	case 16:
            pipe_regs[i][5].ALU_pipe=rstag + rttag;
            break;
	case 17:
            pipe_regs[i][5].ALU_pipe = rstag + rttag;
            break;
    }

    pipe_regs[i][5].pc_pipe = pipe_regs[i][2].pc_pipe;
    strcpy(pipe_regs[i][5].inst, pipe_regs[i][2].inst);
    pipe_regs[i][5].op = pipe_regs[i][2].op;
    pipe_regs[i][5].rd = pipe_regs[i][2].rd;
    pipe_regs[i][5].rs = pipe_regs[i][2].rs;
    pipe_regs[i][5].rt = pipe_regs[i][2].rt;
    pipe_regs[i][5].imm = pipe_regs[i][2].imm;
    pipe_regs[i][5].dist = pipe_regs[i][2].dist;

    fprintf(core_trace_files[i], "%03X ", pipe_regs[i][2].pc_pipe);
}
void wb (int i){ //Takes the opcode , destination , output of alu and data from memory (from pipe_regs[i][6]) and according to the opcode he put the correct result in the destination register .
    int c;
    if(pipe_regs[i][6].op == 20){
        fprintf(core_trace_files[i], "%03X ", pipe_regs[i][6].pc_pipe);
        for(c=2; c<=14; c++){
        fprintf(core_trace_files[i], "%08X ",regs[i][c]);
        }
        fprintf(core_trace_files[i], "%08X \n",regs[i][15]);
        instructions[i] += 1;
        return;
    }

    if(pipe_regs[i][6].pc_pipe == -1){
        fprintf(core_trace_files[i],"--- ");
        for(c=2; c<=14; c++){
        fprintf(core_trace_files[i], "%08X ",regs[i][c]);
        }
        fprintf(core_trace_files[i], "%08X \n",regs[i][15]);
        return;
    }

    fprintf(core_trace_files[i], "%03X ", pipe_regs[i][6].pc_pipe);
    instructions[i] += 1;

    for(c=2; c<=14; c++){
        fprintf(core_trace_files[i], "%08X ",regs[i][c]);
    }
    fprintf(core_trace_files[i], "%08X \n",regs[i][15]);

    int optag = pipe_regs[i][6].op;
    int disttag = pipe_regs[i][6].dist;//the index of the dist register(rd)
    int ALU_pipetag = pipe_regs[i][6].ALU_pipe;//outp of the alu
    int datatag = pipe_regs[i][6].data;// data from memory
    if (optag==0 || optag==1 || optag==2 || optag==3 || optag==4 || optag==5 || optag==6 || optag==7 || optag==8) {
        regs[i][disttag]=ALU_pipetag;
        reg_is_available[i][disttag] = 1;

    }
    if(optag == 15){
        regs[i][15] = pipe_regs[i][6].pc_pipe;
        reg_is_available[i][15] = 1;
    }
    else if (optag==16){
        regs[i][disttag]=datatag;
        reg_is_available[i][disttag] = 1;
    }


}

void slice_inst(char * input, int output[]) { // add int sliced_inst[5];

    // Convert hexadecimal string input to integer
    uint32_t instruction = (uint32_t)strtol(input, NULL, 16);

    output[0] = (instruction >> 24) & 0xFF;       // Bits 31:24 (op)
    output[1] = (instruction >> 20) & 0x0F;       // Bits 23:20 (rd)
    output[2] = (instruction >> 16) & 0x0F;       // Bits 19:16 (rs)
    output[3] = (instruction >> 12) & 0x0F;       // Bits 15:12 (rt)
    output[4] = instruction & 0x0FFF;             // Bits 11:0 (imm)
}


void decode(int i){
    if(the_end[i] == 1 || the_end[i] == 2){
        fprintf(core_trace_files[i], "--- ");
        pipe_regs[i][3].op = -1;
        pipe_regs[i][3].pc_pipe = -2;
        the_end[i] = 2;
        return;
    }
    if(pipe_regs[i][0].inst[0] == '1' && pipe_regs[i][0].inst[1] == '4'){
        fprintf(core_trace_files[i], "%03X ", pipe_regs[i][0].pc_pipe);
        pipe_regs[i][3].op = 20;
        pipe_regs[i][3].pc_pipe = pipe_regs[i][0].pc_pipe;
        pc[i] = -1;
        the_end[i] = 1;
        return;
    }
    if(pc[i] == -1 && pipe_regs[i][0].pc_pipe == -2){ // reached halt
        pipe_regs[i][3].pc_pipe = -2;
        fprintf(core_trace_files[i], "--- ");
        return;
    }
    if(pipe_regs[i][0].pc_pipe == -1){
        fprintf(core_trace_files[i], "--- ");
        return;
    }

    fprintf(core_trace_files[i], "%03X ",pipe_regs[i][0].pc_pipe);
    int sliced_inst[5] = {0};
    int reg_d = 0;
    int reg_s = 0;
    int reg_t = 0;
    int my_imm = 0;
    //should each of these be done on each attribute {op,rd,rs,rt...?}
    slice_inst(pipe_regs[i][0].inst, sliced_inst);

    if(sliced_inst[0] <=8 || sliced_inst[0] ==16){ // rd is not a source reg
        if(!(reg_is_available[i][sliced_inst[2]] && reg_is_available[i][sliced_inst[3]])){
            if(reg_is_available[i][sliced_inst[0]] == 17){ // if op is sw we should check if rd is available
                if(!reg_is_available[i][sliced_inst[1]]){
                    //decode_stall[i] ++;
                    flag_decode_stall[i] = 1;
                    pipe_regs[i][3].pc_pipe = -1;
                    return;
                }
            }
            //decode_stall[i] ++;
            flag_decode_stall[i] = 1;
            pipe_regs[i][3].pc_pipe = -1;
            return;
        }
    }
    else {
        if((!(reg_is_available[i][sliced_inst[1]] )) || (!(reg_is_available[i][sliced_inst[2]])) || (!(reg_is_available[i][sliced_inst[3]]))){
            //decode_stall[i] ++;
            flag_decode_stall[i] = 1;
            pipe_regs[i][3].pc_pipe = -1;
            return;
        }
    }

    flag_decode_stall[i] = 0; // all source regs are ready
    //pc_pipe
    pipe_regs[i][3].pc_pipe = pipe_regs[i][0].pc_pipe;
    //inst
    for (int k=0;k<LINE_LENGTH;k++){
        pipe_regs[i][3].inst[k]=pipe_regs[i][0].inst[k];
    }
    
    pipe_regs[i][3].op = sliced_inst[0];
    
    my_imm = sliced_inst[4];

    //rd
    if(sliced_inst[1] == 1){
        pipe_regs[i][3].rd = my_imm;
        reg_d = my_imm;
    }
    else{
        reg_d = regs[i][sliced_inst[1]];
        pipe_regs[i][3].rd = reg_d;// might need to be converted to decimal from hex
    }

    //rs
    if(sliced_inst[2] == 1){
        pipe_regs[i][3].rs = my_imm;
        reg_s = my_imm;
    }
    else{
        reg_s = regs[i][sliced_inst[2]];
        pipe_regs[i][3].rs = reg_s;// might need to be converted to decimal from hex
    }

    //rt
    if(sliced_inst[3] == 1){
        pipe_regs[i][3].rt = my_imm;
        reg_t = my_imm;
    }
    else{
        reg_t = regs[i][sliced_inst[3]];
        pipe_regs[i][3].rt = reg_t;// might need to be converted to decimal from hex
    }

    //imm
    pipe_regs[i][3].imm = my_imm;// might need to be converted to decimal from hex
    //dist 
    pipe_regs[i][3].dist = sliced_inst[1];
    


    //branching 

    if(is_branch(pipe_regs[i][0].inst)){
        switch(sliced_inst[0]){
            case 9:
                if(reg_s == reg_t){
                    pc[i] = (reg_d & 0x3FF);
                }
                return;
            
            case 10:
                if(reg_s != reg_t){
                    pc[i] = (reg_d & 0x3FF);
                }
                return;

            case 11:
                if(reg_s < reg_t){
                    pc[i] = (reg_d & 0x3FF);
                }
                return;
                
            case 12:
                if(reg_s > reg_t){
                    pc[i] = (reg_d & 0x3FF);
                }
                return;

            case 13:
                    if(reg_s <= reg_t){
                    pc[i] = (reg_d & 0x3FF);
                }
                return;

            case 14:
                if(reg_s >= reg_t){
                    pc[i] = (reg_d & 0x3FF);
                }
                return;

            case 15:
                reg_is_available[i][15] = 0;
                pc[i] =  (reg_d & 0x3FF);
                return;

        }
    }
    if((sliced_inst[0] >=0 && sliced_inst[0] <=8) || sliced_inst[0] ==16){ // making the distenation reg not available for decoding next instructions.
        reg_is_available[i][sliced_inst[1]] = 0;
    }
}    
