/* NAME: Ofir Cohen
 * DATE: 21/6/2018
 *
 * This program simulates access to the physical memory while using paging mechanism
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "mem_sim.h"


int free_frame[MEMORY_SIZE / PAGE_SIZE];
int frame_counter = 0;


struct sim_database* init_system(char exe_file_name[], char swap_file_name[] , int text_size, int data_bss_size, int heap_stack_size)
{
    if(exe_file_name == NULL)
    {
        error("execute file does not exist");
        exit(EXIT_FAILURE);
    }

    struct sim_database* mem_sim = (struct sim_database*)malloc(sizeof(struct sim_database));
    if(mem_sim == NULL)
        error("Could not allocate memory");

    if((mem_sim->program_fd = open(exe_file_name, O_RDONLY)) == -1)
    {
        error("cannot open execute file");
        clear_system(mem_sim);
        exit(EXIT_FAILURE);
    }

    if((mem_sim->swapfile_fd = open(swap_file_name, O_RDWR | O_CREAT, 0777)) == -1)
    {
        error("cannot open swap file");
        clear_system(mem_sim);
    }

    mem_sim->text_size = text_size;
    mem_sim->data_bss_size = data_bss_size;
    mem_sim->heap_stack_size = heap_stack_size;

    int i;
    for(i = 0; i < text_size / PAGE_SIZE; i++)
    {
        mem_sim->page_table[i].V = INVALID;
        mem_sim->page_table[i].D = NO_DIRTY;
        mem_sim->page_table[i].P = P_READ;
        mem_sim->page_table[i].frame = -1;
    }

    while(i < NUM_OF_PAGES)
    {
        mem_sim->page_table[i].V = INVALID;
        mem_sim->page_table[i].D = NO_DIRTY;
        mem_sim->page_table[i].P = P_READ_WRITE;
        mem_sim->page_table[i].frame = -1;
        i++;
    }

    for(i = 0; i < MEMORY_SIZE; i++)
        mem_sim->main_memory[i] = '0';

    char char_to_swap = '0';

    for(i = 0; i < text_size + data_bss_size + heap_stack_size; i++)
    {
        if(lseek(mem_sim->swapfile_fd, i, SEEK_SET) == -1)
        {
            error("lseek failed");
        }

        if(write(mem_sim->swapfile_fd, &char_to_swap, sizeof(char)) == -1)
        {
            error("write failed");
        }
    }

    return mem_sim;
}


char load(struct sim_database* mem_sim , int address)
{
    if(valid_address(mem_sim, address) == -1)
        return '\0';

    int page = address / PAGE_SIZE;
    int offset = address % PAGE_SIZE;

    if(mem_sim->page_table[page].V == VALID)
    {
        int place = mem_sim->page_table[page].frame * PAGE_SIZE + offset;
        return mem_sim->main_memory[place];
    }

        /*Valid is 0*/
    else
    {
        //Permission is read only
        if(mem_sim->page_table[page].P == P_READ)
        {
            char* buff;
            char tmp[PAGE_SIZE];
            get_from_exec(mem_sim, page, tmp);
            buff = tmp;

            //there is a free frame in physical memory
            if(is_available(frame_counter) != ALL_TAKEN)
                insert_to_mem(mem_sim, buff, page);

                //physical memory is full
            else
            {
                int index = free_frame[frame_counter];

                //page has not been modified, D == 0
                if(mem_sim->page_table[index].D == NO_DIRTY)
                {
                    reset_v_f(mem_sim);
                    insert_to_mem(mem_sim, buff, page);
                }

                    //page has been modified, D == 1
                else
                {
                    to_swap(mem_sim, index);
                    reset_v_f(mem_sim);
                    insert_to_mem(mem_sim, buff, page);
                }
            }
            return giveback_val(mem_sim, offset, page);
        }

            /*Permission is READ AND WRITE*/
        else
        {
            //page has not been modified
            if(mem_sim->page_table[page].D == NO_DIRTY)
            {
                char* buff;

                //address is in heap stack place so we need to create new frame
                if(address > mem_sim->text_size + mem_sim->data_bss_size)
                {
                    char block[FRAME_SIZE] = {'\0'};
                    buff = block;
                }

                    //address is in text or data bss
                else
                {
                    char tmp[FRAME_SIZE];
                    get_from_exec(mem_sim, page, tmp);
                    buff = tmp;
                }

                //there is an available frame in main memory
                if(is_available(frame_counter) != ALL_TAKEN)
                    insert_to_mem(mem_sim, buff, page);

                    //physical memory is full, we need to move to swap or override
                else
                {
                    int index = free_frame[frame_counter];

                    if(mem_sim->page_table[index].D == DIRTY)
                        to_swap(mem_sim, page);

                    reset_v_f(mem_sim);
                    insert_to_mem(mem_sim, buff, page);
                }

                return giveback_val(mem_sim, offset, page);
            }

                //D == 1, page has been modified
            else
            {
                int index = free_frame[frame_counter];
                if(mem_sim->page_table[index].D == 1)
                    to_swap(mem_sim, index);

                char* data_from_swap = get_from_swap(mem_sim, page);
                reset_v_f(mem_sim);
                insert_to_mem(mem_sim, data_from_swap, page);
                return giveback_val(mem_sim, offset, page);
            }
        }
    }
}


void store(struct sim_database* mem_sim , int address, char value)
{
    if(valid_address(mem_sim, address) == -1)
        return;


    int page = address / PAGE_SIZE;
    int offset = address % PAGE_SIZE;

    if(address < mem_sim->text_size)
    {
        error("cannot change text section");
    }

    if(mem_sim->page_table[page].V == VALID)
    {
        int place = mem_sim->page_table[page].frame * PAGE_SIZE + offset;
        mem_sim->main_memory[place] = value;
        mem_sim->page_table[free_frame[frame_counter]].D = DIRTY;
    }

        /*Valid is 0*/
    else
    {
        if(mem_sim->page_table[page].P == P_READ)
        {
            char tmp[PAGE_SIZE];
            get_from_exec(mem_sim, page, tmp);
            char* buff = tmp;

            //there is a free frame in physical memory
            if(is_available(frame_counter) != ALL_TAKEN)
            {
                insert_to_mem(mem_sim, buff, page);
                replace_val(mem_sim, offset, page, value);
            }

                //physical memory is full
            else
            {
                int index = free_frame[frame_counter];

                if(mem_sim->page_table[index].D == NO_DIRTY)
                {
                    reset_v_f(mem_sim);
                    insert_to_mem(mem_sim, buff, page);
                    replace_val(mem_sim, offset, page, value);
                }

                else
                {
                    to_swap(mem_sim, page);
                    reset_v_f(mem_sim);
                    insert_to_mem(mem_sim, buff, page);
                    replace_val(mem_sim, offset, page, value);
                }
            }
        }

            /*Permission is READ AND WRITE*/
        else
        {
            if(mem_sim->page_table[page].D == NO_DIRTY)
            {
                char* buff;

                //address is in heap stack place so we need to create new frame
                if(address > mem_sim->text_size + mem_sim->data_bss_size)
                {
                    char block[FRAME_SIZE] = {'\0'};
                    buff = block;
                }

                else
                {
                    char tmp[FRAME_SIZE];
                    get_from_exec(mem_sim, page, tmp);
                    buff = tmp;
                }

                if(is_available(frame_counter) != ALL_TAKEN)
                {
                    insert_to_mem(mem_sim, buff, page);
                    replace_val(mem_sim, offset, page, value);
                }

                else
                {
                    int index = free_frame[frame_counter];

                    if(mem_sim->page_table[index].D == DIRTY)
                        insert_to_mem(mem_sim, buff, page);

                    reset_v_f(mem_sim);
                    insert_to_mem(mem_sim, buff, page);
                    replace_val(mem_sim, offset, page, value);
                }
            }

            else
            {
                int index = free_frame[frame_counter];
                if(mem_sim->page_table[index].D == DIRTY)
                    to_swap(mem_sim, index);

                char* data_from_swap = get_from_swap(mem_sim, page);
                reset_v_f(mem_sim);
                insert_to_mem(mem_sim, data_from_swap, page);
                replace_val(mem_sim, offset,page, value);
            }
        }
    }
}


void clear_system(struct sim_database* mem_sim)
{
    close(mem_sim->program_fd);
    close(mem_sim->swapfile_fd);
    free(mem_sim);
}


void print_memory(struct sim_database * mem_sim)
{
    int i;
    printf("\n Physical memory\n");
    for(i = 0; i < MEMORY_SIZE; i++)
        printf("[%c]\n", mem_sim->main_memory[i]);
}


void print_swap(struct sim_database * mem_sim)
{
    char str[PAGE_SIZE];
    int i;
    printf("\n Swap memory\n");
    lseek(mem_sim->swapfile_fd, 0, SEEK_SET);       //to go to the start of the file
    while(read(mem_sim->swapfile_fd, str, PAGE_SIZE) == PAGE_SIZE)
    {
        for(i = 0; i < PAGE_SIZE; i++)
            printf("[%c]\t", str[i]);

        printf("\n");
    }
}


void print_page_table(struct sim_database* mem_sim)
{
    int i;
    printf("\n page table \n");
    printf("Valid\t Dirty\t Permission\t Frame\n");
    for(i = 0; i < NUM_OF_PAGES; i++)
        printf("[%d]\t[%d]\t[%d]\t[%d]\n", mem_sim->page_table[i].V,
               mem_sim->page_table[i].D, mem_sim->page_table[i].P, mem_sim->page_table[i].frame);
}


void get_from_exec(struct sim_database* mem_sim, int page, char temp[])
{
    if(lseek(mem_sim->program_fd, page * PAGE_SIZE, SEEK_SET) == -1)
        error("lseek failed");

    if(read(mem_sim->program_fd, temp, sizeof(char)*PAGE_SIZE) == -1)
        error("read failed");

}


char* get_from_swap(struct sim_database* mem_sim, int page)
{
    char temp[FRAME_SIZE];

    if(lseek(mem_sim->program_fd, page * PAGE_SIZE, SEEK_SET) == -1)
        error("lseek failed");

    if(read(mem_sim->swapfile_fd, temp, sizeof(char)*PAGE_SIZE) == -1)
        error("read failed");

    char* ptr = temp;
    return ptr;
}


void to_swap(struct sim_database* mem_sim, int page)
{
    char block[FRAME_SIZE];

    int i;
    for(i = 0; i < FRAME_SIZE; i++)
        block[i] = mem_sim->main_memory[frame_counter * FRAME_SIZE + i];

    if(lseek(mem_sim->swapfile_fd, page * PAGE_SIZE, SEEK_SET) == -1)
        error("lseek failed");

    if(write(mem_sim->swapfile_fd, block, sizeof(char) * FRAME_SIZE) == -1)
        error("write failed");

    mem_sim->page_table[free_frame[frame_counter]].D = DIRTY;
}


char giveback_val(struct sim_database* mem_sim, int offset, int page)
{
    int place = mem_sim->page_table[page].frame * PAGE_SIZE + offset;
    return mem_sim->main_memory[place];
}


void replace_val(struct sim_database* mem_sim, int offset, int page, char value)
{
    int place = mem_sim->page_table[page].frame * PAGE_SIZE + offset;
    mem_sim->main_memory[place] = value;
    mem_sim->page_table[page].D = DIRTY;
}


void insert_to_mem(struct sim_database* mem_sim, char* copy, int page)
{
    int i;
    for(i = 0; i < FRAME_SIZE; i++)
        mem_sim->main_memory[(FRAME_SIZE * frame_counter) + i] = copy[i];

    free_frame[frame_counter] = page;
    mem_sim->page_table[free_frame[frame_counter]].V = VALID;
    mem_sim->page_table[free_frame[frame_counter]].frame = frame_counter;
    frame_counter++;

    if(frame_counter == MEMORY_SIZE / FRAME_SIZE)
        frame_counter = 0;
}


int is_available(int next)
{
    if(free_frame[frame_counter] == -1)
        return 1;

    return ALL_TAKEN;
}


int valid_address(struct sim_database* mem_sim, int address)
{
    int last = mem_sim->text_size + mem_sim->data_bss_size + mem_sim->heap_stack_size;

    if(address < 0 || address > last)
    {
        error("invalid address");
        return -1;
    }

    return 1;
}


void reset_v_f(struct sim_database* mem_sim)
{
    mem_sim->page_table[free_frame[frame_counter]].V = INVALID;
    mem_sim->page_table[free_frame[frame_counter]].frame = -1;
}


void error(char* msg)
{
    perror(msg);
}

