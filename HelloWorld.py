#include <stdio.h>

int main() {
    char input[100]; // 准备一个小盒子用来装你输入的文字
    printf("Hello World!\n");
    printf("请输入一些内容: ");

    // 使用 scanf 获取键盘输入的内容
    scanf("%s", input);

    // 使用 printf 把你输入的内容显示出来
    printf("你输入的内容是: %s\n", input);

    return 0;
}