/* e09 答案 */
#include <stdio.h>
void FillData(unsigned char *d, int current){
    unsigned char hi = (unsigned char)((current >> 8) & 0xFF);
    unsigned char lo = (unsigned char)(current & 0xFF);
    int i;
    for (i = 0; i < 4; i++){ d[i*2]=hi; d[i*2+1]=lo; }
}
int main(void){
    unsigned char data[8] = {0};
    FillData(data, 3000);
    for (int i = 0; i < 8; i++) printf("%02X ", data[i]);
    printf("\n");
    return 0; }
