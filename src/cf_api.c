#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>           // HTTP请求库（发起http请求并且处理http响应）
#include "../include/cJSON.h"    // JSON解析库（翻译官，JSON字符串和c语言数据类型之间的转换）           
#include "../include/cf_types.h"
#include <time.h>
#include <windows.h>
static time_t last_request_time = 0;

void wait_for_rate_limit() {
    time_t now = time(NULL);
    double diff = difftime(now, last_request_time);
    
    // 确保至少间隔 2 秒
    if (diff < 2.0) {
        int sleep_ms = (int)((2.0 - diff) * 1000);
        Sleep(sleep_ms);
    }
    
    last_request_time = time(NULL);
}
// 写入回调函数（用于处理HTTP响应数据）
// 功能：当CURL接收到HTTP响应数据时，CURL会调用这个函数来处理数据。函数将接收到的数据追加到内存结构体中，并更新内存大小。
// 参数：ptr - 指向接收到的数据的指针；size - 每个数据块的大小；nmemb - 数据块的数量；userdata - 用户数据指针（这里是指向Memory结构体的指针）
size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata) { 
    size_t realsize = size * nmemb;
    struct Memory* mem = (struct Memory*)userdata;
    char* tmp = realloc(mem->data, mem->size + realsize + 1);
    if (!tmp) return 0;
    mem->data = tmp;
    memcpy(mem->data + mem->size, ptr, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';
    return realsize;
}

void rating_color(int r, char* c) {     // 颜色函数（根据用户评级返回对应的颜色代码）
    if (r >= 3000) strcpy(c, "#FF0000");
    else if (r >= 2400) strcpy(c, "#FF8C00");
    else if (r >= 2100) strcpy(c, "#FFD700");
    else if (r >= 1900) strcpy(c, "#90EE90");
    else if (r >= 1600) strcpy(c, "#ADD8E6");
    else if (r >= 1400) strcpy(c, "#DDA0DD");
    else strcpy(c, "#808080");
}

void rating_class(int r, char* cls) {   // 等级函数（根据用户评级返回对应的等级名称）
    if (r >= 2400) strcpy(cls, "legendary");
    else if (r >= 2100) strcpy(cls, "master");
    else if (r >= 1900) strcpy(cls, "candidate");
    else if (r >= 1600) strcpy(cls, "expert");
    else if (r >= 1400) strcpy(cls, "specialist");
    else strcpy(cls, "newbie");
}

// 获取用户基本信息（发起HTTP请求获取用户基本信息，并解析JSON响应提取用户评级、头衔和头像URL）
int fetch_user_info(const char* handle, int* rating, char* title, char* avatar_url) {
    wait_for_rate_limit();
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    struct Memory mem = { malloc(1), 0 };
    if (!mem.data) {
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);  // 设置写入回调函数（告诉CURL在接收HTTP响应数据时调用这个函数来处理数据）
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);               // 设置写入数据指针（告诉CURL将HTTP响应数据传递给这个内存结构体）
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);                  // 设置超时时间（告诉CURL在30秒内没有响应时停止请求）
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);            // 设置跟随重定向（告诉CURL在遇到HTTP重定向时自动跟随新的URL）

    char url[512];
    snprintf(url, sizeof(url), "https://codeforces.com/api/user.info?handles=%s", handle);
    curl_easy_setopt(curl, CURLOPT_URL, url);      // 设置CURL URL选项（告诉CURL要请求的URL，这里是Codeforces API的用户信息接口，使用用户句柄作为参数）

    CURLcode res = curl_easy_perform(curl);        // 执行HTTP请求（发起HTTP请求并等待响应，结果保存在res中）

    if (res == CURLE_OK) {                         // 如果HTTP请求成功，解析JSON响应并提取用户信息
        cJSON* json = cJSON_Parse(mem.data);              // 解析JSON响应（将HTTP响应数据解析为cJSON对象，方便提取数据）
        if (json) {
            cJSON* result = cJSON_GetObjectItem(json, "result");       // 获取result字段（从JSON对象中提取result字段，result字段包含了用户信息的数组）
            if (result && cJSON_IsArray(result)) {                     // 如果result字段存在且是数组，提取第一个用户的信息（Codeforces API允许一次请求获取多个用户的信息，但这里我们只请求了一个用户，所以取第一个元素）
                cJSON* user = cJSON_GetArrayItem(result, 0);           // 获取第一个用户对象（从result数组中提取第一个用户对象，包含了用户的详细信息）
                if (user) {
                    cJSON* r = cJSON_GetObjectItem(user, "rating");
                    cJSON* t = cJSON_GetObjectItem(user, "rank");
                    cJSON* av = cJSON_GetObjectItem(user, "avatar");

                    if (r) *rating = r->valueint;

                    if (t && t->valuestring) {    // 如果用户有rank字段且是字符串类型，则使用它作为title
                        strncpy(title, t->valuestring, 127);
                        title[127] = '\0';
                    }
                    if (av && av->valuestring) {  // 如果用户有avatar字段且是字符串类型，则使用它作为avatar_url
                        strncpy(avatar_url, av->valuestring, 511);
                        avatar_url[511] = '\0';
                    }
                }
            }
            cJSON_Delete(json);
        }
    }

    free(mem.data);
    curl_easy_cleanup(curl);
    return res == CURLE_OK ? 0 : -1;
}

// 获取用户比赛信息（发起HTTP请求获取用户比赛记录，并解析JSON响应提取比赛信息）
int fetch_user_contests(const char* handle, struct Contest* contests, int* count) {
    wait_for_rate_limit();
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    struct Memory mem = { malloc(1), 0 };
    if (!mem.data) {
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    char url[512];
    snprintf(url, sizeof(url), "https://codeforces.com/api/user.rating?handle=%s", handle);
    curl_easy_setopt(curl, CURLOPT_URL, url);     // 设置CURL URL选项（告诉CURL要请求的URL，这里是Codeforces API的用户比赛记录接口，使用用户句柄作为参数）

    CURLcode res = curl_easy_perform(curl);                // 执行HTTP请求（发起HTTP请求并等待响应，结果保存在res中）
    *count = 0;

    if (res == CURLE_OK) {
        cJSON* json = cJSON_Parse(mem.data);
        if (json) {
            cJSON* contests_json = cJSON_GetObjectItem(json, "result");   // 获取result字段（从JSON对象中提取result字段，result字段包含了用户的比赛记录数组）
            if (contests_json && cJSON_IsArray(contests_json)) {
                int total = cJSON_GetArraySize(contests_json);                // 获取比赛数量（从result数组中获取比赛记录的数量，统计用户参加过的比赛总数）
                *count = total < MAX_CONTEST ? total : MAX_CONTEST;  // 限制比赛数量（如果比赛数量超过MAX_CONTEST，则只处理前MAX_CONTEST条记录，防止内存溢出）

                for (int i = 0; i < *count; i++) {                       // 遍历比赛记录（循环处理每一条比赛记录，提取比赛信息并存储在contests数组中）
                    cJSON* c = cJSON_GetArrayItem(contests_json, i);
                    if (!c) continue;

                    cJSON* name = cJSON_GetObjectItem(c, "contestName");
                    cJSON* t = cJSON_GetObjectItem(c, "ratingUpdateTimeSeconds");
                    cJSON* oldR = cJSON_GetObjectItem(c, "oldRating");
                    cJSON* newR = cJSON_GetObjectItem(c, "newRating");
                    cJSON* rank = cJSON_GetObjectItem(c, "rank");

                    strcpy(contests[i].name, name ? name->valuestring : "Unknown");// 提取比赛名称（如果比赛记录中有contestName字段且是字符串类型，则使用它作为比赛名称，否则使用"Unknown"）
                    contests[i].time = t ? t->valueint : 0;            // 提取比赛时间（如果比赛记录中有ratingUpdateTimeSeconds字段且是整数类型，则使用它作为比赛时间，否则使用0）
                    contests[i].oldRating = oldR ? oldR->valueint : 0;
                    contests[i].newRating = newR ? newR->valueint : 0;
                    contests[i].rank = rank ? rank->valueint : 0;
                    contests[i].contestAC = 0;
                    contests[i].practiceAC = 0;
                }
            }
            cJSON_Delete(json);
        }
    }

    free(mem.data);
    curl_easy_cleanup(curl);
    return res == CURLE_OK ? 0 : -1;
}

// 获取用户提交信息（发起HTTP请求获取用户提交记录，并解析JSON响应提取提交信息，统计AC数量和题目难度分布）
int fetch_user_submissions(const char* handle, struct Submission* subs, int* count) {
    wait_for_rate_limit();
    CURL* curl = curl_easy_init();
    if (!curl) return -1;

    struct Memory mem = { malloc(1), 0 };
    if (!mem.data) {
        curl_easy_cleanup(curl);
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    *count = 0;
    int from = 1;                 // 起始位置（Codeforces API的分页参数，表示从第几条记录开始获取，初始值为1）
    const int batch_size = 1000;  // 每次请求获取的提交数量（Codeforces API允许一次请求获取最多1000条提交记录）

    while (from <= MAX_SUBMISSIONS) {
        char url[512];
        snprintf(url, sizeof(url), "https://codeforces.com/api/user.status?handle=%s&from=%d&count=%d", handle, from, batch_size);
        curl_easy_setopt(curl, CURLOPT_URL, url);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) break;

        cJSON* json = cJSON_Parse(mem.data);
        if (!json) {
            mem.size = 0;
            break;
        }

        cJSON* result = cJSON_GetObjectItem(json, "result");
        if (!result || !cJSON_IsArray(result)) {
            cJSON_Delete(json);
            mem.size = 0;
            break;
        }

        int n = cJSON_GetArraySize(result);
        if (n == 0) {
            cJSON_Delete(json);
            break;
        }

        for (int i = 0; i < n && *count < MAX_SUBMISSIONS; i++) {
            cJSON* s = cJSON_GetArrayItem(result, i);
            if (!s) continue;

            cJSON* v = cJSON_GetObjectItem(s, "verdict");
            cJSON* a = cJSON_GetObjectItem(s, "author");
            cJSON* t = cJSON_GetObjectItem(s, "creationTimeSeconds");
            if (!v || !a || !t) continue;

            strcpy(subs[*count].verdict, v->valuestring ? v->valuestring : "");
            subs[*count].creationTimeSeconds = t->valueint;

            cJSON* p = cJSON_GetObjectItem(a, "participantType");
            strcpy(subs[*count].participantType, p && p->valuestring ? p->valuestring : "");

            cJSON* problem = cJSON_GetObjectItem(s, "problem");
            cJSON* rating_obj = NULL;
            if (problem) {
                rating_obj = cJSON_GetObjectItem(problem, "rating");
            }
            subs[*count].problemRating = rating_obj ? rating_obj->valueint : -1;

            (*count)++;
        }

        cJSON_Delete(json);
        mem.size = 0;

        if (n < batch_size) break;
        from += batch_size;
    }

    free(mem.data);
    curl_easy_cleanup(curl);
    return 0;
}

// 分析提交记录（统计AC数量、补题数量，并根据提交时间和题目难度分布进行统计）
// 需要参数：提交记录数组、提交记录数量、比赛记录数组、比赛记录数量，以及用于存储统计结果的变量和数组
void analyze_submissions(struct Submission* subs, int sub_cnt, struct Contest* contests, int contest_cnt,
                         int* ac_count, int* practice_count,
                         int all_rating_count[], int year_rating_count[],
                         int half_year_rating_count[], int month_rating_count[]) {
    *ac_count = 0;
    *practice_count = 0;

    for (int i = 0; i < 28; i++) {
        all_rating_count[i] = 0;
        year_rating_count[i] = 0;
        half_year_rating_count[i] = 0;
        month_rating_count[i] = 0;
    }

    time_t now = time(NULL);

    for (int i = 0; i < sub_cnt; i++) {            // 遍历提交记录（循环处理每一条提交记录，统计AC数量、补题数量，并根据提交时间和题目难度分布进行统计）
        if (strcmp(subs[i].verdict, "OK") == 0) (*ac_count)++;

        if (strcmp(subs[i].participantType, "PRACTICE") == 0)
            (*practice_count)++;

        long long subTime = subs[i].creationTimeSeconds;
        for (int j = 0; j < contest_cnt; j++) {    // 遍历比赛记录（循环处理每一条比赛记录，检查提交时间是否在比赛时间范围内，并统计比赛AC和补题数量）
            long long start = contests[j].time;
            long long end = start + 3 * 3600;
            if (subTime >= start && subTime <= end && strcmp(subs[i].verdict, "OK") == 0) {// 如果提交时间在比赛时间范围内且提交结果为OK，则根据参与者类型统计比赛AC和补题数量
                if (strcmp(subs[i].participantType, "CONTESTANT") == 0)
                    contests[j].contestAC++;
                else if (strcmp(subs[i].participantType, "PRACTICE") == 0)
                    contests[j].practiceAC++;
                break;
            }
        }

        if (strcmp(subs[i].verdict, "OK") == 0 && subs[i].problemRating >= 800) {          // 统计题目难度分布（如果提交结果为OK且题目有评级，则根据题目难度分布进行统计，分别统计全部时间、最近一年、最近180天和最近1个月的题目数量）
            int idx = (subs[i].problemRating - 800) / 100;
            if (idx >= 0 && idx < 28) {
                all_rating_count[idx]++;
                double diff = difftime(now, subs[i].creationTimeSeconds);
                if (diff <= 365.0 * 24 * 3600) year_rating_count[idx]++;
                if (diff <= 180.0 * 24 * 3600) half_year_rating_count[idx]++;
                if (diff <= 30.0 * 24 * 3600) month_rating_count[idx]++;
            }
        }
    }
}