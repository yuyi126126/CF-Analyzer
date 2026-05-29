#ifndef CF_API_H
#define CF_API_H

#include "cf_types.h"

size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata);
void rating_color(int r, char* c);
void rating_class(int r, char* cls);
int fetch_user_info(const char* handle, int* rating, char* title, char* avatar_url);
int fetch_user_contests(const char* handle, struct Contest* contests, int* count);
int fetch_user_submissions(const char* handle, struct Submission* subs, int* count);
void analyze_submissions(struct Submission* subs, int sub_cnt, struct Contest* contests, int contest_cnt,
                         int* ac_count, int* practice_count,
                         int all_rating_count[], int year_rating_count[],
                         int half_year_rating_count[], int month_rating_count[]);
void wait_for_rate_limit();
#endif