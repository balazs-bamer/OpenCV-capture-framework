/** @file
Properties for retrieval of V4L2 captured frames using custom settings.

Copyleft Balázs Bámer, 2015.
*/

#ifndef CV_VIDEOIO_RETRIEVE
#define CV_VIDEOIO_RETRIEVE

#include <opencv2/core.hpp>

/**
Downsampling possibilities: original size, divide by 2, 4, 8.
*/
enum RetrDownsample { DS_ORIGINAL, DS_HALF, DS_QUARTER, DS_OCT };

/**
Color format possibilities. CS_BGR is not implemented, CS_YCRCB is used instead.
*/
enum RetrColorspace { CS_GRAY, CS_YCRCB, CS_BGR };

/**
Struct describing the retrieval options.
*/
typedef struct RetrieveProps {
	/** Downsampling of the full frame.
	*/
    RetrDownsample sampling;

	/** Region of interest in the frame, considered only when sampling is DS_ORIGINAL.
	*/
    cv::Rect_<unsigned> region;

	/** Resulting color format.
	*/	
    RetrColorspace colorspace;

	/**
	Returns the downsampling denominator for calculations.
	*/
	int getDenominator() {
		return 1 << sampling;
	}

	/**
	Returns the number of channels for the color format.
	*/
	int getChannels() {
		return colorspace == CS_GRAY ? 1 : 3;
	}
} RetrieveProps;

#endif
