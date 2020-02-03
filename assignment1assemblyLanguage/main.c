#include "LPC17xx.h"
#include "stdio.h"

// EE2024 Assignment 1 skeleton code
// (C) CK Tham, ECE, NUS, 2018

 // Do not modify the following function prototype
extern int asm_stats(int* px, int* pvar, int* pmin, int* pmax);

typedef struct data
{
 	int x[12];
 	int mean;
 	int variance;
	int min;
 	int max;
} dataset;

// Do not modify the following function prototype
void compute_stats(dataset* dptr)
{
	//  Write the code to call asm_stats() function
	//  in an appropriate manner to compute the statistical parameters
	//  ... asm_stats( ... )
	asm_stats(dptr->x,&(dptr->variance),&(dptr->min),&(dptr->max));
}

int main(void)
{
	int i;
	//  Instantiate a dataset object X
	dataset X;

	//  Initialize the dataset object X
	for (i=0;i<12;i++)
		X.x[i] = i-6;

	X.mean = X.variance = X.min = X.max = 0;
	X.variance=i;
	//  Call compute_stats() function in an appropriate manner to process the dataset object X
	//  ... compute_stats( ... )
	compute_stats(&X);
	//  Print out the contents of the dataset object X
	for (i=0;i<12;i++)
		printf("x[%d]: %d\n", i, X.x[i]);
	printf("mean: %d\n", X.mean);
	printf("variance: %d\n", X.variance);
	printf("min: %d\n", X.min);
	printf("max: %d\n", X.max);

	// Enter an infinite loop, just incrementing a counter.
	// Do not modify the code below. It enables values or variables and registers to be inspected before the program ends.
	volatile static int loop = 0;
	while (1) {
		loop++;
	}
}
