/* e09: 把 4 个电机电流打包成 CAN 帧数据（模仿 bench 的 FillData）
   帧数据 8 字节 = 4 个电流 × 2 字节，大端(高字节在前) */
#include <stdio.h>

void FillData(unsigned char *d, int current)
{
    // TODO: 把 current 的高字节放 d[0]，低字节放 d[1]
    // 再把同样值复制到 d[2..3], d[4..5], d[6..7]（简单起见 4 槽都填同值）
    // 提示: d[0] = (unsigned char)((current >> 8) & 0xFF); d[1] = (unsigned char)(current & 0xFF);
}

int main(void)
{
    unsigned char data[8] = {0};
    FillData(data, 3000);
    // 期望打印: 0B B8 0B B8 0B B8 0B B8
    for (int i = 0; i < 8; i++) printf("%02X ", data[i]);
    printf("\n");
    return 0;
}
