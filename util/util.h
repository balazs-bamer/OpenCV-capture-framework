/** @file
Auxiliary classes and functions for FEOF.

Copyleft Balázs Bámer, 2015.
*/

#ifndef PROJECTOR_UTIL_H
#define PROJECTOR_UTIL_H

#include"still_config.h"
#include<chrono>
#include<stdexcept>
#include<thread>
#include<mutex>
#include<list>
#include<opencv2/core.hpp>

#if DEBUG_OUTPUT == 1 

/**
This one should be used to test if debug output is on.
*/
#define DEBUGOUTPUT 1

#if DEBUG_STDOUT == 0
#define DBG dbuf
#else
#define DBG std::cout
#include<iostream>
#endif

#include <sstream>
#include <fstream>

#else

#undef DEBUGOUTPUT

#endif

#if USE_NVWA == 1
#include"debug_new.h"
#endif


namespace projector {

	/**
	Parses decimal numbers in a character string to integers. If next is not null, it will point to the first not-yet-parsed character.
	*/
	template<typename T>
    T parseExc(const char* str, const char **next = NULL) {
	    if(str == NULL) {
		    throw std::invalid_argument("NULL");
	    }
		const char *ptr = str;
	    while(std::isspace(*ptr)) {
		    str++;
	    }
		T result = 0;
	    int sign = 1, digits = 0;
		switch(*ptr) {
	    case '-':
		    sign = -1;
			ptr++;
	        break;
		case '+':
			sign = 1;
	        ptr++;
		    break;
	    }
		while(*ptr >= '0' && *ptr <= '9') {
			T prev = result;
	        result = result * 10 + sign * (*ptr++ - '0');
		    digits++;
			if(result / 10 != prev) { // overflow
				throw std::out_of_range(str);
	        }
		}
	    if(digits == 0) { // no digit converted
		    throw std::invalid_argument(str);
	    }
		if(next != NULL) {
			*next = ptr;
	    }
		return result;
	}

	/**
	Class providing stopper-like time measurements, ordering and similar tasks.
	*/
	class Stopper {
	protected:
		/**
		Start time point of this instance.
		*/
		std::chrono::high_resolution_clock::time_point startTp;
	public:
		/**
		Initializes this to current time.
		*/
		Stopper() : startTp(std::chrono::high_resolution_clock::now()) {
		};

		/**
		Initializes this to current time + diff (diff in us).
		*/
		Stopper(long diff) {
			std::chrono::microseconds dtn(diff);
			startTp = std::chrono::high_resolution_clock::now() + dtn;
		}

		/**
		Initializes this to specified time point.
		*/
		Stopper(std::chrono::high_resolution_clock::time_point tp) : startTp(tp) {
		};

		/**
		Returns stored time point.
		*/
		std::chrono::high_resolution_clock::time_point getValue() const {
			return startTp;
		};

		/**
		Compares the two time points.
		*/
		bool operator< (const Stopper &other) const {
			return this->startTp < other.startTp;
		};

		/**
		Returns the elapsed time till now in ms.
		*/
		int elapsedMs() {
			return (int)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTp).count());
		};
	
		/**
		Returns the elapsed time till now in us.
		*/
		long elapsedUs() {
			return (long)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTp).count());
		};

		/**
		Returns the elapsed time till now in s.
		*/
		double elapsedDbl() {
			long l = (long)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTp).count());
			return l / 1000000.0;
		};

		/**
		Returns the elapsed time till now in ms, and actualizes it to current time.
		*/
		int elapsedMsAct() {
			int result = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTp).count());
			startTp = std::chrono::high_resolution_clock::now();
			return result;
		};
	
		/**
		Returns the elapsed time till now in us, and actualizes it to current time.
		*/
		long elapsedUsAct() {
			long result = (long)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTp).count());
			startTp = std::chrono::high_resolution_clock::now();
			return result;
		};
		
		/**
		Returns the elapsed time till now in s, and actualizes it to current time.
		*/
		double elapsedDblAct() {
			long l = (long)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTp).count());
			startTp = std::chrono::high_resolution_clock::now();
			return l / 1000000.0;
		};

		/**
		Actualizes the time point to current time.
		*/
		void actualize() {
			startTp = std::chrono::high_resolution_clock::now();
		};

		/**
		Returns startTp difference in microseconds.
		*/
		long operator-(Stopper& other) {
			return (long)(std::chrono::duration_cast<std::chrono::microseconds>(startTp - other.startTp).count());
		};
	};

#ifdef DEBUGOUTPUT

/**
Declares a Debug instance without a prefix.
*/
#define DEBDEC Debug _debug

/**
Declares a Debug instance with a prefix.
*/
#define DEBDECP(prefix) Debug _debug(prefix)

/**
Sets the prefix for this debug instance.
*/
#define DEBPREF(prefix) _debug.setPrefix(prefix)

/**
Emits a frame per seconds info, optionally also on the screen if curses is anebled and the argument is true.
*/
#define DEBFPS(useCurses) _debug.fps(useCurses)

/**
Emits the str following the elapsed time since the last method call of the _debug instance in scope, and its prefix.
*/
#define DEB1(str) _debug.log(str)

/**
Same as DEB1 but prints arg, too.
*/
#define DEB2(str, arg) _debug.log(str, arg)

/**
Same as DEB2 but prints arg in hexadecimal.
*/
#define DEB2X(str, arg) _debug.logx(str, arg)

/**
Attaches the Showcase instance to the Debug class.
*/
#define DEBSHOW(showcase) Debug::setShowcase(showcase)

/**
Dispatches this and the previous user keystrokes to all the instances.
This is currently used to display OpenCV Mat frames received by the next
DEBIMG call with the same number as the current keystroke.
*/
#define DEBKEY(now, prev) Debug::fromUser(now, prev)

/**
Show the OpenCV Mat img using OpenCV imshow function if windowing is enabled and the user has pressed the key identified by id.
*/
#define DEBIMG(id, img) _debug.image(id, img)

	class Showcase;

	/**
	Class used to print execution steps with timing. The instance
	can be per-class basis or in a local scope and have an unique prefix.
	All operations, including declarations should be done by macros, so
	the conditional compiling can handle them.
	*/
	class Debug {
	private:
		Stopper stopper;
		// last time we called fps
		std::chrono::high_resolution_clock::time_point lastFps, first;
		// samples so far
		int nSampl = 0;
		// Prefix for debug output
		const char *prefix = NULL;
		// Current and previous keystrokes.
		int keyNow = -1, keyPrev = -1;
		// gathers log
		static std::stringstream dbuf;
		// used to keep log message parts together
		static std::mutex bufMutex;
		// keeps track of existing instances. This allows the static fromUser
		// method to dispatch requests to all instances
		static std::list<Debug*> instances;
		// Showcase to be used to show images
		static Showcase *showcase;

		void _log();
	public:
		/**
		Sets an invalid prefix, sets up stringstream, and registers this
		instance for dispatching.
		*/
		Debug();

		/**
		Sets the prefix and as above
		*/
		Debug(const char *str) : Debug() {
			prefix = str;
		}

		/**
		Removes this instance from dipatch list.
		*/
		~Debug();

		/**
		Sets the prefix for debug output.
		*/
		void setPrefix(const char *str) {
			prefix = str;
		}

		/**
		Prints only prefix and time.
		*/
		void log();

		/**
		As above and prints str, too.
		*/
		void log(const char *str);

		/**
		As above and prints t, too.
		*/
		template<typename T>
		void log(const char *str, const T t) {
			std::lock_guard<std::mutex> lock(bufMutex);
			_log();
			DBG << ": " << str << ' ' << t << '\n';
		}

		/**
		As above and prints t in hex.
		*/
		template<typename T>
		void logx(const char *str, const T t) {
			std::lock_guard<std::mutex> lock(bufMutex);
			_log();
			DBG.setf(std::ios::hex , std::ios::basefield);
			DBG << ": " << str << ' ' << t << '\n';
			DBG.setf(std::ios::dec , std::ios::basefield);
		}

		/**
		Prints current frames per secs and displays it on curses if on.
		*/
		void fps(bool useCurses);

		static void setShowcase(Showcase &sc) {
			showcase = &sc;
		};

		/**
		Dispatches the two keystrokes to instances.
		*/
		static void fromUser(int now, int prev);

		/**
		Sets the two keystrokes for later processing.
		*/
		void fromDisp(int now, int prev);

		void image(int i, const cv::Mat img);
		
		/**
		Dumps the buffer to disc.
		*/ 
		static void dump();
	};
#else
#define DEBDEC
#define DEBDECP
#define DEBPREF
#define DEBFPS(useCurses)
#define DEB1(str)
#define DEB2(str, arg)
#define DEB2X(str, arg)
#define DEBSHOW(showcase)
#define DEBKEY(now, prev)
#define DEBIMG(id, img)
#endif

	/**
	This class provides a limited user interface. It provides an optionally OpenCV window, listens OpenCV or Curses keystrokes. If debugging is enabled, it receiving image display requests from Debug (see later).
	*/
	class Showcase {
	private:
		// window name
		cv::String windowName;
		// image currently showing
		cv::Mat showing;
		// used to keep image update clean
        std::mutex updateMutex;
		// previous keystroke
		int prevKey;
		DEBDEC;
	public:
		/**
		Constructs an instance using the specified window name.
		*/
		Showcase(const char *wn);

		/**
		Does nothing.
		*/
		~Showcase() {};

		/**
		Shows the current image - if any for Arguments::optGetchDelay * 5 ms
		and processes the keycode. If it was 'q', returns true.
		*/
		bool check();

#ifdef DEBUGOUTPUT
		/**
		Turns the color depth code into displayable string.
		*/
		static const char* decodeDepth(int d);

		/**
		Updates the image to be shown. We copy the contents to avoid
		side effects. Only effective if DEBUGOUTPUT is defined and Arguments::optShowWindow is 1. OpenCV can display grayscale or BGR images. If the provided
		image is has YCrCb color format, it will be displayed as BGR, so in false colors.
		*/
		void update(const cv::Mat &m);
#endif
	};

	/**
	Calls curses clear if enabled.
	*/
	int cClear();

	/** Calls curses printw if enabled. */
	template<typename T>
	int cPrintw(const char *s, T t) {
		throw std::invalid_argument("FALLBACK: missing template specialisation for cPrintw\n");
	}

	/** Calls curses printw if enabled. */
	template<>
	int cPrintw<unsigned int>(const char *s, unsigned int l);

	/** Calls curses printw if enabled. */
	template<>
	int cPrintw<unsigned long>(const char *s, unsigned long l);

	/** Calls curses printw if enabled. */
	template<>
	int cPrintw<int>(const char *s, int l);

	/** Calls curses printw if enabled. */
	template<>
	int cPrintw<long>(const char *s, long l);

	/** Calls curses printw if enabled. */
	template<>
	int cPrintw<double>(const char *s, double d);

	/**
    Base class implementing a startable and stoppable function.
    Cannot be instantiated. 
	Subclasses must set a valid prefix in _debug.
    */
    class StartStop {
    protected:
		/**
		Priority of theThread if allowed to be set.
		*/
        int priority = -1;
	
		/**
		The thread performing the tasks defined in subclasses.
		*/
        std::thread *theThread;
        
		/**
		Used to mutually exclude start() and stop().
		*/
        std::mutex startStopMutex;
        
		/**
		True if processing is started, false if not or it should stop.
		*/
        volatile bool started = false;
		DEBDEC;
    public:
		/**
		Does nothing.
		*/
        StartStop() {};
        
		/**
		Calls stop().
		*/
		virtual ~StartStop() { stop(); }

        /**
        Starts the run method with specified priority, or top priority if none given, or insufficient privileges to set it.
        */
        void start();

        /**
        Stops the run method, finishes the current processing if any. This is done by first setting started to false, which should cause the thread to exit properly, then calls cleanup(), then waits for the thread to join.
        */
        void stop();
    protected:
        /**
        Effectively runs in a separate thread pointed to by theThread.
        */
        virtual void run() {};

        /**
        Called by stop method to do class-specific cleanup. IMPORTANT! This function is called by stop() before joining the thread.
        */
        virtual void cleanup() {};

    private:
        StartStop(const StartStop&);
        StartStop& operator=(const StartStop&);
    };
}


#endif
