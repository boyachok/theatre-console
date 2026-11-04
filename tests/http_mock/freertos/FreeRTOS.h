#pragma once
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
constexpr int pdTRUE=1,pdPASS=1,portMAX_DELAY=-1;
#define pdMS_TO_TICKS(ms) (ms)
struct MockQueue{std::mutex mutex;std::condition_variable ready;std::deque<void*> items;};
struct MockSemaphore{std::mutex mutex;std::condition_variable ready;bool done=false;};
using QueueHandle_t=MockQueue*;using SemaphoreHandle_t=MockSemaphore*;
inline std::vector<MockQueue*> mockQueues;
inline QueueHandle_t xQueueCreate(int,size_t){auto q=new MockQueue;mockQueues.push_back(q);return q;}
inline int xQueueSend(QueueHandle_t q,const void *data,int){std::lock_guard<std::mutex> lock(q->mutex);void *p;memcpy(&p,data,sizeof(p));q->items.push_back(p);q->ready.notify_one();return pdTRUE;}
inline int xQueueReceive(QueueHandle_t q,void *data,int wait){std::unique_lock<std::mutex> lock(q->mutex);if(wait)q->ready.wait(lock,[&]{return !q->items.empty();});if(q->items.empty())return 0;void *p=q->items.front();q->items.pop_front();memcpy(data,&p,sizeof(p));return pdTRUE;}
inline SemaphoreHandle_t xSemaphoreCreateBinary(){return new MockSemaphore;}
inline int xSemaphoreTake(SemaphoreHandle_t s,int){std::unique_lock<std::mutex> lock(s->mutex);s->ready.wait(lock,[&]{return s->done;});return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t s){std::lock_guard<std::mutex> lock(s->mutex);s->done=true;s->ready.notify_one();}
inline void vSemaphoreDelete(SemaphoreHandle_t s){delete s;}
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void*,int){return pdPASS;}
inline void vTaskDelay(int ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
