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

	int headlessFrames = 0;
	std::chrono::steady_clock::time_point headlessTime = std::chrono::steady_clock::now();

	while ( !signal_recieved )
	{
		if (!lidarSrc->captureLidar(3000, &camOpt)) {
			printf("capture : failed...\n");
			break;
		}

		if (camOpt.headless) {
			headlessFrames++;
			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::seconds>(now - headlessTime).count() >= 5) {
				printf("[headless] alive, frames=%d\n", headlessFrames);
				headlessTime = now;
			}
			continue;
		}

#ifdef SUPPORT_DEEPLEARNING
		lidarSrc->deepLearning(camOpt.frameMat);
#endif
		lidarSrc->drawCaption(camOpt.frameMat, camOpt.distMat, &camOpt);

		if (lidarSrc->prockey(&camOpt) == 27) // ESC
			break;
	}

	lidarSrc->stopLidar();
	delete lidarSrc;
	lidarSrc = NULL;

	printf("end sample_detector\n");

	return 0;
}



