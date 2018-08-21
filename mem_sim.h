/* NAME: Ofir Cohen
 * ID: 312255847
 * DATE: 21/6/2018
 *
 * This program simulates access to the physical memory while using paging mechanism
 */

#ifndef EX5_MEM_SIM_H
#define EX5_MEM_SIM_H

#define PAGE_SIZE 5
#define NUM_OF_PAGES 25
#define MEMORY_SIZE 20

#define VALID 1
#define INVALID 0
#define P_READ 0
#define P_READ_WRITE 1
#define DIRTY 1
#define NO_DIRTY 0
#define ALL_TAKEN -1
#define FRAME_SIZE 5



typedef struct page_descriptor
{
    unsigned int V;         // valid
    unsigned int D;         // dirty
    unsigned int P;         // permission
    int frame;              //the number of a frame if in case it is page-mapped
} page_descriptor;


struct sim_database
{
    page_descriptor page_table[NUM_OF_PAGES];       //pointer to page table
    int swapfile_fd;                                //swap file fd
    int program_fd;                                 //executable file fd
    char main_memory[MEMORY_SIZE];
    int text_size;
    int data_bss_size;
    int heap_stack_size;
};


struct sim_database* init_system(char exe_file_name[], char swap_file_name[] , int text_size, int data_bss_size, int heap_stack_size);
char load(struct sim_database* mem_sim , int address);
void store(struct sim_database* mem_sim , int address, char value);
void print_memory(struct sim_database* mem_sim);
void print_swap (struct sim_database* mem_sim);
void print_page_table(struct sim_database* mem_sim);
void clear_system(struct sim_database* mem_sim);
int is_available(int next);
void get_from_exec(struct sim_database* mem_sim, int page, char temp[]);
char* get_from_swap(struct sim_database* mem_sim, int page);
void to_swap(struct sim_database* mem_sim, int page);
char giveback_val(struct sim_database* mem_sim, int offset, int page);
void replace_val(struct sim_database* mem_sim, int offset, int page, char value);
void insert_to_mem(struct sim_database* mem_sim, char* copy, int page);
void reset_v_f(struct sim_database* mem_sim);
int valid_address(struct sim_database* mem_sim, int address);
void error(char* msg);

#endif //EX5_MEM_SIM_H

