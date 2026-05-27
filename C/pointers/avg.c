#include <stdio.h>

#define SIZE 4

int avg (int arr[SIZE]) {
    if (SIZE==0) return 0;
    int sum = 0;
    for (int i = 0; i < SIZE; i++) {
       sum += arr[i];
    }
    return sum/SIZE;
}

int main(){
    int arr[SIZE] = {80,85,72,90};
    printf("%d\n",avg(arr));
    return 0;
}