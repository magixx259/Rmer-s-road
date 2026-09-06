/* e05 答案 */
#include <stdio.h>
#define MOTOR_MAX_RAW 10000
#define MOTOR_MAX_AMP 10
int main(void){
    int raw_per_amp = MOTOR_MAX_RAW / MOTOR_MAX_AMP;
    printf("1A = %d raw\n", raw_per_amp);
    return 0; }
