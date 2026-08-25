#include <limits.h>
#include<stdio.h>
#include<stdint.h>
int main(){
	int8_t a = 120,b= 20;
	if(__INT8_MAX__ < a + b || a + b < (- __INT8_MAX__ +1)){
		printf("Existe un desvordamiento");
	}else{
		int8_t resultado = a + b;
		printf("%d + %d = %d\n", a ,b ,resultado);
	}
	return 0;
}
