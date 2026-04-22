/*
 * framePipeline.cpp
 *
 * Implementation of PipelineRunner (LatestWinsSlot<T> is header-only).
 */

#include "framePipeline.h"

// ---------------------------------------------------------------------------
// PipelineRunner
// ---------------------------------------------------------------------------

void PipelineRunner::start(std::function<void()> captureFn,
                            std::function<void()> transformFn,
                            std::function<void()> dlFn)
{
    startThread(captureThread_,   std::move(captureFn));
    startThread(transformThread_, std::move(transformFn));
    startThread(dlThread_,        std::move(dlFn));
}

void PipelineRunner::stopAndJoin()
{
    if (captureThread_.joinable())   captureThread_.join();
    if (transformThread_.joinable()) transformThread_.join();
    if (dlThread_.joinable())        dlThread_.join();
}

void PipelineRunner::startThread(std::thread& t, std::function<void()> fn)
{
    if (fn && !t.joinable()) {
        t = std::thread(std::move(fn));
    }
}
