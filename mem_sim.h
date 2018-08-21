//
// Created by ofir on 21/8/18.
//

#ifndef EX5_MEM_SIM_H
#define EX5_MEM_SIM_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pwd.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <assert.h>
#include <math.h>

#define PAGE_SIZE 5
#define NUM_OF_PAGES 25
#define MEMORY_SIZE 20


#define NO_PLACE_AVAILABLE -2
#define FRAME_SIZE 5

typedef struct page_descriptor
{
    unsigned int V; // valid
    unsigned int D; // dirty
    unsigned int P; // permission
    int frame; //the number of a frame if in case it is page-mapped
} page_descriptor;


struct sim_database
{
    page_descriptor page_table[NUM_OF_PAGES]; //pointer to page table
    int swapfile_fd; //swap file fd
    int program_fd; //executable file fd
    char main_memory[MEMORY_SIZE];
    int text_size;
    int data_bss_size;
    int heap_stack_size;
};


struct sim_database* init_system(char exe_file_name[], char swap_file_name[] , int text_size, int data_bss_size, int heap_stack_size );

char load(struct sim_database *mem_sim, int address);

void store(struct sim_database *mem_sim, int address, char value);

void clear_system(struct sim_database* mem_sim);

void getFromExetuable(struct sim_database *mem_sim, int page,char temp[]);

int placeAvailable(int index_to_add);

void move_to_swap(struct sim_database *mem_sim, int page);

char return_char(struct sim_database *mem_sim, int offset, int page);

void replace_char(struct sim_database *mem_sim, int offset, int page, char value);

void insert_to_main_memory(struct sim_database *mem_sim, char *copy, int page);

char* read_from_swap(struct sim_database *mem_sim, int page);

void clear_page_v_f(struct sim_database *mem_sim);

int check_address(struct sim_database *mem_sim ,int address);


void print_memory(struct sim_database* mem_sim);
void print_swap(struct sim_database* mem_sim);
void print_page_table(struct sim_database* mem_sim);



#endif //EX5_MEM_SIM_H
