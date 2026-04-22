/*
 * Copyright (c) 2013, NANOSYSTEMS CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#ifdef _WINDOWS

#include "videoSource.h"
#include "framePipeline.h"
#include <windows.h>
int signal_recieved = 0;

BOOL WINAPI ctrl_handler(DWORD ctrlType)
{
	if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT) {
		printf("received Ctrl+C\n");
		signal_recieved = 1;
		return TRUE;
	}
	return FALSE;
}

#else // linux
#include <stdio.h>
#include <signal.h>

#include "videoSource.h"
#include "framePipeline.h"

int signal_recieved = 0;


void sig_handler(int signo)
{
	if( signo == SIGINT )
	{
		printf("received SIGINT\n");
		signal_recieved = 1;
	}
}

#endif

// ---------------------------------------------------------------------------
// FrameData — payload passed from Transform stage to Display stage.
// Allocated on the heap via unique_ptr to avoid ~15 MB NslPCD stack copies.
// ---------------------------------------------------------------------------
struct FrameData {
	cv::Mat frameMat;  // amplitude/grayscale image (640x480)
	cv::Mat distMat;   // distance image (640x480)
	NslPCD  pcd;       // needed so drawCaption() can read pcdData.distance2D
};

int main(int argc, char** argv)
{
	CaptureOptions camOpt;

#ifdef _WINDOWS
	SetConsoleCtrlHandler(ctrl_handler, TRUE);
#else
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("can't catch SIGINT\n");
#endif

	videoSource* lidarSrc = createApp(argc, argv, &camOpt);

#ifdef SUPPORT_DEEPLEARNING
	if (!camOpt.headless)
		lidarSrc->initDeepLearning(&camOpt);
#endif

	lidarSrc->setLidarOption(&camOpt);

	// -----------------------------------------------------------------------
	// Pipeline slots
	// -----------------------------------------------------------------------
	LatestWinsSlot<NslPCD>    captureSlot;   // Capture  → Transform
	LatestWinsSlot<FrameData> frameSlot;     // Transform → DL (or Display)
	LatestWinsSlot<FrameData> displaySlot;   // DL → Display  (only if DL active)

	// Atomic frame counter for headless logging
	std::atomic<int> captureFrames{0};

	// Decide which pipeline stages are active
	const bool headless = (camOpt.headless != 0);
#ifdef SUPPORT_DEEPLEARNING
	const bool dlActive = !headless && lidarSrc->isDnnReady();
#else
	const bool dlActive = false;
#endif

	// -----------------------------------------------------------------------
	// Stage lambdas
	// -----------------------------------------------------------------------

	// --- Capture thread (always runs) ---
	auto captureFn = [&]() {
		while (!captureSlot.isStopped()) {
			if (lidarSrc->capturePcd(3000)) {
				auto ptr = std::make_unique<NslPCD>(lidarSrc->getPcdData());
				captureFrames.fetch_add(1, std::memory_order_relaxed);
				captureSlot.put(std::move(ptr));
			} else {
				static int failCnt = 0;
				if (++failCnt % 30 == 0) printf("capture timeout...\n");
			}
		}
	};

	// --- Transform thread (skipped in headless mode) ---
	auto transformFn = [&]() {
		while (true) {
			auto pcd = captureSlot.take();
			if (!pcd) break;  // slot was stopped with no pending data

			auto frame = std::make_unique<FrameData>();
			frame->pcd = std::move(*pcd);
			lidarSrc->transformFromPcd(frame->pcd,
			                           frame->frameMat, frame->distMat);
			frameSlot.put(std::move(frame));
		}
	};

	// --- DL thread (only when DL model is loaded) ---
	// When SUPPORT_DEEPLEARNING is not defined, dlFn is an empty immediately-returning
	// lambda, but dlActive is always false in that case so this lambda is never
	// passed to pipeline.start() as a non-null argument.
	auto dlFn = [&]() {
#ifdef SUPPORT_DEEPLEARNING
		while (true) {
			auto frame = frameSlot.take();
			if (!frame) break;  // slot was stopped

			lidarSrc->deepLearning(frame->frameMat);
			displaySlot.put(std::move(frame));
		}
#endif
	};

	// -----------------------------------------------------------------------
	// Start worker threads via PipelineRunner
	// -----------------------------------------------------------------------
	PipelineRunner pipeline;

	if (headless) {
		// Headless: only the capture thread runs; transform/DL/display skipped.
		// captureSlot is consumed by no downstream thread — put() just overwrites
		// the slot; we only need the atomic counter and the isStopped() sentinel.
		pipeline.start(captureFn, nullptr, nullptr);
	} else if (dlActive) {
		pipeline.start(captureFn, transformFn, dlFn);
	} else {
		pipeline.start(captureFn, transformFn, nullptr);
	}

	// -----------------------------------------------------------------------
	// Display loop — runs on the main thread
	// -----------------------------------------------------------------------
	if (headless) {
		// Headless: just keep alive and print a heartbeat every 5 seconds.
		std::chrono::steady_clock::time_point headlessTime =
		    std::chrono::steady_clock::now();

		while (!signal_recieved) {
			std::this_thread::sleep_for(std::chrono::seconds(1));

			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(
			        now - headlessTime).count() >= 5) {
				int frames = captureFrames.load(std::memory_order_relaxed);
				printf("[headless] alive, frames=%d\n", frames);
				headlessTime = now;
			}
		}
	} else {
		// GUI: pull from displaySlot (DL path) or frameSlot (no-DL path).
		LatestWinsSlot<FrameData>& readSlot = dlActive ? displaySlot : frameSlot;

		while (!signal_recieved) {
			auto frame = readSlot.take();
			if (!frame) break;  // slot stopped → shutdown in progress

			// Restore pcdData so drawCaption() can read distance2D for mouse overlay.
			lidarSrc->setPcdData(std::move(frame->pcd));

			lidarSrc->drawCaption(frame->frameMat, frame->distMat, &camOpt);

			if (lidarSrc->prockey(&camOpt) == 27)  // ESC
				break;
		}
	}

	// -----------------------------------------------------------------------
	// Shutdown sequence: stop all slots to wake blocked take() calls.
	// NOTE: The capture thread may take up to 3000ms (one nsl_getPointCloudData timeout)
	// to notice the stop signal, since capturePcd() is not interruptible.
	// -----------------------------------------------------------------------
	captureSlot.stop();
	frameSlot.stop();
	displaySlot.stop();

	pipeline.stopAndJoin();

	lidarSrc->stopLidar();
	delete lidarSrc;
	lidarSrc = NULL;

	printf("end sample_detector\n");

	return 0;
}


