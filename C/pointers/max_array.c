#include <stdio.h>

#define SIZE 4

int max(int n[SIZE]){
    int max = *n;
    for (int i=0; i<SIZE; i++){
        if (max<n[i]){
            max = n[i];
        }
    }
    return max;
}

int main(){
    int arr[SIZE] = {800,85,72,90};
    printf("%d\n",max(arr));
    return 0;
}