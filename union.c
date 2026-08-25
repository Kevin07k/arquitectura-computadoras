#include <stdio.h>
typedef union {
unsigned char byte;

  struct  {
    unsigned char b0 : 1;
    unsigned char b1 : 1;
    unsigned char b2 : 1;
    unsigned char b3 : 1;
    unsigned char b4 : 1;
    unsigned char b5 : 1;
    unsigned char b6 : 1;
    unsigned char b7 : 1;
  } bits;
} Reg8Bits;

int main(){
  Reg8Bits miDato;
  miDato.byte = 5;
  printf("%d%d%d%d%d%d%d%d  \n", miDato.bits.b7, miDato.bits.b6, miDato.bits.b5, miDato.bits.b4, miDato.bits.b3, miDato.bits.b2, miDato.bits.b1, miDato.bits.b0);
}

