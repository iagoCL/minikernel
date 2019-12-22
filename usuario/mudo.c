/*
 * Programa de usuario que "gasta CPU".
 */

#include "servicios.h"

#define TOT_ITER 200000000 /* ponga las que considere oportuno */

int main() {
    int i, tot;
    int j = 5;

    for (i = 0; i < TOT_ITER; i++)
        tot = j * i;
    printf("mudo (%d): termina con %d\n", obtener_id_pr(), tot);
    return 0;
}