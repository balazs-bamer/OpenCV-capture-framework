#include<iostream>
#include<curses.h>
#include<cctype>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/highgui/highgui_c.h>
#include"util.h"

#if USE_NVWA == 1
#include"debug_new.h"
#endif


using namespace projector;


#ifdef DEBUGOUTPUT
std::stringstream Debug::dbuf;
std::mutex Debug::bufMutex;
std::list<Debug*> Debug::instances;
Showcase *Debug::showcase;

Debug::Debug() {
	std::lock_guard<std::mutex> lock(bufMutex);
    //      DBG.str("");
    DBG.precision(6);
    DBG.setf(std::ios::fixed, std::ios::floatfield);
	instances.push_back(this);
}

Debug::~Debug() {
	std::lock_guard<std::mutex> lock(bufMutex);
	instances.remove(this);
}

void Debug::_log() {
	double diff = stopper.elapsedDblAct();
	DBG << diff << ' ' << prefix;
}

void Debug::log() {
    std::lock_guard<std::mutex> lock(bufMutex);
    _log();
    DBG << '\n';
}

void Debug::log(const char *str) {
	std::lock_guard<std::mutex> lock(bufMutex);
	_log();
	DBG << ": " << str << '\n';
}

void Debug::fps(bool useCursesHere) {
	std::lock_guard<std::mutex> lock(bufMutex);
	std::chrono::high_resolution_clock::time_point now = stopper.getValue();
	if(nSampl > 0) {
		double period = (long)(std::chrono::duration_cast<std::chrono::microseconds>(now - lastFps).count()) / 1000000.0;
		double timeSofar = (long)(std::chrono::duration_cast<std::chrono::microseconds>(now - first).count()) / 1000000.0;
		double avg = timeSofar / nSampl;
		double fps = 1.0 / avg;
		double momFps = 1.0 / period;
		DBG << "         " << prefix << " n: " << nSampl << " last: " << period << " fps! " << momFps << " avg: " << avg << " fps: " << fps << '\n';
		if(useCursesHere) {
			cPrintw(prefix, nSampl);
			cPrintw("last", period);
			cPrintw("=fps", momFps);
			cPrintw(" avg", avg);
			cPrintw(" fps", fps);
		}
	}
	else {
		first = now;
	}
	nSampl++;
	lastFps = now;
}

void Debug::fromUser(int now, int prev) {
	std::lock_guard<std::mutex> lock(bufMutex);
	for(std::list<Debug*>::const_iterator it = instances.cbegin(); it != instances.cend(); ++it) {
		(**it).fromDisp(now, prev);
	}
}

void Debug::fromDisp(int now, int prev) {
    keyNow = now; keyPrev = prev;
}


void Debug::image(int i, const cv::Mat img) {
	{
		std::lock_guard<std::mutex> lock(bufMutex);
		char str[6];
		str[0] = 'n';
		str[1] = keyNow == -1 ? '-' : keyNow ;
		str[2] = ' ';
		str[3] = 'i';
		str[4] = '0' + i;
		str[5] = 0;
		DBG << "         " << prefix << ": image summary and display req: " << str << '\n';
	}
	if(i >= 0 && i <= 9 && i + '0' == keyNow) {
		if(std::isalpha(keyPrev)) {
			// save series of images, not implemented yet
		}
		else {
			showcase->update(img);
		}
		keyNow = keyPrev = -1; // reset to prevent subsequent updates
	}
}

void Debug::dump() {
#if DEBUG_STDOUT == 0
	std::lock_guard<std::mutex> lock(bufMutex);
	std::ofstream f;
	f.open(DEBUG_LOC);
	if(f.good()) {
		f << dbuf.rdbuf();
		f.close();
	}
#endif
}

#endif

Showcase::Showcase(const char *wn) : showing(cv::Mat::eye(100, 100, CV_8U) * 255), prevKey(-1), windowName(cv::String(wn)) {
    DEBPREF("showcase");
	if(Arguments::optShowWindow) {
		cv::namedWindow(windowName, CV_WINDOW_AUTOSIZE); //resizable window;
	}
}

bool Showcase::check() {
	int key = '!';
	if(Arguments::optShowWindow) { // use this to read keyboard
		std::lock_guard<std::mutex> lock(updateMutex);
		cv::imshow(windowName, showing);
		key = cv::waitKey(Arguments::optGetchDelay * 5); //delay N millis, usually long enough to display and capture input
	}
	else {
		if(Arguments::optUseCurses) {
			std::chrono::milliseconds dura(Arguments::optGetchDelay);
			std::this_thread::sleep_for(dura);
			key = getch();
		}
	}
	if(std::isalnum(key)) {
		DEBKEY(key, prevKey);
		prevKey = key;
	}
	return key == 'q';
}

#ifdef DEBUGOUTPUT
const char* Showcase::decodeDepth(int d) {
	switch(d) {
	case CV_8U:
		return "CV_8U";
	case CV_8S:
		return "CV_8S";
	case CV_16U:
		return "CV_16U";
	case CV_16S:
		return "CV_16S";
	case CV_32S:
		return "CV_32S";
	case CV_32F:
		return "CV_32F";
	case CV_64F:
		return "CV_64F";
	default:
		return "unknown";
	}
}

void Showcase::update(const cv::Mat &m) {
	if(Arguments::optShowWindow) {
		std::lock_guard<std::mutex> lock(updateMutex);
		m.copyTo(showing);
	}
}
#endif

int projector::cClear() {
	if(Arguments::optUseCurses) {
        return clear();
    }
	return 0;
}

template<>
int projector::cPrintw<unsigned int>(const char *s, unsigned int i) {
	if(Arguments::optUseCurses) {
		return printw("%s: %u\n", s, i);
	}
	return 0;
}

template<>
int projector::cPrintw<unsigned long>(const char *s, unsigned long l) {
	if(Arguments::optUseCurses) {
		return printw("%s: %lu\n", s, l);
	}
	return 0;
}

template<>
int projector::cPrintw<int>(const char *s, int i) {
	if(Arguments::optUseCurses) {
		return printw("%s: %d\n", s, i);
	}
	return 0;
}

template<>
int projector::cPrintw<long>(const char *s, long l) {
	if(Arguments::optUseCurses) {
		return printw("%s: %ld\n", s, l);
	}
	return 0;
}

template<>
int projector::cPrintw<double>(const char *s, double d) {
	if(Arguments::optUseCurses) {
		return printw("%s: %07.4f\n", s, d);
	}
	return 0;
}

void StartStop::start() {
    std::lock_guard<std::mutex> lock(startStopMutex);
	DEB1("starting...");
    started = true;
    theThread = new std::thread([this] {run();});
    sched_param sch;
    int policy;
    pthread_t nativeHandle;
    pthread_getschedparam(nativeHandle = theThread->native_handle(), &policy, &sch);
    sch.sched_priority = priority == -1 ? sched_get_priority_max(SCHED_FIFO) : priority;
    if(pthread_setschedparam(nativeHandle, SCHED_FIFO, &sch)) {
        std::cerr << "Cannot set filter thread priority." << std::endl;
    }
	DEB1("started.");
}

void StartStop::stop() {
    std::lock_guard<std::mutex> lock(startStopMutex);
	if(started && theThread != NULL) {
		DEB1("stopping...");
		//filter thread should terminate
		started = false;
		DEB1("cleanup...");
		cleanup();
		DEB1("cleanup ready.");
		// wait for termination to let it cleanup its stuff
		DEB1("join...");
		theThread->join();
		DEB1("join ready.");
		delete theThread;
		theThread = NULL;
		DEB1("stopped.");
	}
}

