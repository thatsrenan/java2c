#include <stdio.h>

int N;
int g[][];

int estacoesAcimaDe(int t) {
    int contador = 0;

    for (int i = 0; i < N; i++) {
        int ligacoes = 0;
        int acima = 0;

        for (int j = 0; j < N; j++) {
            if (g[i][j] != 0) {
                ligacoes++;

                if (g[i][j] >= t) {
                    acima++;
                }
            }
        }

        if (ligacoes == acima) {
            contador++;
        }
    }

    return contador;
}