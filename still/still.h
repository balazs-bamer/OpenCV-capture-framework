/** @file
Classes providing frame capturing, management, optional filtering for unmoved and/or sharp frames. There is also a default implementation FrameProcessor.

Copyleft Balázs Bámer, 2015.
*/

#ifndef PROJECTOR_STILL_H
#define PROJECTOR_STILL_H

#include<thread>
#include<set>
#include<opencv2/core.hpp>

#include"opencv2/videoio_mod.hpp"
#include"still_config.h"
#include"util.h"
#include"measure.h"

#if USE_NVWA == 1
#include"debug_new.h"
#endif


namespace projector {
	/**
	Describes the current frame processor status.
	*/
	enum FrameProcStatus {
		/** No current image processing, no thread object */
		RESULT_NOIMAGE = -1,
		
		/** Image being processed, it has an active thread. */
		RESULT_PROCESSING,
		
		/** The calculation could not be performed, because the image was
		not good enough. Inactive thread object. */
		RESULT_FAIL,

		/** The calculation was interrupted (return from finish). 
		Inactive thread object.*/
		RESULT_INCOMPLETE,
		
		/** Any-time algorithm was interrupted, or the image did not allow
		exact result. Inactive thread object. */
		RESULT_APPROXIMATE,
		
		/** The result is exact. Inactive thread object. */
		RESULT_EXACT
	};

	/**
	Describes a sharp tile of the image.
	*/
	class SharpTile {
	public:
		/** Percentage of the count of adjacent pixel differences in a tile exceeding a higher threshold to the lower. See README.md for more details.*/
		int highPercent;
		/** Width of the tile. */
		int width;
		/** Height of the tile. */
		int height;
		/** Upper-left corner X coordinates of the tile. */
		int startX;
		/** Upper-left corner Y coordinates of the tile. */
		int startY;

		/**
		This constructor sets everything.
		*/
		SharpTile(int p, int w, int h, int x, int y) : highPercent(p), width(w), height(h), startX(x), startY(y) {};

		/**
		Provides ordering using the diff field.
		*/
		bool operator<(const SharpTile &other) const {
			return highPercent < other.highPercent;
		};
	};

	class StillFilter;

	/**
	Contains all the parameters a FrameProcessor::process method call needs.
	*/
	class ProcessArgs {
		friend class FrameProcessor;
		friend class StillFilter;
	protected:
		/** Timestamp of frame grabbing. Instantiation of this class should be close in time to the effective grab call.
		*/
		Stopper timestamp; 

		/**
		The frame itself. It is assumed to be full-size and have YCrCb color format in unsigned 8 bit depth. */
		cv::Mat *frame;

		/**
		Contains the sharp tiles if sharpness has been checked in StillFilter, otherwise NULL.
		*/
		std::set<SharpTile> *tiles;
		
		/**
		Initializes the instance with an empty frames and NULL tiles.
		*/
		ProcessArgs() {
			// we use the frame definitely
			frame = new cv::Mat();
			// but get the tiles from outside
			tiles = NULL;
		};

		/**
		Deletes everything not NULL.
		*/
		~ProcessArgs() {
			if(frame != NULL) {
				delete frame;
			}
			if(tiles != NULL) {
				delete tiles;
			}
		};
	};

	/**
	Class to process the (still) images identified during capture. The images are considered to be sharp. The default implementation saves the images.
	*/
	class FrameProcessor {
	protected:
		/**
		Current processing status.
		*/
		FrameProcStatus current;
		
		/**
		Indication for doProcess to finish processing.
		*/
		volatile bool finish = false; 

		/**
		The thread running the method doProcess.
		*/
		std::thread *theThread = NULL;

		/**
		Class instance providing measurements.
		*/
		PollSensors measure;

		DEBDEC;
	public:
		/**
		Sets the debug prefix if needed and the current status to no image.
		*/
		FrameProcessor();

		/**
		Joins the processing thread if any.
		*/
		virtual ~FrameProcessor();

		/**
		Starts measurements.
		*/
		void startMeasure() { measure.start(); };

		/**
		Stops measurements.
		*/
		void stopMeasure() { measure.stop(); };
	
		/**
		Starts processing the frame and its properties held by arg in a separate thread.
		If Arguments::optHandlerTimeout > 0, a timeout lambda is started, which sets finish to true on exit, thus signing that doProcess should exit.
		This method deletes the arg, doProcess must not do it.
		ONLY StillFilter may call this method.
		*/
		void process(const ProcessArgs *arg);

		/**
		Checks current processing status. If it is one of the terminated results, the call
		cleans up the thread and resets the status to RESULT_NOIMAGE.
		ONLY StillFilter may call this method.
		*/
		FrameProcStatus status();

		/**
		The handler is asked to terminate processing the current image. 
		ONLY StillFilter may call this method.
		*/
		void die();
	protected:
		/**
		Does the actual processing, parameter is the same as by process.
		If finish becomes true, the implementation must take Arguments.optForceHandlerExit into account
		 whether it should return without a result or with one.
		The processing may use any-time algorithm or not. The value for field current should be returned. 
		Subclasses should have a mechanism for forwarding the results to other class. It should happen here before exit.

		This implementation saves the frames in JPEG format with the
		rectangles considered sharp highlighted.
		*/
		virtual FrameProcStatus doProcess(const ProcessArgs *arg);
	};

	/**
	Class implementing a framework for managing a modified VideoCapture
	stream and filtering out sharp images, which are fed into the handler
	for processing.
	*/
	class StillFilter : public StartStop {
	protected:
		/**
		The initialized capture providing the video stream.
		*/
		cv::VideoCapture_mod& capture;

		/**
		The initialized frame processor instance to use.
		*/
		FrameProcessor& processor;
	public:
		/**
		Constructs a new filter without starting it. Just sets the two arguments.
		*/
		StillFilter(cv::VideoCapture_mod& capture, FrameProcessor& handler);
	
	protected:
		/**
		Updates capture settings according to the passed still sampling percent value. Needs to get saved values instead of accessing Arguments::opt... because these may change runtime and this method is used more times.
		*/
		void updateCaptureProps(int optStillSamplingPercent, int optStillDownsampleExponent);

		/**
		Checks if the two images are different enough. See README.md for more details.
		*/
		virtual bool hasChanged(const cv::Mat *current, const cv::Mat *last, int optStillSamplingPercent);

		/**
		Calculates a dividor from div such that dividing len with it yields
		at least 16.
		*/
		int dividor(int len, int div);

		/**
		Checks if this image is sharp enough. This implementation considers
		only YCrCb Images and their Y channel. See README.md for more details.
		*/
		virtual std::set<SharpTile>* checkSharpness(const cv::Mat& frame);
		
		/**
		Does the actual filtering in separate thread. See README.md for more details.
		*/
		virtual void run();
		
		/**
		Instructs the frame processor to stop if it is processing.
		*/
		virtual void cleanup();
	};
}


#endif
