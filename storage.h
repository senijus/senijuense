#ifndef __STORAGE_H__
#define __STORAGE_H__

#include "mailbox.h"  // 引入Data_t结构体
#include "log.h"      // 引入日志接口
#include <sqlite3.h>
#include <pthread.h>

// 全局数据库句柄（仅内部使用，对外隐藏）
extern sqlite3 *g_db_handle;
// 数据库操作锁（保证线程安全）
extern pthread_mutex_t g_storage_lock;

/**
 * @brief 存储模块初始化（打开数据库+创建数据表）
 * @param db_path 数据库文件路径（如"/data/sensor.db"）
 * @return 0成功 / -1失败
 */
int StorageInit(const char *db_path);

/**
 * @brief 存储模块销毁（关闭数据库+销毁锁）
 * @return 0成功 / -1失败
 */
int StorageDeInit(void);

/**
 * @brief 插入传感器数据（供mailbox模块调用）
 * @param p_data 待存储的Data_t数据指针
 * @return 0成功 / -1失败
 */
int StorageInsertData(const Data_t *p_data);

/**
 * @brief 按时间范围查询数据（依赖sqlite3_exec回调解析结果）
 * @param start_time 开始时间（格式"YYYY-MM-DD HH:MM:SS"）
 * @param end_time 结束时间（格式"YYYY-MM-DD HH:MM:SS"）
 * @param p_result 输出：查询结果数组（需提前分配内存）
 * @param p_count 输出：实际查询到的数据条数
 * @return 0成功 / -1失败
 */
int StorageQueryData(const char *start_time, const char *end_time, Data_t *p_result, int *p_count);

/**
 * @brief 清理过期数据（参考LogCleanOldFiles逻辑）
 * @param keep_days 保留天数
 * @return 0成功 / -1失败
 */
int StorageCleanOldData(int keep_days);

#endif