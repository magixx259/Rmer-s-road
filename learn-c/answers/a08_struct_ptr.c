/* e08 答案 */
#include <stdio.h>
struct Motor { int id; int current; };
int main(void){
    struct Motor m = {1, 3000};
    struct Motor *p = &m;
    p->current = 1500;
    printf("current=%d\n", m.current);
    return 0; }
