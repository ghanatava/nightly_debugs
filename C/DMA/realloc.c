#include <stdio.h>
#include <stdlib.h>

int main() {
    int* grades = NULL;
    int* tempGrades = NULL;  //to save the memory from being wiped out in reallocation
    int totalGrades = 0;
    printf("Enter total number of grades: ");
    scanf("%d", &totalGrades);
    grades = (int*) malloc(totalGrades * sizeof(int));
    if (grades == NULL) {
        printf("Memory allocation failed");
        return 1;
    }
    for (int i = 0; i < totalGrades; i++) {
        printf("Enter grade %d: ", i + 1);
        scanf("%d", &grades[i]);
    }
    totalGrades = totalGrades +2;
    tempGrades = (int*) realloc(grades, totalGrades * sizeof(int));  //if it failed temp would point to NULL
    if (tempGrades == NULL) {
        printf("Memory re-allocation failed");
        return 2;
    }
    grades = tempGrades;
    grades[totalGrades-1] = 100;
    grades[totalGrades-2] = 90;

    //Code....
    totalGrades = totalGrades -3;
    tempGrades = (int*) realloc(grades, totalGrades * sizeof(int));
    if (tempGrades == NULL) {
        printf("Memory re-allocation failed to shrink");
        return 3;
    }
    grades = tempGrades;
    //Code....

    return 0;
}