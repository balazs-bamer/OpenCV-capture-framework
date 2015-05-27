/*
* starter_video.cpp
*
*  Created on: Nov 23, 2010
*      Author: Ethan Rublee
*
*  Modified on: April 17, 2013
*      Author: Kevin Hughes
*
* A starter sample for using OpenCV VideoCapture with capture devices, video files or image sequences
* easy as CV_PI right?
*/

#include "opencv2/videoio_mod.hpp"
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/videoio/videoio_c.h>
#include <opencv2/highgui/highgui_c.h>

#include <curses.h>
#include <iostream>
#include <time.h>
#include <getopt.h>
#include <cctype>
#include <csignal>
#include <chrono>
#include "still_config.h"
#include "still.h"

using namespace projector;

cv::VideoCapture_mod capture;
volatile bool started = false;
char *incaseofBadalloc = NULL;

void stop() {
	started = false;
}

void sigintHandler(int signal) {
	stop();
}

void done();

void sigabrtHandler(int signal) {
	done();
}

int init() {
	incaseofBadalloc = new char[65536];
	capture.open(Arguments::optVideoNum); //try to open as a video camera, through the use of an integer param
    if (!capture.isOpened()) {
	    std::cerr << "Failed to open the video device!\n" << std::endl;
		return 1;
    }
	capture.set(CV_CAP_PROP_FRAME_WIDTH, 640);
    capture.set(CV_CAP_PROP_FRAME_HEIGHT, 480);
	if(Arguments::optUseCurses) {
		initscr();			/* Start curses mode		*/
		cbreak();				/* Line buffering disabled	*/
		keypad(stdscr, true);		/* We get F1, F2 etc..		*/
		noecho();			/* Don't echo() while we do getch */
		nodelay(stdscr, true);
		scrollok(stdscr, false);
	}
	// ctrl-c to exit program
	std::signal(SIGINT, sigintHandler);
	std::signal(SIGTERM, sigintHandler);
	std::signal(SIGABRT, sigabrtHandler);
	return 0;
}

int process() {
	DEBDECP("main");
	DEB1("starting...");
	FrameProcessor frameProcessor;
	StillFilter filter(capture, frameProcessor);	// use default handler
	Showcase showcase("Image");
	DEBSHOW(showcase);
	filter.start();
	started = true;
	DEB1("started.");
	while(started) {
		if(showcase.check()) {
			stop();
		}
	}
	DEB1("stopping...");
	filter.stop();
	DEB1("stopped.");
}

void done() {
	if(incaseofBadalloc != NULL) {
		delete [] incaseofBadalloc;
	}
	if(Arguments::optUseCurses) {
		endwin();			/* End curses mode		  */
	}
#ifdef DEBUGOUTPUT
	Debug::dump();
#endif
}

int main(int argc, char** argv) {
#if USE_NVWA == 1
	nvwa::new_progname = argv[0];
#endif
//	nvwa::new_verbose_flag = true;
	Arguments::parse(argc, argv);
	if(Arguments::optShowOpts || Arguments::optHelp) {
		Arguments::showOpts();
	}
	if(Arguments::optHelp) {
		return 0;
	}
	int ret;
	try {
		ret = init();
		if(!ret) {
			ret = process();
		}
	}
	catch(std::bad_alloc) {
		delete [] incaseofBadalloc;
		incaseofBadalloc = NULL;
		std::cerr << "Out of memory error!" << std::endl;
	}
	done();
	return ret;
}
