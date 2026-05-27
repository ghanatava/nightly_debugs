#include <stdio.h>
#include <string.h>

void swap(void *a, void *b, size_t size){
    char temp[size];        // temporary buffer of exact size
    memcpy(temp, a, size);  // copy a into temp
    memcpy(a, b, size);     // copy b into a
    memcpy(b, temp, size);  // copy temp into b
}

int main(){
    // swap ints
    int x = 10, y = 20;
    swap(&x, &y, sizeof(int));
    printf("ints: x=%d y=%d\n", x, y);

    // swap floats
    float f1 = 3.14, f2 = 9.99;
    swap(&f1, &f2, sizeof(float));
    printf("floats: f1=%.2f f2=%.2f\n", f1, f2);

    // swap chars
    char c1 = 'A', c2 = 'Z';
    swap(&c1, &c2, sizeof(char));
    printf("chars: c1=%c c2=%c\n", c1, c2);

    // swap structs
    typedef struct { int age; float salary; } Person;
    Person p1 = {25, 50000.0};
    Person p2 = {30, 80000.0};
    swap(&p1, &p2, sizeof(Person));
    printf("persons: p1.age=%d p2.age=%d\n", p1.age, p2.age);
}