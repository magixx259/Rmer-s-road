/* e08: struct + 指针 */
#include <stdio.h>

struct Motor {
    int id;       // CAN ID
    int current;  // 电流原始值
};

int main(void)
{
    struct Motor m = {1, 3000};   // id=1, current=3000
    struct Motor *p = &m;         // p 指向 m

    // TODO: 用指针 p 把 current 改成 1500，然后打印 m.current（期望 1500）
    // 提示: p->current = 1500;
    return 0;
}
