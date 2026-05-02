#ifndef EXECUTION_LIMITER_H
#define EXECUTION_LIMITER_H

#include <chrono>
#include <stdexcept>

// 실행 시간, 루프 반복 제한 관리
class ExecutionLimiter {
public:
    // 기본값
    static constexpr long long DEFAULT_TIMEOUT_MS = 5000;      // 5초
    static constexpr long long DEFAULT_MAX_LOOP_ITERATIONS = 1000000;

    explicit ExecutionLimiter(
        long long timeoutMs = DEFAULT_TIMEOUT_MS,
        long long maxLoopIterations = DEFAULT_MAX_LOOP_ITERATIONS
    )
        : maxTimeoutMs(timeoutMs)
        , maxLoopIterations(maxLoopIterations)
        , loopIterationCount(0)
    {
        startTime = std::chrono::steady_clock::now();
    }

    // 실행 시간 체크
    bool isTimeoutExceeded() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        return elapsed > maxTimeoutMs;
    }

    long long getElapsedMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
    }

    // 루프 반복 추적
    void incrementLoopCounter() {
        if (++loopIterationCount > maxLoopIterations) {
            throw std::runtime_error("루프 반복 횟수 초과: " + std::to_string(maxLoopIterations));
        }
    }

    void resetLoopCounter() {
        loopIterationCount = 0;
    }

    long long getLoopIterationCount() const {
        return loopIterationCount;
    }

private:
    std::chrono::steady_clock::time_point startTime;
    long long maxTimeoutMs;
    long long maxLoopIterations;
    long long loopIterationCount;
};

#endif // EXECUTION_LIMITER_H
