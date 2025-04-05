#include <pthread.h>
#include <queue>
#include "../utils/vector.h"
#pragma once


#define THREAD_POOL_SIZE 10

struct Task {
    void (*func)(void*);  
    void* arg;            
};

class ThreadPool {
private:
    std::vector<pthread_t> workers;
    std::queue<Task> taskQueue;
    pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t queueCond = PTHREAD_COND_INITIALIZER;
    bool stop = false;

    // Worker thread function
    static void* workerThread(void* arg) {
        ThreadPool* pool = static_cast<ThreadPool*>(arg);

        while (true) {
            pthread_mutex_lock(&pool->queueMutex);

            // Wait for a task
            while (pool->taskQueue.empty() && !pool->stop) {
                pthread_cond_wait(&pool->queueCond, &pool->queueMutex);
            }

            // Stop signal
            if (pool->stop && pool->taskQueue.empty()) {
                pthread_mutex_unlock(&pool->queueMutex);
                break;
            }

            // Get task from queue
            Task task = pool->taskQueue.front();
            pool->taskQueue.pop();

            pthread_mutex_unlock(&pool->queueMutex);

            // Execute task
            task.func(task.arg);
        }
        return nullptr;
    }

public:
    ThreadPool(size_t numThreads = THREAD_POOL_SIZE) {
        for (size_t i = 0; i < numThreads; ++i) {
            pthread_t thread;
            pthread_create(&thread, nullptr, workerThread, this);
            workers.push_back(thread);
        }
    }

    void submit(void (*func)(void*), void* arg) {
        pthread_mutex_lock(&queueMutex);
        if (taskQueue.size() < THREAD_POOL_SIZE * 10)
            taskQueue.push(Task{func, arg});
        pthread_cond_signal(&queueCond);
        pthread_mutex_unlock(&queueMutex);
    }

    void wake() {
        pthread_cond_signal(&queueCond);
    }

    bool isFull() {
        if (taskQueue.size() >= THREAD_POOL_SIZE + 10)
            return true;
        return false;
    }

    ~ThreadPool() {
        pthread_mutex_lock(&queueMutex);
        stop = true;
        pthread_cond_broadcast(&queueCond);
        pthread_mutex_unlock(&queueMutex);

        for (pthread_t thread : workers) {
            pthread_join(thread, nullptr);
        }

        pthread_mutex_destroy(&queueMutex);
        pthread_cond_destroy(&queueCond);
    }
};

