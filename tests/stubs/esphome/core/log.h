#pragma once

inline void mock_log(const char *, const char *, ...) {}
#define ESP_LOGCONFIG(...) mock_log(__VA_ARGS__)
#define ESP_LOGI(...) mock_log(__VA_ARGS__)
#define ESP_LOGD(...) mock_log(__VA_ARGS__)
#define ESP_LOGW(...) mock_log(__VA_ARGS__)
#define LOG_SENSOR(prefix, type, sensor) (void) (sensor)
