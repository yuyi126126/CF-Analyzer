#define _CRT_SECURE_NO_WARNINGS   // 禁用安全函数警告（告诉编译器不要警告使用不安全的函数，如strcpy）
#include <stdio.h>
#include <stdlib.h>              
#include <string.h>     
#include "../include/cf_types.h"  // 数据类型定义
#include "../include/cf_html.h"   // HTML生成接口

void print_usage(const char* program) {   // 打印使用说明函数（显示程序的用法和选项说明）
    printf("用法: %s [选项]\n", program);
    printf("选项:\n");
    printf("  -u <username>   单人模式，分析指定用户\n");
    printf("  -f <filename>   多用户模式，从文件读取用户列表\n");
    printf("  -h, --help      显示帮助信息\n");
    printf("\n示例:\n");
    printf("  %s -u tourist          # 分析单个用户 tourist\n", program);
    printf("  %s -f data/users.txt   # 分析文件中的所有用户\n", program);
    printf("  %s                   # 默认使用 data/users.txt\n", program);
}

void show_menu() {                        // 显示菜单函数
    printf("\n");
    printf("===============================\n");
    printf("    Codeforces 用户分析工具\n");
    printf("===============================\n");
    printf("\n");
    printf("1. 单人模式 - 分析指定用户\n");
    printf("2. 多用户模式 - 使用默认用户列表\n");
    printf("3. 退出\n");
    printf("\n");
}

int main(int argc, char* argv[]) {       // 主函数（argc: 命令行参数个数，argv: 命令行参数数组）
    struct UserStats users[MAX_USERS];
    int user_cnt = 0;
    char filename[512] = "data/users.txt";
    int single_user_mode = 0;            
    char single_handle[128] = "";        // 单人模式标志和用户名（用于判断是否进入单人模式以及存储单人模式下的用户名）

    for (int i = 1; i < argc; i++) {     // 解析命令行参数（循环处理命令行参数，判断用户是否请求帮助信息，是否指定了单人模式的用户名，或者是否指定了多用户模式的文件路径）
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);        // argv[0]是程序的名称，传递给print_usage函数以显示使用说明
            return 0;
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) { // 单人模式参数（如果参数为-u且后面有一个参数，则进入单人模式，并将后面的参数作为用户名）
            single_user_mode = 1;
            strncpy(single_handle, argv[i + 1], sizeof(single_handle) - 1);
            break;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) { // 多用户模式参数（如果参数为-f且后面有一个参数，则将后面的参数作为用户列表文件路径）
            strncpy(filename, argv[i + 1], sizeof(filename) - 1);
            break;
        }
    }
    if (!single_user_mode && argc == 1) {   // 如果没有指定单人模式且没有提供任何参数，则尝试使用默认的用户列表文件路径，并检查文件是否存在，如果不存在则尝试其他可能的路径，直到找到文件或者提示错误信息
        FILE* test = fopen(filename, "r");  // 尝试打开默认文件路径（如果文件存在则继续执行，如果文件不存在则尝试其他路径）
        if (!test) {                        // 尝试其他路径
            strncpy(filename, "../data/users.txt", sizeof(filename) - 1);
            test = fopen(filename, "r");
            if (!test) {
                strncpy(filename, "../../data/users.txt", sizeof(filename) - 1);
                test = fopen(filename, "r");
                if (!test) {
                    fprintf(stderr, "错误: 无法找到 data/users.txt 文件\n");
                    fprintf(stderr, "请确保在项目根目录运行程序，或使用 -f 参数指定文件路径\n");
                    return 1;             
                }
            }
            fclose(test);
        } else {
            fclose(test);
        }
    }
    if (argc == 1) {
        while (1) {
            show_menu();
            printf("请选择操作 (1/2/3): ");
            int choice;
            if (scanf("%d", &choice) != 1) {     // 输入逻辑验证1（得是数字）
                printf("无效输入，请重新选择\n");
                while (getchar() != '\n');
                continue;
            }          
            if (choice == 1) {
                printf("请输入用户名: ");
                scanf("%127s", single_handle);
                printf("\n分析用户: %s\n", single_handle);
                generate_user_page(single_handle, NULL, 1);
                printf("✅ 已生成 %s.html\n", single_handle);
                printf("\n按任意键继续...");
                getchar();      
                getchar();
            } else if (choice == 2) {
                printf("\n正在分析多用户数据...\n");
                printf("这可能需要几分钟时间，请耐心等待...\n\n");
                break;
            } else if (choice == 3) {
                return 0;
            } else {                             // 输入逻辑验证2（得是1、2或3）
                printf("无效输入，请重新选择\n");
            }
        }
    }
    if (single_user_mode) {            // 单人模式报错信息以及单用户页面生成提示
        if (strlen(single_handle) == 0) {
            fprintf(stderr, "错误: -u 参数需要指定用户名\n");
            print_usage(argv[0]);
            return 1;
        }
        generate_user_page(single_handle, NULL, 1);      // 调用生成单用户页面的函数，传入用户名和单人模式标志
        printf("✅ 已生成 %s.html\n", single_handle);
        return 0;
    }
    FILE* in = fopen(filename, "r");
    if (!in) {
        fprintf(stderr, "无法打开文件: %s\n", filename);
        return 1;
    }
    char handle[128];
    while (fscanf(in, "%127s", handle) == 1 && user_cnt < MAX_USERS) {
        printf("正在分析用户: %s...", handle);
        generate_user_page(handle, &users[user_cnt], 0); // 调用生成用户页面的函数，传入用户名和用户统计信息结构体指针
        printf(" ✅\n");
        user_cnt++;
    }
    fclose(in);
    generate_index_page(users, user_cnt);
    printf("✅ 已生成 index.html\n");    // 说明已经生成了索引目录
    return 0;
}