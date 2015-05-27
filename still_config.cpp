#include<iostream>
#include<stdexcept>
#include"util.h"
#include"still_config.h"

#if USE_NVWA == 1
#include"debug_new.h"
#endif


namespace projector {

	int Arguments::optHelp = 0;
	int Arguments::optShowOpts = 0;
	int Arguments::optUseCurses = 0;
	int Arguments::optShowWindow = 0;
	int Arguments::optVideoNum = VIDEO_NUM;
	int Arguments::optGetchDelay = GETCH_DELAY;
	int Arguments::optHandlerTimeout = HANDLER_TIMEOUT;
	int Arguments::optForceHandlerExit = FORCE_HANDLER_EXIT;
	int Arguments::optUseStaleFrame = USE_STALE_FRAME;
	int Arguments::optStillDownsampleExponent = STILL_DOWNSAMPLE_EXPONENT;
	int Arguments::optStillChangeTime = STILL_CHANGE_TIME;
	int Arguments::optStillNoiseThreshold = STILL_NOISE_THRESHOLD;
	int Arguments::optStillSamplingInc = STILL_SAMPLING_INC;
	int Arguments::optStillSamplingPercent = STILL_SAMPLING_PERCENT;
	int Arguments::optStillDeflectionPercent = STILL_DEFLECTION_PERCENT;
	int Arguments::optSharpTilesPerSide = SHARP_TILES_PER_SIDE;
	int Arguments::optSharpDiffLow = SHARP_DIFF_LOW;
	int Arguments::optSharpDiffHigh = SHARP_DIFF_HIGH;
	int Arguments::optSharpHighPercent = SHARP_HIGH_PERCENT;
	int Arguments::optSharpTilesRequired = SHARP_TILES_REQUIRED;

	const OptLimits Arguments::optLimits[] = {
            {OPT_NONE, 0, 0, NULL}, // getopt_long return value 0 means it has set the veriable
            {OPT_VIDEO_NUM, 0, 9, &optVideoNum},
            {OPT_GETCH_DELAY, 10, 5000, &optGetchDelay},
            {OPT_HANDLER_TIMEOUT, 0, 2000, &optHandlerTimeout},
            {OPT_FORCE_HANDLER_EXIT, 0, 1, &optForceHandlerExit},
            {OPT_USE_STALE_FRAME, 0, 1, &optUseStaleFrame},
            {OPT_STILL_DOWNSAMPLE_EXPONENT, 0, 3, &optStillDownsampleExponent},
            {OPT_STILL_CHANGE_TIME, 0, 10000, &optStillChangeTime},
            {OPT_STILL_NOISE_THRESHOLD, 1, 100, &optStillNoiseThreshold},
            {OPT_STILL_SAMPLING_INC, 2, 4441, &optStillSamplingInc},
            {OPT_STILL_SAMPLING_PERCENT, 0, 20, &optStillSamplingPercent},
            {OPT_STILL_DEFLECTION_PERCENT, 0, 20, &optStillDeflectionPercent},
            {OPT_SHARP_TILES_PER_SIDE, 1, 40, &optSharpTilesPerSide},
            {OPT_SHARP_DIFF_LOW, 1, 100, &optSharpDiffLow},
            {OPT_SHARP_DIFF_HIGH, 2, 100, &optSharpDiffHigh},
            {OPT_SHARP_HIGH_PERCENT, 0, 100, &optSharpHighPercent},
            {OPT_SHARP_TILES_REQUIRED, 0, 100, &optSharpTilesRequired},
            {OPT_END, -1, -1, NULL}
    };

	const struct option Arguments::options[] = {
            {"help", no_argument, &optHelp, 1},
            {"show-opts", no_argument, &optShowOpts, 1},
            {"use-curses", no_argument, &optUseCurses, 1},
            {"show-window", no_argument, &optShowWindow, 1},
            {"video-num", required_argument, NULL, OPT_VIDEO_NUM},
            {"getch-delay", required_argument, NULL, OPT_GETCH_DELAY},
            {"handler-timeout", required_argument, NULL, OPT_HANDLER_TIMEOUT},
            {"force-handler-exit", required_argument, NULL, OPT_FORCE_HANDLER_EXIT},
            {"use-stale-frame", required_argument, NULL, OPT_USE_STALE_FRAME},
            {"still-downsample-exponent", required_argument, NULL, OPT_STILL_DOWNSAMPLE_EXPONENT},
            {"still-change-time", required_argument, NULL, OPT_STILL_CHANGE_TIME},
            {"still-noise-limit", required_argument, NULL, OPT_STILL_NOISE_THRESHOLD},
            {"still-sample-inc", required_argument, NULL, OPT_STILL_SAMPLING_INC},
            {"still-sample-percent", required_argument, NULL, OPT_STILL_SAMPLING_PERCENT},
            {"still-deflection-percent", required_argument, NULL, OPT_STILL_DEFLECTION_PERCENT},
            {"sharp-tiles-per-side", required_argument, NULL, OPT_SHARP_TILES_PER_SIDE},
            {"sharp-diff-low", required_argument, NULL, OPT_SHARP_DIFF_LOW},
            {"sharp-diff-high", required_argument, NULL, OPT_SHARP_DIFF_HIGH},
            {"sharp-high-percent", required_argument, NULL, OPT_SHARP_HIGH_PERCENT},
            {"sharp-tiles-req", required_argument, NULL, OPT_SHARP_TILES_REQUIRED},
            {0, 0, 0, 0}
    };

	void Arguments::parse(int argc, char **argv) {
		int indexptr, i, numLimits = sizeof(optLimits) / sizeof(OptLimits);
	    for(;;) {
	        int opt = getopt_long_only (argc, argv, "", options, &indexptr);
		    if(opt == -1) {
				break;
	        }
			if(opt >= OPT_END) {
			    throw std::invalid_argument(argv[indexptr]);
			}
		    int limitLow = optLimits[opt].limitLow;
			int limitHigh = optLimits[opt].limitHigh;
	        int argInt;
		    if(limitLow < limitHigh) {
			    argInt = parseExc<int>(optarg);
				if(argInt >= optLimits[opt].limitLow && argInt <= optLimits[opt].limitHigh) {
					*(optLimits[opt].pointer) = argInt;
	            }
		        else {
			        throw std::out_of_range("Command line argument out of range");
				}
	        }
		    else {
            // here comes a switch for string arguments
			}
	    }
	}

	void Arguments::showOpts() {
		std::cout << "-use-curses: " << optUseCurses << '\n';
		std::cout << "-show-window: " << optShowWindow << '\n';
		std::cout << "-video-num: " << optVideoNum << '\n';
		std::cout << "-getch-delay: " << optGetchDelay << '\n';
		std::cout << "-handler-timeout: " << optHandlerTimeout << '\n';
		std::cout << "-force-handler-exit: " << optForceHandlerExit << '\n';
		std::cout << "-use-stale-frame: " << optUseStaleFrame << '\n';
		std::cout << "-still-downsample-exponent: " << optStillDownsampleExponent << '\n';
		std::cout << "-still-noise-limit: " << optStillNoiseThreshold << '\n';
		std::cout << "-still-change-time: " << optStillChangeTime << '\n';
		std::cout << "-still-sample-inc: " << optStillSamplingInc << '\n';
		std::cout << "-still-sample-percent: " << optStillSamplingPercent << '\n';
		std::cout << "-still-deflection-percent: " << optStillDeflectionPercent << '\n';
		std::cout << "-sharp-tiles-per-side: " << optSharpTilesPerSide << '\n';
		std::cout << "-sharp-diff-low: " << optSharpDiffLow << '\n';
		std::cout << "-sharp-diff-high: " << optSharpDiffHigh << '\n';
		std::cout << "-sharp-high-percent: " << optSharpHighPercent << '\n';
		std::cout << "-sharp-tiles-req: " << optSharpTilesRequired << std::endl;
	}
}
