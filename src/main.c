#define _CRT_SECURE_NO_WARNINGS  // 禁用安全函数警告（告诉编译器不要警告使用不安全的函数，如strcpy）
#include <stdio.h>
#include <stdlib.h>              
#include <string.h>     
#include <time.h>                // 时间处理库
#include <curl/curl.h>           // HTTP请求库
#include "../include/cJSON.h"    // JSON解析库           
#include <math.h>

#define MAX_CONTEST 1000
#define MAX_SUBMISSIONS 10000    // 最大提交记录数（用于限制从Codeforces API获取的提交记录数量，防止内存溢出）
#define MAX_USERS 100

struct Memory {                  // 内存结构体（用于存储HTTP响应数据）     
    char* data;
    size_t size;
};

struct Contest {                 // 比赛结构体（用于存储比赛信息）
    char name[128];
    long long time;
    int oldRating;
    int newRating;
    int rank;
    int contestAC;
    int practiceAC;
};

struct Submission {              // 提交结构体（用于存储提交信息）
    char verdict[32];                  // 提交结果（如"OK"、"WRONG_ANSWER"等）
    char participantType[32];          // 参与者类型（如"CONTESTANT"、"PRACTICE"等）
    long long creationTimeSeconds;     // 提交时间（以秒为单位的Unix时间戳）
    int problemRating;                 // 题目难度等级（如800、900等，如果没有则为-1）
};

struct UserStats {               // 用户统计结构体（用于存储用户统计信息）
    char handle[128];
    int rating;
    char title[128];
    char avatar_url[512];        // 头像URL
    int total_contests;
    int max_rating;
    int last180_contests;
    int last180_max_rating;
    int ac_count;
    int practice_count;
};

size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata) { // 写入回调函数（用于处理HTTP响应数据）
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

void rating_color(int r, char* c) {     // 评级颜色函数（根据用户评级返回对应的颜色代码）
    if (r >= 3000) strcpy(c, "#FF0000");
    else if (r >= 2400) strcpy(c, "#FF8C00");
    else if (r >= 2100) strcpy(c, "#FFD700");
    else if (r >= 1900) strcpy(c, "#90EE90");
    else if (r >= 1600) strcpy(c, "#ADD8E6");
    else if (r >= 1400) strcpy(c, "#DDA0DD");
    else strcpy(c, "#808080");
}

void rating_class(int r, char* cls) {   // 评级等级函数（根据用户评级返回对应的等级名称）
    if (r >= 2400) strcpy(cls, "legendary");
    else if (r >= 2100) strcpy(cls, "master");
    else if (r >= 1900) strcpy(cls, "candidate");
    else if (r >= 1600) strcpy(cls, "expert");
    else if (r >= 1400) strcpy(cls, "specialist");
    else strcpy(cls, "newbie");
}

void generate_user_page(const char* handle, struct UserStats* stats, int single_mode) { // 生成用户页面函数（根据用户句柄和统计信息生成HTML页面）
    CURL* curl = curl_easy_init();          // 初始化CURL（创建一个CURL句柄，用于执行HTTP请求）
    if (!curl) return;

    struct Memory mem = { malloc(1), 0 };   // 初始化内存结构体（分配初始内存并设置大小为0）
    if (!mem.data) {                        // 内存分配失败，清理CURL并返回
        curl_easy_cleanup(curl);
        return;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);  // 设置写入回调函数（告诉CURL在接收HTTP响应数据时调用这个函数来处理数据）
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);               // 设置写入数据指针（告诉CURL将HTTP响应数据传递给这个内存结构体）
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);                  // 设置超时时间（告诉CURL在30秒内没有响应时停止请求）
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);            // 设置跟随重定向（告诉CURL在遇到HTTP重定向时自动跟随新的URL）

    char url[512];
    cJSON* json = NULL;

    int rating = 0;
    char title[128] = "Unrated";
    char avatar_url[512] = "https://userpic.codeforces.org/no-title.jpg";

    int i, j;

    snprintf(url, sizeof(url), "https://codeforces.com/api/user.info?handles=%s", handle);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {                         // 如果HTTP请求成功，解析JSON响应并提取用户信息
        json = cJSON_Parse(mem.data);
        if (json) {
            cJSON* result = cJSON_GetObjectItem(json, "result");
            if (result && cJSON_IsArray(result)) {
                cJSON* user = cJSON_GetArrayItem(result, 0);
                if (user) {
                    cJSON* r = cJSON_GetObjectItem(user, "rating");
                    cJSON* t = cJSON_GetObjectItem(user, "rank");
                    cJSON* av = cJSON_GetObjectItem(user, "avatar");

                    if (r) rating = r->valueint;

                    if (t && t->valuestring) {    // 如果用户有rank字段且是字符串类型，则使用它作为title
                        strncpy(title, t->valuestring, sizeof(title) - 1);
                        title[sizeof(title) - 1] = '\0';
                    }
                    if (av && av->valuestring) {  // 如果用户有avatar字段且是字符串类型，则使用它作为avatar_url
                        strncpy(avatar_url, av->valuestring, sizeof(avatar_url) - 1);
                        avatar_url[sizeof(avatar_url) - 1] = '\0';
                    }
                }
            }
            cJSON_Delete(json);
        }
    }
    mem.size = 0;

    struct Contest contests[MAX_CONTEST];
    int contest_cnt = 0;
    int total = 0, last180 = 0;
    int maxRating = 0, maxRating180 = 0;

    snprintf(url, sizeof(url), "https://codeforces.com/api/user.rating?handle=%s", handle);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        json = cJSON_Parse(mem.data);
        if (json) {
            cJSON* contests_json = cJSON_GetObjectItem(json, "result");
            if (contests_json && cJSON_IsArray(contests_json)) {
                total = cJSON_GetArraySize(contests_json);
                time_t now = time(NULL);   // 获取当前时间（以秒为单位的Unix时间戳）
                contest_cnt = total < MAX_CONTEST ? total : MAX_CONTEST;

                for (i = 0; i < contest_cnt; i++) {
                    cJSON* c = cJSON_GetArrayItem(contests_json, i);
                    if (!c) continue;

                    cJSON* name = cJSON_GetObjectItem(c, "contestName");
                    cJSON* t = cJSON_GetObjectItem(c, "ratingUpdateTimeSeconds");
                    cJSON* oldR = cJSON_GetObjectItem(c, "oldRating");
                    cJSON* newR = cJSON_GetObjectItem(c, "newRating");
                    cJSON* rank = cJSON_GetObjectItem(c, "rank");

                    strcpy(contests[i].name, name ? name->valuestring : "Unknown");
                    contests[i].time = t ? t->valueint : 0;
                    contests[i].oldRating = oldR ? oldR->valueint : 0;
                    contests[i].newRating = newR ? newR->valueint : 0;
                    contests[i].rank = rank ? rank->valueint : 0;
                    contests[i].contestAC = 0;
                    contests[i].practiceAC = 0;

                    if (contests[i].newRating > maxRating)
                        maxRating = contests[i].newRating;

                    if (t && difftime(now, t->valueint) <= 180.0 * 24 * 3600) {
                        last180++;
                        if (contests[i].newRating > maxRating180)
                            maxRating180 = contests[i].newRating;
                    }
                }
            }
            cJSON_Delete(json);
        }
    }
    mem.size = 0;

    int ac = 0, practice = 0;
    int all_rating_count[28] = { 0 };
    int year_rating_count[28] = { 0 };
    int half_year_rating_count[28] = { 0 };
    int month_rating_count[28] = { 0 };

    struct Submission subs[MAX_SUBMISSIONS];
    int sub_cnt = 0;              // 提交数量（用于统计用户的提交记录，最多统计MAX_SUBMISSIONS条记录）
    int from = 1;
    const int batch_size = 1000;  // 每次请求获取的提交数量（Codeforces API允许一次请求获取最多1000条提交记录）

    while (from <= MAX_SUBMISSIONS) {
        snprintf(url, sizeof(url), "https://codeforces.com/api/user.status?handle=%s&from=%d&count=%d", handle, from, batch_size);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        res = curl_easy_perform(curl);

        if (res != CURLE_OK) break;

        json = cJSON_Parse(mem.data);
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

        for (i = 0; i < n && sub_cnt < MAX_SUBMISSIONS; i++) {
            cJSON* s = cJSON_GetArrayItem(result, i);
            if (!s) continue;

            cJSON* v = cJSON_GetObjectItem(s, "verdict");
            cJSON* a = cJSON_GetObjectItem(s, "author");
            cJSON* t = cJSON_GetObjectItem(s, "creationTimeSeconds");
            if (!v || !a || !t) continue;

            strcpy(subs[sub_cnt].verdict, v->valuestring ? v->valuestring : "");
            subs[sub_cnt].creationTimeSeconds = t->valueint;

            cJSON* p = cJSON_GetObjectItem(a, "participantType");
            strcpy(subs[sub_cnt].participantType, p && p->valuestring ? p->valuestring : "");

            cJSON* problem = cJSON_GetObjectItem(s, "problem");
            cJSON* rating_obj = NULL;
            if (problem) {
                rating_obj = cJSON_GetObjectItem(problem, "rating");
            }
            subs[sub_cnt].problemRating = rating_obj ? rating_obj->valueint : -1;

            sub_cnt++;
        }

        cJSON_Delete(json);
        mem.size = 0;

        if (n < batch_size) break;
        from += batch_size;
    }

    time_t now = time(NULL);
    for (i = 0; i < sub_cnt; i++) {
        if (strcmp(subs[i].verdict, "OK") == 0) ac++;

        if (strcmp(subs[i].participantType, "PRACTICE") == 0)
            practice++;

        long long subTime = subs[i].creationTimeSeconds;
        for (j = 0; j < contest_cnt; j++) {
            long long start = contests[j].time;
            long long end = start + 3 * 3600;
            if (subTime >= start && subTime <= end && strcmp(subs[i].verdict, "OK") == 0) {
                if (strcmp(subs[i].participantType, "CONTESTANT") == 0)
                    contests[j].contestAC++;
                else if (strcmp(subs[i].participantType, "PRACTICE") == 0)
                    contests[j].practiceAC++;
                break;
            }
        }

        if (strcmp(subs[i].verdict, "OK") == 0 && subs[i].problemRating >= 800) {
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

    if (stats != NULL) {
        strcpy(stats->handle, handle);
        stats->rating = rating;
        strcpy(stats->title, title);
        strcpy(stats->avatar_url, avatar_url);
        stats->total_contests = total;
        stats->max_rating = maxRating;
        stats->last180_contests = last180;
        stats->last180_max_rating = maxRating180;
        stats->ac_count = ac;
        stats->practice_count = practice;
    }

    char file[256];
    snprintf(file, sizeof(file), "%s.html", handle);
    FILE* fp = fopen(file, "w");
    if (!fp) goto cleanup;

    char cls[30];
    rating_class(rating, cls);

    fprintf(fp, "<!DOCTYPE html>\n<html lang='zh-CN'>\n<head>\n");
    fprintf(fp, "<meta charset='UTF-8'>\n");
    fprintf(fp, "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n");
    fprintf(fp, "<title>%s | Codeforces Analysis</title>\n", handle);
    fprintf(fp, "<script src='https://cdn.jsdelivr.net/npm/echarts@5.4.3/dist/echarts.min.js'></script>\n");
    fprintf(fp, "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap' rel='stylesheet'>\n");

    fprintf(fp, "<style>\n");
    fprintf(fp, "*{margin:0;padding:0;box-sizing:border-box;font-family:'Inter','Segoe UI','Microsoft YaHei',sans-serif;}\n");
    fprintf(fp, "body{background:#f5f7fc;color:#212529;line-height:1.6;}\n");
    fprintf(fp, ".container{max-width:1200px;margin:0 auto;padding:0 20px;}\n");

    fprintf(fp, "header{position:fixed;top:0;left:0;width:100%%;z-index:999;background:rgba(255,255,255,.95);backdrop-filter:blur(10px);box-shadow:0 5px 15px rgba(0,0,0,.08);}\n");
    fprintf(fp, "nav{display:flex;justify-content:space-between;align-items:center;padding:14px 0;}\n");
    fprintf(fp, ".logo{font-size:1.6rem;font-weight:800;background:linear-gradient(90deg,#3a86ff,#8338ec);-webkit-background-clip:text;-webkit-text-fill-color:transparent;}\n");
    fprintf(fp, ".nav-links{display:flex;gap:28px;list-style:none;}\n");
    fprintf(fp, ".nav-links a{text-decoration:none;color:#212529;font-weight:600;transition:.3s;}\n");
    fprintf(fp, ".nav-links a:hover{color:#3a86ff;}\n");

    fprintf(fp, ".card{background:#fff;border-radius:18px;padding:32px;margin:100px 0 40px;box-shadow:0 10px 30px rgba(0,0,0,.06);transition:.3s;}\n");
    fprintf(fp, ".card:hover{transform:translateY(-6px);box-shadow:0 20px 40px rgba(0,0,0,.1);}\n");

    fprintf(fp, ".user-header{display:flex;align-items:center;gap:28px;}\n");
    fprintf(fp, ".avatar{width:96px;height:96px;border-radius:50%%;border:3px solid #eee;}\n");
    fprintf(fp, ".stats{display:flex;gap:36px;margin-top:20px;}\n");
    fprintf(fp, ".stat{text-align:center;}.stat-value{font-size:22px;font-weight:700;}.stat-label{font-size:13px;color:#666;}\n");

    fprintf(fp, ".legendary{color:#ff006e;font-weight:800;}.master{color:#ff8c00;}.candidate{color:#aa00aa;}.expert{color:#2266cc;}.specialist{color:#03a89e;}.newbie{color:#808080;}\n");

    fprintf(fp, "table{width:100%%;border-collapse:collapse;margin-top:20px;}\n");
    fprintf(fp, "th,td{padding:12px;border-bottom:1px solid #eee;text-align:center;}\n");
    fprintf(fp, "th{background:#f8f9fb;font-weight:600;}\n");
    fprintf(fp, "tr:hover{background:#f6f8ff;}\n");

    fprintf(fp, ".chart-box{height:420px;margin-top:20px;}\n");
    fprintf(fp, ".period-buttons{display:flex;gap:12px;margin:20px 0;}\n");
    fprintf(fp, ".period-btn{padding:10px 20px;border-radius:8px;border:2px solid #e0e0e0;background:#fff;cursor:pointer;font-weight:600;transition:.3s;}\n");
    fprintf(fp, ".period-btn:hover{background:#f0f4ff;border-color:#3a86ff;}\n");
    fprintf(fp, ".period-btn.active{background:#3a86ff;color:#fff;border-color:#3a86ff;}\n");

    fprintf(fp, ".section-title{font-size:1.4rem;font-weight:700;margin-bottom:10px;color:#212529;}\n");
    fprintf(fp, "</style></head><body>\n");

    fprintf(fp, "<header><nav class='container'><div class='logo'>CF Analyzer</div><ul class='nav-links'>");
    fprintf(fp, "<li><a href='#overview'>概览</a></li>");
    fprintf(fp, "<li><a href='#rating'>Rating</a></li>");
    fprintf(fp, "<li><a href='#problems'>题目分析</a></li>");
    fprintf(fp, "<li><a href='#contests'>比赛记录</a></li>");
    if (!single_mode) {
        fprintf(fp, "<li><a href='index.html' class='period-btn'>返回列表</a></li>");
    }
    fprintf(fp, "</ul></nav></header>\n");

    fprintf(fp, "<div class='container'>\n");
    fprintf(fp, "<section id='overview' class='card user-header'>\n");

    fprintf(fp, "<img class='avatar' src='%s'>\n", avatar_url);

    fprintf(fp, "<div><h1 class='%s'>%s</h1><p>头衔：%s</p>", cls, handle, title);
    fprintf(fp, "<div class='stats'>\n");
    fprintf(fp, "<div class='stat'><div class='stat-value'>%d</div><div class='stat-label'>当前等级分</div></div>\n", rating);
    fprintf(fp, "<div class='stat'><div class='stat-value'>%d</div><div class='stat-label'>最高等级分</div></div>\n", maxRating);
    fprintf(fp, "<div class='stat'><div class='stat-value'>%d</div><div class='stat-label'>总比赛次数</div></div>\n", total);
    fprintf(fp, "<div class='stat'><div class='stat-value'>%d</div><div class='stat-label'>通过题目</div></div>\n", ac);
    fprintf(fp, "<div class='stat'><div class='stat-value'>%d</div><div class='stat-label'>补题数量</div></div>\n", practice);
    fprintf(fp, "<div class='stat'><div class='stat-value'>%d</div><div class='stat-label'>近180天比赛次数</div></div>\n", last180);
    fprintf(fp, "<div class='stat'><div class='stat-value'>%d</div><div class='stat-label'>近180天最高等级分</div></div>\n", maxRating180);
    fprintf(fp, "</div></div></section>\n");

    fprintf(fp, "<section id='rating' class='card'><h2 class='section-title'>Rating 变化趋势</h2><div id='ratingChart' class='chart-box'></div></section>\n");

    fprintf(fp, "<section id='problems' class='card'>\n");
    fprintf(fp, "<h2 class='section-title'>题目难度分布直方图</h2>\n");
    fprintf(fp, "<div class='period-buttons'>\n");
    fprintf(fp, "<button class='period-btn' data-index='0' onclick='switchPeriod(0)'>全部</button>\n");
    fprintf(fp, "<button class='period-btn' data-index='1' onclick='switchPeriod(1)'>最近一年</button>\n");
    fprintf(fp, "<button class='period-btn' data-index='2' onclick='switchPeriod(2)'>最近180天</button>\n");
    fprintf(fp, "<button class='period-btn' data-index='3' onclick='switchPeriod(3)'>最近1个月</button>\n");
    fprintf(fp, "</div>\n");
    fprintf(fp, "<div id='problemChart' class='chart-box'></div>\n");
    fprintf(fp, "</section>\n");

    fprintf(fp, "<section id='contests' class='card'><h2 class='section-title'>比赛记录</h2><table>\n");
    fprintf(fp, "<tr><th>比赛</th><th>时间</th><th>旧 Rating</th><th>新 Rating</th><th>排名</th><th>Δ</th><th>比赛 AC</th><th>补题</th></tr>\n");

    for (i = contest_cnt - 1; i >= 0; i--) {
        char date[32];
        strftime(date, sizeof(date), "%Y-%m-%d", localtime(&contests[i].time));
        int delta = contests[i].newRating - contests[i].oldRating;
        char sign[4] = ""; if (delta > 0) strcpy(sign, "+");

        char oc[20], nc[20];
        rating_color(contests[i].oldRating, oc);
        rating_color(contests[i].newRating, nc);

        fprintf(fp, "<tr><td>%s</td><td>%s</td>", contests[i].name, date);
        fprintf(fp, "<td style='color:%s;font-weight:700'>%d</td>", oc, contests[i].oldRating);
        fprintf(fp, "<td style='color:%s;font-weight:700'>%d</td>", nc, contests[i].newRating);
        fprintf(fp, "<td>%d</td>", contests[i].rank);
        fprintf(fp, "<td style='color:%s;font-weight:700'>%s%d</td>", delta >= 0 ? "#00a854" : "#e53935", sign, delta);
        fprintf(fp, "<td>%d</td><td>%d</td></tr>\n", contests[i].contestAC, contests[i].practiceAC);
    }

    fprintf(fp, "</table></section>\n</div>\n");

    fprintf(fp, "<script>\n");

    fprintf(fp, "const ratingChart = echarts.init(document.getElementById('ratingChart'));\n");
    fprintf(fp, "ratingChart.setOption({\n");
    fprintf(fp, "  tooltip: { trigger: 'axis' },\n");
    fprintf(fp, "  grid: { left: 50, right: 20, top: 30, bottom: 60 },\n");
    fprintf(fp, "  xAxis: {\n");
    fprintf(fp, "    type: 'category',\n");
    fprintf(fp, "    data: [");
    for (i = contest_cnt - 1; i >= 0; i--) {
        fprintf(fp, "'%s'", contests[i].name);
        if (i > 0) fprintf(fp, ",");
    }
    fprintf(fp, "],\n");
    fprintf(fp, "    axisLabel: { rotate: 40, fontSize: 11 }\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  yAxis: { type: 'value', name: 'Rating' },\n");
    fprintf(fp, "  series: [{\n");
    fprintf(fp, "    name: 'Rating',\n");
    fprintf(fp, "    type: 'line',\n");
    fprintf(fp, "    smooth: true,\n");
    fprintf(fp, "    data: [");
    for (i = contest_cnt - 1; i >= 0; i--) {
        fprintf(fp, "%d", contests[i].newRating);
        if (i > 0) fprintf(fp, ",");
    }
    fprintf(fp, "],\n");
    fprintf(fp, "    lineStyle: { width: 4, color: '#3a86ff' },\n");
    fprintf(fp, "    itemStyle: { color: '#8338ec' }\n");
    fprintf(fp, "  }]\n");
    fprintf(fp, "});\n");

    fprintf(fp, "const difficultyLabels = [");
    for (i = 0; i < 28; i++) {
        fprintf(fp, "'%d'", 800 + i * 100);
        if (i < 27) fprintf(fp, ",");
    }
    fprintf(fp, "];\n");

    fprintf(fp, "const problemData = [\n");
    fprintf(fp, "  [%d", all_rating_count[0]);
    for (i = 1; i < 28; i++) fprintf(fp, ",%d", all_rating_count[i]);
    fprintf(fp, "],\n");

    fprintf(fp, "  [%d", year_rating_count[0]);
    for (i = 1; i < 28; i++) fprintf(fp, ",%d", year_rating_count[i]);
    fprintf(fp, "],\n");

    fprintf(fp, "  [%d", half_year_rating_count[0]);
    for (i = 1; i < 28; i++) fprintf(fp, ",%d", half_year_rating_count[i]);
    fprintf(fp, "],\n");

    fprintf(fp, "  [%d", month_rating_count[0]);
    for (i = 1; i < 28; i++) fprintf(fp, ",%d", month_rating_count[i]);
    fprintf(fp, "]\n");
    fprintf(fp, "];\n");

    fprintf(fp, "const problemChart = echarts.init(document.getElementById('problemChart'));\n");
    fprintf(fp, "let currentPeriod = 0;\n");

    fprintf(fp, "function updateProblemChart(period) {\n");
    fprintf(fp, "  problemChart.setOption({\n");
    fprintf(fp, "    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },\n");
    fprintf(fp, "    grid: { left: 40, right: 20, top: 20, bottom: 40 },\n");
    fprintf(fp, "    xAxis: {\n");
    fprintf(fp, "      type: 'category',\n");
    fprintf(fp, "      data: difficultyLabels,\n");
    fprintf(fp, "      name: '题目难度'\n");
    fprintf(fp, "    },\n");
    fprintf(fp, "    yAxis: {\n");
    fprintf(fp, "      type: 'value',\n");
    fprintf(fp, "      name: '通过数量',\n");
    fprintf(fp, "      min: 0\n");
    fprintf(fp, "    },\n");
    fprintf(fp, "    series: [{\n");
    fprintf(fp, "      name: '题目数量',\n");
    fprintf(fp, "      type: 'bar',\n");
    fprintf(fp, "      data: problemData[period],\n");
    fprintf(fp, "      itemStyle: {\n");
    fprintf(fp, "        color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [\n");
    fprintf(fp, "          { offset: 0, color: '#3a86ff' },\n");
    fprintf(fp, "          { offset: 1, color: '#8338ec' }\n");
    fprintf(fp, "        ]),\n");
    fprintf(fp, "        borderRadius: [6, 6, 0, 0]\n");
    fprintf(fp, "      }\n");
    fprintf(fp, "    }]\n");
    fprintf(fp, "  });\n");
    fprintf(fp, "}\n");

    fprintf(fp, "function switchPeriod(index) {\n");
    fprintf(fp, "  currentPeriod = index;\n");
    fprintf(fp, "  updateProblemChart(index);\n");
    fprintf(fp, "  const buttons = document.querySelectorAll('.period-btn');\n");
    fprintf(fp, "  buttons.forEach(btn => btn.classList.remove('active'));\n");
    fprintf(fp, "  buttons.forEach(btn => {\n");
    fprintf(fp, "    if(parseInt(btn.dataset.index) === index) {\n");
    fprintf(fp, "      btn.classList.add('active');\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "  });\n");
    fprintf(fp, "}\n");

    fprintf(fp, "switchPeriod(0);\n");

    fprintf(fp, "document.querySelectorAll('a[href^=\"#\"]').forEach(a => {\n");
    fprintf(fp, "  a.addEventListener('click', e => {\n");
    fprintf(fp, "    e.preventDefault();\n");
    fprintf(fp, "    document.querySelector(a.getAttribute('href')).scrollIntoView({ behavior: 'smooth' });\n");
    fprintf(fp, "  });\n");
    fprintf(fp, "});\n");

    fprintf(fp, "window.addEventListener('resize', () => {\n");
    fprintf(fp, "  ratingChart.resize();\n");
    fprintf(fp, "  problemChart.resize();\n");
    fprintf(fp, "});\n");

    fprintf(fp, "</script>\n");
    fprintf(fp, "</body></html>\n");

    fclose(fp);
    printf("✅ 已生成 %s (提交记录: %d)\n", file, sub_cnt);

cleanup:
    if (mem.data) free(mem.data);
    if (curl) curl_easy_cleanup(curl);
}

void print_usage(const char* program) {
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

void show_menu() {
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

int main(int argc, char* argv[]) {
    struct UserStats users[MAX_USERS];
    int user_cnt = 0;
    char filename[512] = "data/users.txt";
    int single_user_mode = 0;
    char single_handle[128] = "";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            single_user_mode = 1;
            strncpy(single_handle, argv[i + 1], sizeof(single_handle) - 1);
            break;
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            strncpy(filename, argv[i + 1], sizeof(filename) - 1);
            break;
        }
    }

    if (!single_user_mode && argc == 1) {
        FILE* test = fopen(filename, "r");
        if (!test) {
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
            if (scanf("%d", &choice) != 1) {
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
            } else {
                printf("无效输入，请重新选择\n");
            }
        }
    }

    if (single_user_mode) {
        if (strlen(single_handle) == 0) {
            fprintf(stderr, "错误: -u 参数需要指定用户名\n");
            print_usage(argv[0]);
            return 1;
        }
        generate_user_page(single_handle, NULL, 1);
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
        generate_user_page(handle, &users[user_cnt], 0);
        printf(" ✅\n");
        user_cnt++;
    }

    fclose(in);

    FILE* out = fopen("index.html", "w");
    if (!out) {
        fprintf(stderr, "无法生成 index.html\n");
        return 1;
    }

    fprintf(out, "<!DOCTYPE html>\n<html lang='zh-CN'>\n<head>\n");
    fprintf(out, "<meta charset='UTF-8'>\n");
    fprintf(out, "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n");
    fprintf(out, "<title>Codeforces 用户列表</title>\n");
    fprintf(out, "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap' rel='stylesheet'>\n");
    fprintf(out, "<style>\n");
    fprintf(out, "*{margin:0;padding:0;box-sizing:border-box;font-family:'Inter','Segoe UI','Microsoft YaHei',sans-serif;}\n");
    fprintf(out, "body{background:#f5f7fc;color:#212529;line-height:1.6;}\n");
    fprintf(out, ".container{max-width:1400px;margin:0 auto;padding:40px 20px;}\n");
    fprintf(out, ".card{background:#fff;border-radius:18px;box-shadow:0 10px 30px rgba(0,0,0,.06);overflow:hidden;}\n");
    fprintf(out, ".card-header{background:linear-gradient(90deg,#3a86ff,#8338ec);color:#fff;padding:24px 32px;}\n");
    fprintf(out, ".card-header h1{font-size:1.8rem;margin:0;}\n");
    fprintf(out, "table{width:100%%;border-collapse:collapse;}\n");
    fprintf(out, "th{background:#f8f9fb;font-weight:600;padding:16px 12px;text-align:center;border-bottom:2px solid #e0e0e0;}\n");
    fprintf(out, "td{padding:14px 12px;text-align:center;border-bottom:1px solid #eee;transition:.2s;}\n");
    fprintf(out, "tr:hover{background:#f6f8ff;}\n");
    fprintf(out, ".user-link{text-decoration:none;color:#3a86ff;font-weight:700;font-size:1.1rem;transition:.2s;}\n");
    fprintf(out, ".user-link:hover{color:#8338ec;}\n");
    fprintf(out, ".legendary{color:#ff006e;font-weight:700;}\n");
    fprintf(out, ".master{color:#ff8c00;font-weight:700;}\n");
    fprintf(out, ".candidate{color:#aa00aa;font-weight:700;}\n");
    fprintf(out, ".expert{color:#2266cc;font-weight:700;}\n");
    fprintf(out, ".specialist{color:#03a89e;font-weight:700;}\n");
    fprintf(out, ".newbie{color:#808080;font-weight:700;}\n");
    fprintf(out, ".avatar{width:48px;height:48px;border-radius:50%%;vertical-align:middle;margin-right:8px;border:2px solid #eee;}\n");
    fprintf(out, ".stat-badge{display:inline-block;padding:4px 12px;border-radius:20px;font-size:13px;font-weight:600;}\n");
    fprintf(out, ".stat-badge.rating{background:#e8f4fd;color:#1e88e5;}\n");
    fprintf(out, ".stat-badge.contest{background:#fff3e0;color:#ff9800;}\n");
    fprintf(out, ".stat-badge.max{background:#f3e5f5;color:#7b1fa2;}\n");
    fprintf(out, "</style>\n");
    fprintf(out, "</head>\n<body>\n");
    fprintf(out, "<div class='container'>\n");
    fprintf(out, "<div class='card'>\n");
    fprintf(out, "<div class='card-header'><h1>Codeforces 用户列表</h1></div>\n");
    fprintf(out, "<table>\n");
    fprintf(out, "<tr><th>用户ID</th><th>头像</th><th>当前等级分</th><th>当前头衔</th><th>比赛次数</th><th>最高等级分</th><th>近180天比赛</th><th>近180天最高</th></tr>\n");

    for (int i = 0; i < user_cnt; i++) {
        char cls[30];
        rating_class(users[i].rating, cls);
        fprintf(out, "<tr>");
        fprintf(out, "<td><a href='%s.html' class='user-link'>%s</a></td>", users[i].handle, users[i].handle);
        fprintf(out, "<td><img src='%s' class='avatar' alt='%s'></td>", users[i].avatar_url, users[i].handle);
        fprintf(out, "<td><span class='%s'>%d</span></td>", cls, users[i].rating);
        fprintf(out, "<td>%s</td>", users[i].title);
        fprintf(out, "<td><span class='stat-badge contest'>%d</span></td>", users[i].total_contests);
        fprintf(out, "<td><span class='stat-badge max'>%d</span></td>", users[i].max_rating);
        fprintf(out, "<td><span class='stat-badge'>%d</span></td>", users[i].last180_contests);
        fprintf(out, "<td><span class='stat-badge'>%d</span></td>", users[i].last180_max_rating);
        fprintf(out, "</tr>\n");
    }

    fprintf(out, "</table>\n");
    fprintf(out, "</div>\n");
    fprintf(out, "</div>\n");
    fprintf(out, "</body>\n</html>");

    fclose(out);

    printf("✅ 已生成 index.html\n");
    return 0;
}