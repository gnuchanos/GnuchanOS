#include <stdio.h>




int main() {
    // STDIO
    printf("SIMPLE PRINTF \n");

    // write thing to file
    FILE *TESTFILE = fopen("test.gctf", "w");
    fprintf(TESTFILE, "THIS IS MY KINGDOM COME \n");
    fprintf(TESTFILE, "SPEED=31 \n");
    fclose(TESTFILE);

    // scanf, fgets --> fgets become \n this is problem
    char name[64];
    printf("NAME:>> ");
    fgets(name, sizeof(name), stdin);
    printf("WELCOME- %s \n", name);

    // it's not safe to char strings but fine with int, float, double
    int NUMBER;
    printf(">> "); scanf("%d", &NUMBER);
    printf("SCANF- NUMBER: %d \n", NUMBER);





    return 0;
}