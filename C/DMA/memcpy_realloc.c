// void* myRealloc(void* srcBlock,unsigned oldSize,unsigned newSize){}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *my_realloc(void *srcBlock, size_t oldsize, size_t newsize) {
    if (newsize == 0) {
        free(srcBlock);
        return NULL;
    }

    if (srcBlock == NULL) {
        return malloc(newsize);
    }

    void *temp = malloc(newsize);
    if (temp == NULL) {
        printf("Error allocating memory\n");
        return NULL;   // srcBlock is still valid here
    }

    size_t copy_size = oldsize < newsize ? oldsize : newsize;
    memcpy(temp, srcBlock, copy_size);

    free(srcBlock);
    return temp;
}
int main () {
    int* numbers = (int*) malloc(sizeof(int)*3);
    int* newNumbers = NULL;
    numbers[0] = 1;
    numbers[1] = 2;
    numbers[2] = 3;
    newNumbers = (int*) my_realloc(numbers,sizeof(int)*3,sizeof(int)*4);
    if (newNumbers == NULL) {
        printf("Error allocating memory\n");
        return -1;
    }
    newNumbers[3] = 1;
    for (int i = 0; i < 3; i++) {
        printf("numbers[%d] = %d\n", i, newNumbers[i]);
    }
    return 0;
}