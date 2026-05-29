#ifndef CF_TYPES_H   // 头文件保护（防止重复包含）
#define CF_TYPES_H   // 数据类型定义（定义了内存结构体、比赛记录结构体、提交记录结构体和用户统计信息结构体）

#define MAX_CONTEST 1000
#define MAX_SUBMISSIONS 10000
#define MAX_USERS 100

struct Memory {
    char* data;
    size_t size;
};

struct Contest {
    char name[128];
    long long time;
    int oldRating;
    int newRating;
    int rank;
    int contestAC;
    int practiceAC;
};

struct Submission {
    char verdict[32];
    char participantType[32];
    long long creationTimeSeconds;
    int problemRating;
};

struct UserStats {
    char handle[128];
    int rating;
    char title[128];
    char avatar_url[512];
    int total_contests;
    int max_rating;
    int last180_contests;
    int last180_max_rating;
    int ac_count;
    int practice_count;
};

#endif