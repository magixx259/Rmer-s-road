/* e07: 位运算：把 3000 拆成高/低字节（电机电流帧关键！）
   大端(高字节在前) = 和 RmMotor 一致 */
#include <stdio.h>

int main(void)
{
    int raw = 3000;   // 电流原始值
    unsigned char hi, lo;

    // TODO: hi = raw 的高 8 位; lo = raw 的低 8 位
    // 提示: hi = (raw >> 8) & 0xFF;  lo = raw & 0xFF;
    // 然后打印 hi 和 lo（期望 hi=11, lo=184）

    return 0;
}
