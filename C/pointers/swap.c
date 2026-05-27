#include <stdio.h>

void swap(int *n1, int *n2){
    int temp;
    temp = *n1;
    *n1 = *n2;
    *n2 = temp;
}

void main(){
    int n1 = 10;
    int n2 = 20;

    swap(&n1, &n2);
    printf("n1 is %d and n2 is %d \n", n1, n2);
}