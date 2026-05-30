#ifndef CF_HTML_H    // 头文件保护（防止重复包含）
#define CF_HTML_H    // HTML生成接口（声明生成用户页面和索引页面的函数）

#include "cf_types.h"

// 返回值：0-成功，-1-用户不存在，-2-网络错误，-3-其他错误
int generate_user_page(const char* handle, struct UserStats* stats, int single_mode);
void generate_index_page(struct UserStats* users, int user_cnt);

#endif