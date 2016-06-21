//
// This is a small isolated template for doing C timing tests.
//

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char *argv[]) {
    clock_t begin, end;
    double time_spent;

    begin = clock();

    // Put testing code here

    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

    printf("%f\n", time_spent);
    return 0;
}
