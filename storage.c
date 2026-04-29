#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 全局变量（对外隐藏，仅内部使用）
sqlite3 *g_db_handle = NULL;
pthread_mutex_t g_storage_lock;

// 查询回调函数（给sqlite3_exec用，解析查询结果到Data_t数组）
// 注意：参数3的void*需传递{Data_t数组, 计数}的结构体，适配多结果
typedef struct {
    Data_t *p_data_arr;  // 结果数组
    int *p_count;        // 已解析的条数
    int max_count;       // 数组最大容量
} QueryCallbackParam_t;

// sqlite3_exec的查询回调函数（核心：解析每行数据到Data_t）
static int query_data_callback(void *param, int col_num, char **col_val, char **col_name) {
    QueryCallbackParam_t *p_cb_param = (QueryCallbackParam_t *)param;
    // 防止数组越界
    if (*(p_cb_param->p_count) >= p_cb_param->max_count) {
        return 1;  // 终止回调
    }
    // 解析列值到Data_t（对应sensor_data表的字段：timestamp, temperature, humidity, voltage, electricity）
    // 跳过id列，从第2列开始解析（timestamp是第2列，索引1；temperature索引2，以此类推）
    Data_t *p_data = &(p_cb_param->p_data_arr[*(p_cb_param->p_count)]);
    p_data->Tem = atoi(col_val[2]);    // temperature列
    p_data->Hum = atoi(col_val[3]);     // humidity列
    p_data->Volt = atoi(col_val[4]);    // voltage列
    p_data->Elec = atoi(col_val[5]);    // electricity列
    (*p_cb_param->p_count)++;
    return 0;  // 继续回调
}

// 初始化：打开数据库+创建数据表
int StorageInit(const char *db_path) {
    int ret = 0;
    char *err_msg = NULL;

    // 初始化线程锁（参考mailbox的锁初始化逻辑）
    ret = pthread_mutex_init(&g_storage_lock, NULL);
    if (ret != 0) {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "StorageInit: mutex init fail: %s", strerror(ret));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        return -1;
    }

    // 打开数据库（sqlite3_open）
    ret = sqlite3_open(db_path, &g_db_handle);
    if (ret != SQLITE_OK) {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "StorageInit: sqlite3_open fail: %s", sqlite3_errmsg(g_db_handle));
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        sqlite3_close(g_db_handle);  // 失败时关闭句柄
        pthread_mutex_destroy(&g_storage_lock);
        return -1;
    }

    // 建表SQL（适配Data_t，仅用sqlite3_exec执行）
    const char *create_sql = 
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "timestamp DATETIME DEFAULT (datetime('now', 'localtime')),"  // 自动填充本地时间
        "temperature REAL NOT NULL,"
        "humidity REAL NOT NULL,"
        "voltage REAL NOT NULL,"
        "electricity REAL NOT NULL);"
        "CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_data(timestamp);";
    
    // 执行建表SQL（sqlite3_exec）
    ret = sqlite3_exec(g_db_handle, create_sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "StorageInit: create table fail: %s", err_msg);
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        sqlite3_free(err_msg);  // 释放错误信息
        sqlite3_close(g_db_handle);
        pthread_mutex_destroy(&g_storage_lock);
        return -1;
    }

    LogWrite(LOG_LEVEL_INFO, "StorageInit: sqlite3 init success");
    return 0;
}

// 销毁：关闭数据库+销毁锁
int StorageDeInit(void) {
    // 加锁保护
    pthread_mutex_lock(&g_storage_lock);

    // 关闭数据库（sqlite3_close）
    if (g_db_handle != NULL) {
        sqlite3_close(g_db_handle);
        g_db_handle = NULL;
    }

    // 销毁锁
    pthread_mutex_unlock(&g_storage_lock);
    pthread_mutex_destroy(&g_storage_lock);

    LogWrite(LOG_LEVEL_INFO, "StorageDeInit: sqlite3 deinit success");
    return 0;
}

// 插入数据：拼接INSERT SQL，用sqlite3_exec执行
int StorageInsertData(const Data_t *p_data) {
    if (p_data == NULL || g_db_handle == NULL) {
        LogWrite(LOG_LEVEL_ERROR, "StorageInsertData: invalid param");
        return -1;
    }

    int ret = 0;
    char insert_sql[512] = {0};
    char *err_msg = NULL;

    // 加锁（参考mailbox的锁逻辑）
    pthread_mutex_lock(&g_storage_lock);

    // 拼接插入SQL（适配Data_t字段）
    snprintf(insert_sql, sizeof(insert_sql),
             "INSERT INTO sensor_data (temperature, humidity, voltage, electricity) "
             "VALUES (%.2lf, %.2lf, %.2lf, %.2lf);",
             p_data->Tem, p_data->Hum, p_data->Volt, p_data->Elec);
    
    // 执行插入（sqlite3_exec）
    ret = sqlite3_exec(g_db_handle, insert_sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "StorageInsertData: insert fail: %s", err_msg);
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        sqlite3_free(err_msg);
        pthread_mutex_unlock(&g_storage_lock);
        return -1;
    }

    // 解锁+日志
    pthread_mutex_unlock(&g_storage_lock);
    char log_buf[256] = {0};
    snprintf(log_buf, sizeof(log_buf), 
         "StorageInsertData: insert success, Tem=%.2lf, Hum=%.2lf, Volt=%.2lf, Elec=%.2lf", 
         p_data->Tem, p_data->Hum, p_data->Volt, p_data->Elec);
    LogWrite(LOG_LEVEL_INFO, log_buf);
    return 0;
}

// 查询数据：拼接SELECT SQL，通过回调解析结果
int StorageQueryData(const char *start_time, const char *end_time, Data_t *p_result, int *p_count) {
    if (start_time == NULL || end_time == NULL || p_result == NULL || p_count == NULL || g_db_handle == NULL) {
        LogWrite(LOG_LEVEL_ERROR, "StorageQueryData: invalid param");
        return -1;
    }

    int ret = 0;
    char select_sql[512] = {0};
    char *err_msg = NULL;
    QueryCallbackParam_t cb_param = {0};

    // 初始化回调参数
    cb_param.p_data_arr = p_result;
    cb_param.p_count = p_count;
    cb_param.max_count = *p_count;  // 传入的count为数组最大容量，输出实际条数
    *p_count = 0;  // 重置计数

    // 加锁
    pthread_mutex_lock(&g_storage_lock);

    // 拼接查询SQL（按时间范围）
    snprintf(select_sql, sizeof(select_sql),
             "SELECT * FROM sensor_data WHERE timestamp >= '%s' AND timestamp <= '%s';",
             start_time, end_time);
    
    // 执行查询（sqlite3_exec+回调函数）
    ret = sqlite3_exec(g_db_handle, select_sql, query_data_callback, &cb_param, &err_msg);
    if (ret != SQLITE_OK) {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "StorageQueryData: query fail: %s", err_msg);
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        sqlite3_free(err_msg);
        pthread_mutex_unlock(&g_storage_lock);
        return -1;
    }

    // 解锁+日志
    pthread_mutex_unlock(&g_storage_lock);
    char log_buf[256] = {0};
    snprintf(log_buf, sizeof(log_buf), "StorageQueryData: query success, count=%d", *p_count);
    LogWrite(LOG_LEVEL_INFO, log_buf);
    return 0;
}

// 清理过期数据：拼接DELETE SQL，用sqlite3_exec执行
int StorageCleanOldData(int keep_days) {
    if (keep_days < 0 || g_db_handle == NULL) {
        LogWrite(LOG_LEVEL_ERROR, "StorageCleanOldData: invalid param");
        return -1;
    }

    int ret = 0;
    char delete_sql[512] = {0};
    char *err_msg = NULL;

    // 加锁
    pthread_mutex_lock(&g_storage_lock);

    // 拼接删除SQL（计算过期时间：当前时间 - keep_days天）
    snprintf(delete_sql, sizeof(delete_sql),
             "DELETE FROM sensor_data WHERE timestamp < datetime('now', 'localtime', '-%d day');",
             keep_days);
    
    // 执行删除（sqlite3_exec）
    ret = sqlite3_exec(g_db_handle, delete_sql, NULL, NULL, &err_msg);
    if (ret != SQLITE_OK) {
        char log_buf[256] = {0};
        snprintf(log_buf, sizeof(log_buf), "StorageCleanOldData: delete fail: %s", err_msg);
        LogWrite(LOG_LEVEL_ERROR, log_buf);
        sqlite3_free(err_msg);
        pthread_mutex_unlock(&g_storage_lock);
        return -1;
    }

    // 解锁+日志
    pthread_mutex_unlock(&g_storage_lock);
    char log_buf[256] = {0};
    snprintf(log_buf, sizeof(log_buf), "StorageCleanOldData: clean success, keep %d days", keep_days);
    LogWrite(LOG_LEVEL_INFO, log_buf);
    return 0;
}