/*
 * framePipeline.h
 *
 * Thread pipeline infrastructure for the NSL capture pipeline.
 *
 * Provides:
 *   - LatestWinsSlot<T>  : thread-safe single-slot buffer (producer always
 *                          overwrites; consumer always sees the newest value)
 *   - PipelineRunner     : manages up to 3 worker threads (capture / transform
 *                          / dl) with clean stop-and-join semantics
 *
 * No nanolib or OpenCV dependencies — pure C++14 standard library.
 */

#ifndef FRAME_PIPELINE_H
#define FRAME_PIPELINE_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

// ---------------------------------------------------------------------------
// LatestWinsSlot<T>
//
// A single-slot, thread-safe handoff channel where a fast producer drops
// stale data and a slow consumer always reads the most-recent value.
//
// Ownership is transferred via std::unique_ptr so T is never copied.
// ---------------------------------------------------------------------------
template <typename T>
class LatestWinsSlot {
public:
    LatestWinsSlot() = default;

    // Non-copyable, non-movable (owns a mutex)
    LatestWinsSlot(const LatestWinsSlot&)            = delete;
    LatestWinsSlot& operator=(const LatestWinsSlot&) = delete;

    // -----------------------------------------------------------------------
    // put() — producer side, never blocks.
    // Overwrites any previously un-consumed value (old value is freed).
    // -----------------------------------------------------------------------
    void put(std::unique_ptr<T> val) {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            slot_     = std::move(val);
            has_data_ = true;
        }
        cv_.notify_one();
    }

    // -----------------------------------------------------------------------
    // take() — consumer side, blocks until data is available or stop() is
    // called.  Returns nullptr if the slot was stopped with no pending data.
    // -----------------------------------------------------------------------
    std::unique_ptr<T> take() {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait(lk, [this] { return has_data_ || stopped_; });

        if (stopped_ && !has_data_) {
            return nullptr;
        }

        std::unique_ptr<T> val = std::move(slot_);
        has_data_ = false;
        return val;
    }

    // -----------------------------------------------------------------------
    // tryTake() — non-blocking attempt.  Returns nullptr if no data is ready.
    // -----------------------------------------------------------------------
    std::unique_ptr<T> tryTake() {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!has_data_) {
            return nullptr;
        }
        std::unique_ptr<T> val = std::move(slot_);
        has_data_ = false;
        return val;
    }

    // -----------------------------------------------------------------------
    // stop() — signals all blocked take() callers to wake and return nullptr.
    // -----------------------------------------------------------------------
    void stop() {
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

private:
    std::mutex              mtx_;
    std::condition_variable cv_;
    std::unique_ptr<T>      slot_;
    bool                    has_data_ = false;
    bool                    stopped_  = false;
};

// ---------------------------------------------------------------------------
// PipelineRunner
//
// Owns up to three std::thread objects (capture, transform, dl).  Each thread
// runs the provided std::function<void()> until that function returns (i.e.
// until the caller's loop detects the stop signal via its slot).
//
// The runner itself does NOT manage stop flags — callers drive shutdown by
// calling stop() on the relevant LatestWinsSlot objects, which makes the
// worker functions return naturally.
// ---------------------------------------------------------------------------
class PipelineRunner {
public:
    PipelineRunner()  = default;
    ~PipelineRunner() { stopAndJoin(); }

    // Non-copyable, non-movable
    PipelineRunner(const PipelineRunner&)            = delete;
    PipelineRunner& operator=(const PipelineRunner&) = delete;

    // -----------------------------------------------------------------------
    // start() — launch worker threads.  Pass nullptr (or empty function) for
    // any stage that should be skipped.
    // -----------------------------------------------------------------------
    void start(std::function<void()> captureFn,
               std::function<void()> transformFn,
               std::function<void()> dlFn);

    // -----------------------------------------------------------------------
    // stopAndJoin() — join all threads that are still joinable.
    // Safe to call multiple times (idempotent after first call).
    // -----------------------------------------------------------------------
    void stopAndJoin();

private:
    void startThread(std::thread& t, std::function<void()> fn);

    std::thread captureThread_;
    std::thread transformThread_;
    std::thread dlThread_;
};

#endif // FRAME_PIPELINE_H
