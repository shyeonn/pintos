/* We use 17.14 format
 * So p = 17, q = 14
 * And f = 1 << q
 * 
 * x, y is fp
 * n is integer
 */
#include <stdio.h>
#include <stdint.h>
#include "threads/arithmetic.h"
		
#define Q 14
#define F (1<<Q)

int
i2f(int n) {
	return n * F;
}

int 
f2i(int x) {
	if(x >= 0)
		return (x + (F / 2))/F;
	else
		return (x - (F / 2))/F;
}

int
add_x_n(int x, int n) {
	return x + n * F;
}

int
sub_n_x(int x, int n) {
	return x - n * F;
}

int
mul_x_y(int x, int y) {
	return ((int64_t) x) * y / F;
}

int
div_x_y(int x, int y) {
	return ((int64_t) x) * F / y;
}
/*
int main(){
	int x = 59;
	int y = 60;

	int load_avg = 245;

	int ready_threads = 10;

	int result = div_x_y(x, y);

	printf("result  ");
	printf("bin : %b ", x); 
	printf("bin : %b ", fixed_to_near_integer(result)); 
	printf("  %b    ",integer_to_fixed(x));
	printf("  %b    ",fixed_to_near_integer(integer_to_fixed(x)));
	

	printf("\n");
}

*/
