#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define LOW 0
#define HIGH 1
//define MAX 131072 //2^(17)
//#define MAX 262144 //2^(18)
#define MAX 1048576 //2^(20)

int isSorted(int vetor[], int tamanho) {
    for (int i = 1; i < tamanho; i++)
        if (vetor[i] < vetor[i - 1])
            return 0;
    return 1;
}

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

//Mescla dois vetores ordenados guardando atualizando local_list os menores elementos dentre {local_list, temp_list}
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

//Mescla dois vetores ordenados guardando atualizando local_list os maiores elementos dentre {local_list, temp_list}
void Merge_list_high(int list_size, int local_list[], int temp_list[]) {
    int merged_list[list_size];
    int i = list_size - 1, j = list_size - 1, k = list_size - 1;

    while (k > -1) {
        if (local_list[i] >= temp_list[j])
            merged_list[k--] = local_list[i--];
        else
            merged_list[k--] = temp_list[j--];
    }

    for (i = 0; i < list_size; i++)
        local_list[i] = merged_list[i];
}

void Merge_split(int original_list[], int list_size, int temp_list[], int which_keys, int tid, int partner, int thread_state[]) {
    for (int i = 0; i < list_size; i++) 
        temp_list[i] = original_list[partner * list_size + i]; //Copia a área da memória da parceira para dentro de temp_list
    
    thread_state[tid] = 1; 

    while (thread_state[partner] == 0) {} //Espera até que a thread parceira tenha lido a lista temporária

    if (which_keys == HIGH)
        Merge_list_high(list_size, &original_list[tid * list_size], temp_list);
    else
        Merge_list_low(list_size, &original_list[tid * list_size], temp_list);

    thread_state[tid] = 2;

    while (thread_state[partner] == 1) {} //Espera até que a thread parceira tenha realizado o devido merge

    thread_state[tid] = 0;
}

void Par_bitonic_sort_incr(int original_list[], int list_size, int thread_set_size, int temp_list[], int tid, int thread_state[]) {
    int stage;
    int thread_set_dim = log_base2(thread_set_size);
    unsigned eor_bit = 1 << (thread_set_dim - 1);

    for (stage = 0; stage < thread_set_dim; stage++) {
        int partner = tid ^ eor_bit;

        if (tid < partner)
            Merge_split(original_list, list_size, temp_list, LOW, tid, partner, thread_state);
        else
            Merge_split(original_list, list_size, temp_list, HIGH, tid, partner, thread_state);


        #pragma omp barrier //Espera que todos os pares tenham realizado o merge para seguir para o próximo estágio
        eor_bit = eor_bit >> 1;
    } 
}

void Par_bitonic_sort_decr(int original_list[], int list_size, int thread_set_size, int temp_list[], int tid, int thread_state[])  {
    int stage;
    int thread_set_dim = log_base2(thread_set_size);
    unsigned eor_bit = 1 << (thread_set_dim - 1);

    for (stage = 0; stage < thread_set_dim; stage++) {
        int partner = tid ^ eor_bit;

        if (tid > partner)
            Merge_split(original_list, list_size, temp_list, LOW, tid, partner, thread_state);
        else
            Merge_split(original_list, list_size, temp_list, HIGH, tid, partner, thread_state);

        #pragma omp barrier //Espera que todos os pares tenham realizado o merge para seguir para o próximo estágio
        eor_bit = eor_bit >> 1;
    }
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Insira a quantidade de threads a serem criadas!\n");
        return 1;
    }

    int t_num = atoi(argv[1]);
    omp_set_num_threads(t_num);
    int list_size = MAX / t_num;
    
    int original_list[MAX];
    int thread_state[t_num];

    //Inicializa o vetor de estado das threads
    for(int i=0; i<t_num; i++)
        thread_state[i] = 0;

    double initial_time, final_time;

    //FILE *file = fopen("lista131072.txt", "r");
    //FILE *file = fopen("lista262144.txt", "r");
    FILE *file = fopen("lista1048576.txt", "r");

    //Inicia contagem de tempo, lê o arquivo para dentro de original_list
    initial_time = omp_get_wtime();
    for (int i = 0; i < MAX; i++) 
        fscanf(file, "%d", &original_list[i]);
    fclose(file);

    #pragma omp parallel shared(list_size, original_list, thread_state) 
    {
        int tid = omp_get_thread_num();

        Local_sort(list_size, &original_list[tid * list_size]);

        #pragma omp barrier //Espera até que todas as threads tenham ordenado suas partes para dar prosseguimento

        int *temp_list = (int *)malloc(sizeof(int) * list_size);
        int thread_set_size;
        unsigned and_bit;
        for (thread_set_size = 2, and_bit = 2; thread_set_size <= t_num; thread_set_size *= 2, and_bit = and_bit << 1) {
            if ((tid & and_bit) == 0)
                Par_bitonic_sort_incr(original_list, list_size, thread_set_size, temp_list, tid, thread_state);
            else
                Par_bitonic_sort_decr(original_list, list_size, thread_set_size, temp_list, tid, thread_state);
        }
    }

    final_time = omp_get_wtime();
    //printf("O vetor %sestá ordenado!\n", isSorted(original_list, MAX)? "": "não ");
    printf("Tempo total em segundos para ordenar um vetor de tamanho %d = %3.6f, com precisão de %3.3e \n", MAX, final_time - initial_time, omp_get_wtick());

    return 0;
}
