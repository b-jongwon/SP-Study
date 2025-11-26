#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#define CHUNK_SIZE (64*1024)
#define BUFFER_CAPACITY 64
#define MAX_CONSUMERS 32

typedef struct {
    char* data;
    size_t size;
    int starts_inside_word;
} Chunk;

Chunk buffer[BUFFER_CAPACITY];
int in = 0, out = 0, count = 0;
int total_word_count = 0;
int num_consumers = 1;
int is_done = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t not_full = PTHREAD_COND_INITIALIZER;

int is_word_char(char c) {
    return isalnum(c);
}

int count_words_in_chunk(char* buf, size_t size, int starts_inside_word) {
    int count = 0;
    int in_word = 0;
    size_t i = 0;

    if (starts_inside_word) {
        while (i < size && is_word_char(buf[i])) i++;
    }

    for (; i < size; i++) {
        if (is_word_char(buf[i])) {
            if (!in_word) {
                count++;
                in_word = 1;
            }
        } else {
            in_word = 0;
        }
    }
    return count;
}

void* producer(void* arg) {
    FILE* fp = fopen((char*)arg, "r");
    if (!fp) {
        perror("fopen");
        exit(1);
    }

    int prev_ends_in_word = 0;

    while (1) {
        char* buf = malloc(CHUNK_SIZE + 256);
        if (!buf) {
            perror("malloc");
            exit(1);
        }

        size_t size = fread(buf, 1, CHUNK_SIZE, fp);
        if (size == 0) {
            free(buf);
            break;
        }

        int c;
        while (size > 0 && is_word_char(buf[size - 1]) && (c = fgetc(fp)) != EOF) {
            buf[size++] = (char)c;
        }

        Chunk chunk = {
            .data = buf,
            .size = size,
            .starts_inside_word = prev_ends_in_word
        };

        prev_ends_in_word = is_word_char(buf[size - 1]);

        pthread_mutex_lock(&mutex);
        while (count == BUFFER_CAPACITY) {
            pthread_cond_wait(&not_full, &mutex);
        }

        buffer[in] = chunk;
        in = (in + 1) % BUFFER_CAPACITY;
        count++;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&mutex);
    }
    
    fclose(fp);

    pthread_mutex_lock(&mutex);
    is_done = 1;
    pthread_cond_broadcast(&not_empty);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        while (count == 0 && !is_done) {
            pthread_cond_wait(&not_empty, &mutex);
        }

        if (count == 0 && is_done) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        Chunk chunk = buffer[out];
        out = (out + 1) % BUFFER_CAPACITY;
        count--;
        
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);

        int wc = count_words_in_chunk(chunk.data, chunk.size, chunk.starts_inside_word);

        free(chunk.data);

        pthread_mutex_lock(&mutex);
        total_word_count += wc;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <filename> <num_consumers>\n", argv[0]);
        return 1;
    }

    num_consumers = atoi(argv[2]);
    if (num_consumers <= 0 || num_consumers > MAX_CONSUMERS) {
        printf("Number of consumers must be between 1 and %d\n", MAX_CONSUMERS);
        return 1;
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);

    pthread_t prod;
    pthread_t consumers[MAX_CONSUMERS];

    pthread_create(&prod, NULL, producer, argv[1]);
    for (int i = 0; i < num_consumers; i++) {
        pthread_create(&consumers[i], NULL, consumer, NULL);
    }

    pthread_join(prod, NULL);
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], NULL);
    }

    gettimeofday(&end, NULL);
    double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +
                     (end.tv_usec - start.tv_usec) / 1000.0;

    printf("Total words: %d\n", total_word_count);
    printf("Elapsed time (total): %.2f ms\n", elapsed);

    return 0;
}