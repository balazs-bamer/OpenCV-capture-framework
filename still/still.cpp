#include<curses.h>
#include<pthread.h>
#include<future>
#include<sched.h>
#include<iostream>
#include<exception>
#include<math.h>
#include<opencv2/imgcodecs.hpp>

#include"still.h"

#if USE_NVWA == 1
#include"debug_new.h"
#endif


using namespace projector;

FrameProcessor::FrameProcessor() {
	DEBPREF("proc");
	current = RESULT_NOIMAGE;
}

FrameProcessor::~FrameProcessor() {
	if(theThread != NULL) { // for the case when no status query occured after termination
		theThread->join();
		delete theThread;
	}
}

void FrameProcessor::process(const ProcessArgs *arg) {
/*	if(current == RESULT_PROCESSING) {
		throw std::runtime_error("Cannot start new processing over existing one.");
	}*/
	DEBFPS(false);
	current = RESULT_PROCESSING;
	finish = false;
	theThread = new std::thread([this, arg] {
		DEB1("processing...");
#ifdef DEBUGOUTPUT
		std::chrono::high_resolution_clock::time_point startProc = std::chrono::high_resolution_clock::now();
#endif
		current = doProcess(arg);
		delete arg;
#ifdef DEBUGOUTPUT
		long elapsedUs = (long)(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startProc).count());
		DEB2("processing ready, it took ", elapsedUs / 1000000.0);
#endif
	});
	if(Arguments::optHandlerTimeout > 0) {
		std::async(std::launch::async,
			[this]{
				DEB1("timeout...");
				std::chrono::milliseconds dura(Arguments::optHandlerTimeout);
                std::this_thread::sleep_for(dura);
				finish = true;		
				DEB1("timeout over.");
			}
			);
	}
}

FrameProcStatus FrameProcessor::doProcess(const ProcessArgs *arg) {
	// processing comes, may continue while !finish
	// if finish becomes true, the implementation must take Arguments::optForceHandlerExit into account
	// to see if it should return without a result or with one.
	// In the end show or forward result.

	DEBDECP("doProc");
	DEB1("start");
	// Let's use high-level OpenCV functions instead of doing it by hand.
	// Here we have plenty of time here.
	cv::Mat grayFrame(arg->frame->size(), CV_8U);
	int mapping[] = {0, 0};
	// copy only the brightness channel of the YCrCb image
	cv::mixChannels(arg->frame, 1, &grayFrame, 1, mapping, 1);
	// the mask has initially half brightness
	cv::Mat mask(grayFrame.size(), CV_8U);
	mask = cv::Scalar(127);
	if(arg->tiles != NULL) {
		// we set it to full brightness in sharp rectangles
		for(std::set<SharpTile>::const_iterator it = arg->tiles->cbegin(); it != arg->tiles->cend(); ++it) {
			SharpTile tile = *it;
			cv::Mat region(mask, cv::Rect(tile.startX, tile.startY, tile.width, tile.height));
			region = 255;
		}
    }
	grayFrame = grayFrame.mul(mask, 1.0/255.0);
	DEB1("highlight ready.");

	if(finish) {
		DEB1("request to finish, abort processing");
		return RESULT_INCOMPLETE;
	}

	cv::String fileName(OUTPUT_FILE_PREFIX);
	fileName += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()).c_str();
	fileName += ".jpg";

	std::vector<int> compression_params;
	cv::imwrite(fileName, grayFrame, compression_params);
	DEB1("JPEG ready.");
	
	return RESULT_EXACT;
}

FrameProcStatus FrameProcessor::status() {
	FrameProcStatus result = current;
	DEB2("status:", current);
	if(current != RESULT_NOIMAGE && current != RESULT_PROCESSING) {
		// processing is ready, clean up
		current = RESULT_NOIMAGE;
		DEB1("status reset to noimage");
		if(theThread) {
			DEB1("status: joining proc thread...");
			theThread->join();
			delete theThread;
			theThread = NULL;
			DEB1("status: deleted proc thread.");
		}
	}
	return result;
}

void FrameProcessor::die() {
	finish = true;
}

StillFilter::StillFilter(cv::VideoCapture_mod& cap, FrameProcessor& handler) : capture(cap), processor(handler) {
	DEBPREF("filter");
	started = false;
}

void StillFilter::cleanup() {
	//initiate extraordinary handler halt
	if(processor.status() == RESULT_PROCESSING) {
		processor.die();
	}
}

void StillFilter::run() {
	processor.startMeasure();
	// using pointers to avoid copying here
	cv::Mat *smallFrameLast = NULL;
	int lastStillSamplingPercent = -1;	// force update on first run
	int lastStillDownsampleExponent = -1;
	ProcessArgs *staleArg = NULL;	// state is valid across several runs
	bool keepAlive = started;	// this thread must live while there is a processing running
	Stopper timeInChange(-(Arguments::optStillChangeTime + 1) * 1000);		
	// time spent in consecutive image change, initially big enough to accept the first still frame
	while(keepAlive) {
		ProcessArgs *readArg;		// state is valid only in one run of the loop
		cv::Mat *smallFrameCurr = NULL, *framep;
		DEB1("0 loop begin.");
		// we store some option variables because changing their value during the loop would mess it up
		int optUseStaleFrame = Arguments::optUseStaleFrame;
		int optStillSamplingPercent = Arguments::optStillSamplingPercent;
		int optStillDownsampleExponent = Arguments::optStillDownsampleExponent;
		int optSharpTilesRequired = Arguments::optSharpTilesRequired;

		// update capture properties if sampling percent was changed		
		if(lastStillSamplingPercent != optStillSamplingPercent) {
			updateCaptureProps(optStillSamplingPercent, optStillDownsampleExponent);
			lastStillSamplingPercent = optStillSamplingPercent;
		}
		if(lastStillDownsampleExponent != optStillDownsampleExponent) {
            updateCaptureProps(optStillSamplingPercent, optStillDownsampleExponent);
            lastStillDownsampleExponent = optStillDownsampleExponent;
			if(smallFrameLast != NULL) { // invalidate the old one if any
			   delete smallFrameLast;
				smallFrameLast = NULL;
		    }
        }

		readArg = new ProcessArgs();			// it should be invalid here, timestamp is saved
		bool cond = started && // if dying, we only grab
			// we also need if we require fresh ones or have no stale frame
            (!optUseStaleFrame || staleArg == NULL);
		// if we have a staleArg but don't need it, delete
		// this may happen if Arguments::optUseStaleFrame changes runtime
		if(staleArg != NULL && !optUseStaleFrame) {
			delete staleArg;
			staleArg = NULL;
		}
	
		// Grab and retrieve frame if needed
		
		bool goOn = capture.grab() && cond;
		DEB1("1 frame grabbed.");
		if(goOn) {
			if(optStillSamplingPercent == 0) {
				framep = readArg->frame;
			}
			else {
				smallFrameCurr = new cv::Mat();
				framep = smallFrameCurr;
			}
            goOn = capture.retrieve(*framep, 0);
			DEB1("2 frame retrieved.");
		}
		if(goOn) {
            goOn = !(framep->empty());
		}
		if(!goOn) {
			if(smallFrameCurr != NULL) {
				delete smallFrameCurr;
			}
		}
		else {
			DEBIMG(1, *framep);
		}
		cClear();
		DEBFPS(true);

		// check for still images if retrieved and needed
		
		if(goOn && optStillSamplingPercent > 0) { 
			bool changed = hasChanged(smallFrameCurr, smallFrameLast, optStillSamplingPercent);
			int elapsed = timeInChange.elapsedMs();
			if(Arguments::optStillChangeTime > elapsed) {
				goOn = false; // not enough yet
			}
			if(!changed) {
				timeInChange.actualize();
				if(goOn) {
					DEB2("3 frame not changed, enough time spent in change", elapsed);
					// set downsampling
					updateCaptureProps(0, optStillDownsampleExponent);
					goOn = capture.retrieve(*(readArg->frame), 0);
					if(goOn) {
						goOn = !(readArg->frame->empty());
					}
					// reset properties
					updateCaptureProps(optStillSamplingPercent, optStillDownsampleExponent);
					DEB1("4 big frame retrieved.");
				}
				else {
					DEB2("3 frame not changed, more time needed in change", elapsed);
				}
			}
			// save current frame
			if(smallFrameLast != NULL) {
				delete smallFrameLast;
			}
			smallFrameLast = smallFrameCurr;  // current is invalid now
			// if we use stale frames, there may be a long gap in retrieved frames, but we may still use the old one
			if(changed) { // we don't want changes to be processed
				DEB1("3 frame changed.");
				goOn = false;
			}
		}
		
		// check sharpness if retrieved and needed
		
		if(goOn && optSharpTilesRequired > 0) {
			readArg->tiles = checkSharpness(*(readArg->frame));
			DEB2("5 sharpness ready, tiles:", readArg->tiles->size());
			// are there enough sharp regions?
			// started may have changed
			if(readArg->tiles->size() < optSharpTilesRequired) {
				goOn = false;
			}
		}

		// see what we have

		if(!goOn) {
			delete readArg;	// one check failed, readArg won't be used
			readArg = NULL;
		}
		else {
			if(started && optUseStaleFrame && staleArg == NULL) { // set it if needed and empty
				staleArg = readArg;
				readArg = NULL;
				DEB1("6 updated stale.");
			}
		}

		// see if we need new processing

		FrameProcStatus processingResult = processor.status();
		// we must remain in the loop while processing
		keepAlive = started || processingResult == RESULT_PROCESSING;
		if(started && processingResult != RESULT_PROCESSING) {
			if(optUseStaleFrame) {
				// no new one, use the stale frame if any
				if(staleArg != NULL) {
					// we will start over obtaining a stale frame, so reset the time spent in change
					// because we don't have any info for the skipped period
					timeInChange.actualize();
					DEB1("7 stale frame will be processed.");
					processor.process(staleArg);
					staleArg = NULL;
				}
			}
			else {
				// we don't use stale frames, take the new one if ready
				if(readArg != NULL) {
					processor.process(readArg);
					DEB1("7 read frame will be processed.");
				}
			}
		}
		else {
			// we don't use readArg, it must be deleted
			if(readArg != NULL) {
				delete readArg;
			}
		}
	}
	if(staleArg != NULL) {
		delete staleArg;
	}
	if(smallFrameLast != NULL) {
		delete smallFrameLast;
	}
	// end of loop, stop measurements
	DEB1("loop is over.");
	processor.stopMeasure();
}

void StillFilter::updateCaptureProps(int optStillSamplingPercent, int optStillDownsampleExponent) {
	RetrieveProps props;
    props.region.x = -1;    // use whole image
    if(optStillSamplingPercent == 0) { // no check for still images
    // we go direct for sharpness using YCrCb
        props.sampling = DS_ORIGINAL;
        props.colorspace = CS_YCRCB;
    }
    else {  // we check still images on a tiny resized image to eliminate
        // camera shake
        props.sampling = (RetrDownsample)optStillDownsampleExponent;
        props.colorspace = CS_GRAY;
    }
    capture.set(props);
}

bool StillFilter::hasChanged(const cv::Mat *current, const cv::Mat *last, int optStillSamplingPercent) {
				std::chrono::milliseconds dura(100);
	if(last == NULL || last->empty()) { // we discard the first frame
		return true;
	}
	if(!current->isContinuous() || !last->isContinuous() || current->channels() != 1 || current->depth() != CV_8U || last->channels() != 1 || last->depth() != CV_8U) {
		throw std::invalid_argument("StillFilter::hasChanged: both arguments are expected to be continuous and of same parameters.");
	}
    int len = current->cols * current->rows;
	// number of pixels to examine
	int n = len * optStillSamplingPercent / 100;
	const char* cp = current->ptr<char>(0);
	const char* lp = last->ptr<char>(0);
	int rel = 0; // common relative index
	int nDiff = 0;
	for(int i = n; i > 0; i--) {
		int diff = (int)(cp[rel]) - (int)(lp[rel]);
		if(diff < 0) {
			diff = -diff;
		}
		if(diff > Arguments::optStillNoiseThreshold) {
			nDiff++;
		}
		// this increment is expected to be smaller than len
		rel += Arguments::optStillSamplingInc;
		while(rel > len) { // avoid division on ARM v6
			rel -= len;
		}
	}
	return nDiff * 100 > n * Arguments::optStillDeflectionPercent;
}

int StillFilter::dividor(int len, int div) {
	// minimal length after division
    int min = 16;
    int c = len + (min >> 1);
    int ret = div;
    if(c / div < min) {
        ret = c / min;
        if(ret == 0) {
            ret = 1;
        }
    }
	return ret;
}

std::set<SharpTile>* StillFilter::checkSharpness(const cv::Mat& frame) {
	if(!frame.isContinuous() || frame.channels() != 3 || frame.depth() != CV_8U) {
        throw std::invalid_argument("StillFilter::checkSharpness: frame should be unsigned char encoded YCrCB with continuous storage.");
    }
	// place to gather the sharp tiles
	std::set<SharpTile> *sharp = new std::set<SharpTile>();
	const unsigned char *image = frame.ptr();
    int imageHeight = frame.rows;
	int imageWidth = frame.cols;
    int divHor = dividor(imageWidth, Arguments::optSharpTilesPerSide);
    int divVert = dividor(imageHeight, Arguments::optSharpTilesPerSide);
    int dividedWidth = imageWidth / divHor;
    int dividedHeight = imageHeight / divVert;
    int lastWidth = imageWidth - dividedWidth * (divHor - 1) - 1;
    int lastHeight = imageHeight - dividedHeight * (divVert - 1) - 1;
    int *lenHor = new int[divHor];
	for(int i = divHor - 2; i >= 0; i--) {
		lenHor[i] = dividedWidth;
	}
    lenHor[divHor - 1] = lastWidth;
    int *lenVert = new int[divVert];
	for(int i = divVert - 2; i >= 0; i--) {
		lenVert[i] = dividedHeight;
	}
    lenVert[divVert - 1] = lastHeight;
	int lineLen = imageWidth * 3;
    for(int fx = 0; fx < divHor; fx++) {
        int thisWidth = lenHor[fx];
        for(int fy = 0; fy < divVert; fy++) {
            int thisHeight = lenVert[fy];
			// number of horizontally or vertically adjacent samples exceeding lower limit
            register int dVertL = 0, dHorL = 0;
			// number of horizontally or vertically adjacent samples exceeding higher limit
            register int dVertH = 0, dHorH = 0;
			int startX = fx * dividedWidth;
			int startY = fy * dividedHeight;
			register int y = startY;
			register const unsigned char *thisLine = image + lineLen * y + startX * 3;
			int endy = startY + thisHeight;
			for(; y < endy; y++) {
				register int endx = startX + thisWidth;
	            for(register int x = startX; x < endx; x++) {
                    register int d = (int)(thisLine[lineLen]) - (int)(*thisLine);
					if(d < 0) {
						d = -d;
					}
                    if(d > Arguments::optSharpDiffLow) {
                        dVertL++;
                        if(d > Arguments::optSharpDiffHigh) {
                            dVertH++;
                        }
                    }
                    d = (int)(*thisLine);
					thisLine += 3;
					d -= (int)(*thisLine);
					if(d < 0) {
						d = -d;
					}
                    if(d > Arguments::optSharpDiffLow) {
                        dHorL++;
                        if(d > Arguments::optSharpDiffHigh) {
                            dHorH++;
                        }
                    }
				}
				thisLine += lineLen - thisWidth * 3;
            }
			// if the ratio of lower/higher differences is small, there are likely to be edges
			// yielding higher differences
            int highPercentV = -1, highPercentH = -1;
			// we need at least so many differences in one direction as the rectangle side length
            if(dHorL >= dividedWidth) {
				highPercentH = dHorH * 100 / dHorL;
            }
            if(dVertL >= dividedHeight) {
				highPercentV = dVertH * 100 / dVertL;
            }
			int highPercent = highPercentH > highPercentV ? highPercentH : highPercentV;
            if(highPercent > Arguments::optSharpHighPercent) {
				sharp->insert(SharpTile(highPercent, thisWidth, thisHeight, startX, startY));
            }
		}
	}

	delete[] lenHor;
	delete[] lenVert;
	return sharp;
}
