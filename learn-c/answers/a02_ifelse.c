/* e02 答案 */
#include <stdio.h>
int main(void){
    int speed = 1500;
    if (speed == 0)      printf("停止\n");
    else if (speed < 3000) printf("慢转\n");
    else                 printf("快转\n");
    return 0; }
