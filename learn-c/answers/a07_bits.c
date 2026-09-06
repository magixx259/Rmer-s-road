/* e07 答案 */
#include <stdio.h>
int main(void){
    int raw = 3000;
    unsigned char hi, lo;
    hi = (unsigned char)((raw >> 8) & 0xFF);
    lo = (unsigned char)(raw & 0xFF);
    printf("hi=%u lo=%u\n", hi, lo);
    return 0; }
