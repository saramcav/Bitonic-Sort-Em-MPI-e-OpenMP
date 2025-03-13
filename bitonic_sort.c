#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>

#define LOW 0 
#define HIGH 1

#define MAX 131072 //2^(17)
//#define MAX 262144 //2^(18)
//#define MAX 1048576 //2^(20)

int temp_list[MAX];

int log_base2(int x) {
    int result = 0;

    while (x >= 2) {
        x /= 2;
        result += 1;
    }
    return result;
}

int comparador(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);
}

void Local_sort(int list_size, int local_list[]) {
    qsort(local_list, list_size, sizeof(int), comparador);
}


void Merge_list_low(int list_size, int local_list[], int temp_list[]) {
    int merged_list[list_size];
    int i = 0, j = 0, k = 0;

    while (k < list_size) {
        if (local_list[i] <= temp_list[j]) 
            merged_list[k++] = local_list[i++];
        else 
            merged_list[k++] = temp_list[j++];
    }

    for (i = 0; i < list_size; i++) 
        local_list[i] = merged_list[i];
    
}

void Merge_list_high(int list_size, int local_list[], int temp_list[]) {
    int merged_list[list_size];
    int i = list_size - 1, j = list_size - 1, k = list_size - 1;

    while (k > -1) 
        if (local_list[i] >= temp_list[j]) 
            merged_list[k--] = local_list[i--];
        else 
            merged_list[k--] = temp_list[j--];
        
    

    for (i = 0; i < list_size; i++) 
        local_list[i] = merged_list[i];
}

void Merge_split(int list_size, int local_list[], int which_keys, int partner, MPI_Comm comm) {
    MPI_Status status;

    MPI_Sendrecv(local_list, list_size, MPI_INT, partner, 0, temp_list, list_size, MPI_INT, partner, 0, comm, &status);
    if (which_keys == HIGH)
        Merge_list_high(list_size, local_list, temp_list);
    else
        Merge_list_low(list_size, local_list, temp_list);
}


void Par_bitonic_sort_incr (int list_size, int local_list[], int proc_set_size, MPI_Comm comm) {
    int stage, proc_set_dim, partner, my_rank;
    unsigned eor_bit;
    proc_set_dim = log_base2(proc_set_size);
    eor_bit = 1 << (proc_set_dim - 1);
    MPI_Comm_rank(comm, &my_rank); 

    for(stage = 0; stage < proc_set_dim; stage++) {
        partner = my_rank ^ eor_bit;
        if(my_rank < partner)
            Merge_split(list_size, local_list, LOW, partner, comm);
        else
            Merge_split(list_size, local_list, HIGH, partner, comm);
        eor_bit = eor_bit >> 1;
    }
}

void Par_bitonic_sort_decr (int list_size, int local_list[], int proc_set_size, MPI_Comm comm) {
    int stage, proc_set_dim, partner, my_rank;
    unsigned eor_bit;
    proc_set_dim = log_base2(proc_set_size);
    eor_bit = 1 << (proc_set_dim - 1);
    MPI_Comm_rank(comm, &my_rank); 

    for(stage = 0; stage < proc_set_dim; stage++) {
        partner = my_rank ^ eor_bit;
        if(my_rank > partner)
            Merge_split(list_size, local_list, LOW, partner, comm);
        else
            Merge_split(list_size, local_list, HIGH, partner, comm);
        eor_bit = eor_bit >> 1;
    }
}

void Printa_lista(int lista[], int tam) {
    int i;
    for(i=0; i<tam; i++) 
        printf("%d ", lista[i]);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int *original_list = NULL;
    int *sorted_list = NULL;
    double initial_time, final_time;

    MPI_Comm comm = MPI_COMM_WORLD;
    int my_rank, p, proc_set_size, i;
    unsigned and_bit;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(comm, &my_rank);
    MPI_Comm_size(comm, &p);
    int list_size = MAX / p;
    int local_list[list_size];

     if (my_rank == 0) {
        //FILE *file = fopen("lista131072.txt", "r");
        //FILE *file = fopen("lista262144.txt", "r");
        FILE *file = fopen("lista1048576.txt", "r");
        original_list = (int *) malloc(sizeof(int) * MAX);
        sorted_list = (int *) malloc(sizeof(int) * MAX);
        initial_time = MPI_Wtime();
        for (i = 0; i < MAX; i++) 
            fscanf(file, "%d", &original_list[i]);
        fclose(file);
    }

    MPI_Scatter(&original_list[my_rank * list_size], list_size, MPI_INT, local_list, list_size, MPI_INT, 0, comm);

    if (my_rank == 0) 
        free(original_list);

    Local_sort(list_size, local_list); 

    for (proc_set_size = 2, and_bit = 2; proc_set_size <= p; proc_set_size = proc_set_size * 2, and_bit = and_bit << 1) 
        if ((my_rank & and_bit) == 0) 
            Par_bitonic_sort_incr(list_size, local_list, proc_set_size, comm);
        else 
            Par_bitonic_sort_decr(list_size, local_list, proc_set_size, comm);


    MPI_Gather(local_list, list_size, MPI_INT, sorted_list, list_size, MPI_INT, 0, comm);

    if (my_rank == 0) {
        final_time = MPI_Wtime();
        //printf("Vetor Ordenado:\n");
        //Printa_lista(sorted_list, MAX);
        printf("Tempo total em segundos para ordenar um vetor de tamanho %d = %3.6f, com precisão de %3.3e \n", MAX, final_time - initial_time, MPI_Wtick());
    }

    MPI_Finalize();
    return 0;
}
