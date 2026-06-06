// void* myRealloc(void* srcBlock,unsigned oldSize,unsigned newSize){}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *my_realloc(void *srcBlock, unsigned int oldsize, unsigned int newsize) {
     int i;
     char* resultArr = (char*)malloc(newsize);  //copy byte by byte
     if (!resultArr) return NULL;

     for (i = 0; i < oldsize; i++) {
        resultArr[i] = ((char*)srcBlock)[i];
     }
     free(srcBlock);
     return resultArr;
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