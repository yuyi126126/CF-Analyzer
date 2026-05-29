#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include "../include/cJSON.h"
#include "../include/cf_types.h"
#include "../include/cf_api.h"

// 生成用户页面函数（根据用户句柄和统计信息生成HTML页面）
// 参数：handle - 用户句柄；stats - 用户统计信息；single_mode - 单用户模式标志（如果为1，则生成的页面不包含返回列表的链接）
void generate_user_page(const char* handle, struct UserStats* stats, int single_mode) { 
// 第一部分：初始化CURL和内存结构体（为HTTP请求和响应数据准备环境）+ 设置CURL选项（配置HTTP请求的参数和回调函数） 
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
// 第二部分：获取用户信息（用户当前等级分、头衔和头像URL）
    int rating = 0;
    char title[128] = "Unrated";                   // 默认头衔（如果用户没有评级，则显示为Unrated）
    char avatar_url[512] = "https://userpic.codeforces.org/no-title.jpg";
    fetch_user_info(handle, &rating, title, avatar_url);
// 第三部分：获取用户比赛记录（总比赛次数，近180天比赛次数，最高等级分，近180天最高等级分）
    struct Contest contests[MAX_CONTEST];
    int contest_cnt = 0; 
    int total = 0, last180 = 0;
    int maxRating = 0, maxRating180 = 0;
    fetch_user_contests(handle, contests, &contest_cnt);
    total = contest_cnt;
    for (int i = 0; i < contest_cnt; i++) {
        if (contests[i].newRating > maxRating)
            maxRating = contests[i].newRating;
        time_t now = time(NULL);
        if (difftime(now, contests[i].time) <= 180.0 * 24 * 3600) {   // 近180天比赛统计（如果比赛记录中有ratingUpdateTimeSeconds字段且比赛时间在当前时间的180天内，则统计近180天的比赛次数和最高等级分）
            last180++;
            if (contests[i].newRating > maxRating180)
                maxRating180 = contests[i].newRating;
        }
    }
// 第四部分：分析提交记录（通过题目数量，补题数量，不同难度等级的题目分布：分所有，年，半年，月四种情况）
    struct Submission subs[MAX_SUBMISSIONS];// 提交记录数组（用于存储从Codeforces API获取的提交记录，最多存储MAX_SUBMISSIONS条记录）
    int sub_cnt = 0;
    fetch_user_submissions(handle, subs, &sub_cnt);
    int ac = 0, practice = 0;
    int all_rating_count[28] = { 0 };
    int year_rating_count[28] = { 0 };
    int half_year_rating_count[28] = { 0 };
    int month_rating_count[28] = { 0 };
    analyze_submissions(subs, sub_cnt, contests, contest_cnt,
                       &ac, &practice,
                       all_rating_count, year_rating_count,
                       half_year_rating_count, month_rating_count);
    if (stats != NULL) {                       // 送进来了一个用户统计信息的盒子，告诉我们要用多用户模式了，我们就把统计信息放到这个盒子里，等会儿生成索引页面的时候就能用上了
        strcpy(stats->handle, handle);         // 填充的信息包括（用户句柄、等级分、头衔、头像URL、总比赛次数、最高等级分、近180天比赛次数、近180天最高等级分、AC数量和补题数量）
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
    char file[256];                            // 生成HTML文件（根据用户句柄生成HTML文件名，例如handle为"tourist"则生成"tourist.html"）
    snprintf(file, sizeof(file), "%s.html", handle);
    FILE* fp = fopen(file, "w");
    if (!fp) goto cleanup;                     // 文件打开失败，跳转到清理代码
    char cls[30];                              // 获取用户等级类（根据用户评级获取对应的等级类名称，用于HTML页面中的CSS样式）
    rating_class(rating, cls);                 
// 第五部分：生成HTML页面
    // 初始化配置（生成HTML页面的头部，包括页面标题、引入ECharts库和Google Fonts）
    fprintf(fp, "<!DOCTYPE html>\n<html lang='zh-CN'>\n<head>\n");
    fprintf(fp, "<meta charset='UTF-8'>\n");
    fprintf(fp, "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n");
    fprintf(fp, "<title>%s | Codeforces Analysis</title>\n", handle);
    fprintf(fp, "<script src='https://cdn.jsdelivr.net/npm/echarts@5.4.3/dist/echarts.min.js'></script>\n");  // 引入ECharts库（用于生成图表，展示用户的Rating变化趋势和题目难度分布）
    fprintf(fp, "<link href='https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap' rel='stylesheet'>\n");// 引入Google Fonts（用于页面的字体样式，提升页面的美观度和可读性）
    // CSS样式（生成HTML页面的内嵌CSS样式，定义页面的布局、颜色、字体等视觉效果）
    fprintf(fp, "<style>\n");
    fprintf(fp, "*{margin:0;padding:0;box-sizing:border-box;font-family:'Inter','Segoe UI','Microsoft YaHei',sans-serif;}\n");
    fprintf(fp, "body{background:#f5f7fc;color:#212529;line-height:1.6;}\n");
    fprintf(fp, ".container{max-width:1200px;margin:0 auto;padding:0 20px;}\n");
    fprintf(fp, "header{position:fixed;top:0;left:0;width:100%%;z-index:999;background:rgba(255,255,255,.95);backdrop-filter:blur(10px);box-shadow:0 5px 15px rgba(0,0,0,.08);}\n");
    fprintf(fp, "nav{display:flex;justify-content:space-between;align-items:center;padding:14px 0;}\n");
        // logo样式（定义页面顶部logo的样式，使用线性渐变和文本剪裁效果，使logo具有炫酷的视觉效果）
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
    // 页面结构（生成HTML页面的主体结构，包括固定顶部导航栏、用户信息卡片、Rating变化趋势图表、题目难度分布图表和比赛记录表格）
    fprintf(fp, "<header><nav class='container'><div class='logo'>CF Analyzer</div><ul class='nav-links'>");
    fprintf(fp, "<li><a href='#overview'>概览</a></li>");
    fprintf(fp, "<li><a href='#rating'>Rating</a></li>");
    fprintf(fp, "<li><a href='#problems'>题目分析</a></li>");
    fprintf(fp, "<li><a href='#contests'>比赛记录</a></li>");
    if (!single_mode) {   // 如果不是单用户模式，添加返回列表的链接
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
       // Rating变化趋势图表（生成一个ECharts图表，用于展示用户的Rating变化趋势，X轴为比赛名称，Y轴为Rating值）
    fprintf(fp, "<section id='rating' class='card'><h2 class='section-title'>Rating 变化趋势</h2><div id='ratingChart' class='chart-box'></div></section>\n");
       // 题目难度分布图表（生成一个ECharts图表，用于展示用户通过的题目的难度分布，提供不同时间范围的切换按钮）
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
       // 比赛记录表格（生成一个HTML表格，用于展示用户的比赛记录，包括比赛名称、时间、旧Rating、新Rating、排名、Rating变化、比赛AC数量和补题数量）
    fprintf(fp, "<section id='contests' class='card'><h2 class='section-title'>比赛记录</h2><table>\n");
    fprintf(fp, "<tr><th>比赛</th><th>时间</th><th>旧 Rating</th><th>新 Rating</th><th>排名</th><th>Δ</th><th>比赛 AC</th><th>补题</th></tr>\n");
    for (int i = contest_cnt - 1; i >= 0; i--) {
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
    // JavaScript代码（生成HTML页面的内嵌JavaScript代码，用于初始化ECharts图表并设置图表选项，展示用户的Rating变化趋势和题目难度分布）
    fprintf(fp, "<script>\n");
         //画rating变化趋势图
    fprintf(fp, "const ratingChart = echarts.init(document.getElementById('ratingChart'));\n"); // 初始化echart画布
    fprintf(fp, "ratingChart.setOption({\n");                              // 设置图表选项（配置图表的标题、提示框、网格、坐标轴和数据系列等选项，展示用户的Rating变化趋势）
    fprintf(fp, "  tooltip: { trigger: 'axis' },\n");                      // 提示框配置
    fprintf(fp, "  grid: { left: 50, right: 20, top: 30, bottom: 60 },\n");// 网格配置
    fprintf(fp, "  xAxis: {\n");                                           // X轴配置
    fprintf(fp, "    type: 'category',\n");
    fprintf(fp, "    data: [");
    for (int i = contest_cnt - 1; i >= 0; i--) {
        fprintf(fp, "'%s'", contests[i].name);
        if (i > 0) fprintf(fp, ",");
    }
    fprintf(fp, "],\n");
    fprintf(fp, "    axisLabel: { rotate: 40, fontSize: 11 }\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  yAxis: { type: 'value', name: 'Rating' },\n");         // Y轴配置
    fprintf(fp, "  series: [{\n");
    fprintf(fp, "    name: 'Rating',\n");
    fprintf(fp, "    type: 'line',\n");                                   // 折线图配置
    fprintf(fp, "    smooth: true,\n");
    fprintf(fp, "    data: [");
    for (int i = contest_cnt - 1; i >= 0; i--) {
        fprintf(fp, "%d", contests[i].newRating);
        if (i > 0) fprintf(fp, ",");
    }
    fprintf(fp, "],\n");
    fprintf(fp, "    lineStyle: { width: 4, color: '#3a86ff' },\n");
    fprintf(fp, "    itemStyle: { color: '#8338ec' }\n");
    fprintf(fp, "  }]\n");
    fprintf(fp, "});\n");
        // 画题目难度分布图
          //数据准备
    fprintf(fp, "const difficultyLabels = [");    // 生成题目难度标签（x轴）（从800到3500，每100一个等级，共28个等级）
    for (int i = 0; i < 28; i++) {
        fprintf(fp, "'%d'", 800 + i * 100);
        if (i < 27) fprintf(fp, ",");
    }
    fprintf(fp, "];\n");
    fprintf(fp, "const problemData = [\n");       // 生成题目难度分布数据（y轴）（根据统计的题目难度分布，生成一个二维数组，分别对应全部时间、最近一年、最近180天和最近1个月的题目数量）
    fprintf(fp, "  [%d", all_rating_count[0]);
    for (int i = 1; i < 28; i++) fprintf(fp, ",%d", all_rating_count[i]);
    fprintf(fp, "],\n");
    fprintf(fp, "  [%d", year_rating_count[0]);
    for (int i = 1; i < 28; i++) fprintf(fp, ",%d", year_rating_count[i]);
    fprintf(fp, "],\n");
    fprintf(fp, "  [%d", half_year_rating_count[0]);
    for (int i = 1; i < 28; i++) fprintf(fp, ",%d", half_year_rating_count[i]);
    fprintf(fp, "],\n");
    fprintf(fp, "  [%d", month_rating_count[0]);
    for (int i = 1; i < 28; i++) fprintf(fp, ",%d", month_rating_count[i]);
    fprintf(fp, "]\n");
    fprintf(fp, "];\n");
        // 图表初始化
    fprintf(fp, "const problemChart = echarts.init(document.getElementById('problemChart'));\n");
    fprintf(fp, "let currentPeriod = 0;\n");                // 记录现在显示的时间段
    fprintf(fp, "function updateProblemChart(period) {\n"); // （函数）更新题目难度分布图表（根据选择的时间段，更新图表的数据和标题，展示对应时间范围内的题目难度分布）
    fprintf(fp, "  problemChart.setOption({\n");            // 设置图表选项
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
    fprintf(fp, "function switchPeriod(index) {\n");        //（函数）切换时间段
    fprintf(fp, "  currentPeriod = index;\n");
    fprintf(fp, "  updateProblemChart(index);\n");          // 调用更新函数，重绘图表
    fprintf(fp, "  const buttons = document.querySelectorAll('.period-btn');\n");
    fprintf(fp, "  buttons.forEach(btn => btn.classList.remove('active'));\n");
    fprintf(fp, "  buttons.forEach(btn => {\n");
    fprintf(fp, "    if(parseInt(btn.dataset.index) === index) {\n");
    fprintf(fp, "      btn.classList.add('active');\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "  });\n");
    fprintf(fp, "}\n");
    fprintf(fp, "switchPeriod(0);\n");                     // 默认显示全部时间段的题目难度分布
    fprintf(fp, "document.querySelectorAll('a[href^=\"#\"]').forEach(a => {\n"); // 平滑滚动（为页面内的锚点链接添加点击事件监听，实现平滑滚动效果，提升用户体验）
    fprintf(fp, "  a.addEventListener('click', e => {\n");
    fprintf(fp, "    e.preventDefault();\n");
    fprintf(fp, "    document.querySelector(a.getAttribute('href')).scrollIntoView({ behavior: 'smooth' });\n");
    fprintf(fp, "  });\n");
    fprintf(fp, "});\n");
    fprintf(fp, "window.addEventListener('resize', () => {\n");// 实现自适应屏幕窗口功能
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

void generate_index_page(struct UserStats* users, int user_cnt) {
    FILE* out = fopen("index.html", "w");
    if (!out) {
        fprintf(stderr, "无法生成 index.html\n");
        return;
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

    for (int i = 0; i < user_cnt; i++) { // 用户列表表格（循环生成用户列表的表格行，展示每个用户的基本信息和统计数据，并根据用户的等级分设置对应的CSS类，以显示不同颜色的等级分）
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
    fprintf(out, "</body></html>");
    fclose(out);
    printf("✅ 已生成 index.html\n");
}