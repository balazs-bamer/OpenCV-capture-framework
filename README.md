# A flexible embeddable OpenCV capture framework for Raspberry Pi

## Motivation

After some experiments with a simple OpenCV video capture loop on the Raspberry Pi, I realised that it needs custom debug solutions. Moreover, the Video for Linux 2 capturing framework spends a considerable amount of time on waiting for the frames, which time could be used effectively in a multithreaded application. So I decided to develop a flexible framework fulfilling the following aims:
* Multithreaded architecture with separate capture and frame processing threads. It has advantages on single-core CPUs, but can be really successful on multicore systems.
* Optional filtering for unchanged (still) and / or sharp frames. This mechanism could prevent processing of blurred frames. As the targeted architecture has limited CPU power, these routines must be fast.
* Reusable captured frames based on a [previous work](https://github.com/balazs-bamer/OpenCV-V4L2-directFormat).
* Flexible configuration possibilities make-time and runtime.
* Command-line invocation.
* An example frame processor with simple processing code and output to file. This example implementation waits until the camera sees a still picture with sharp parts, darkens the unsharp regions and writes the image into a file.
* Versatile debug mechanism providing
  * tracking events and statuses in separate threads simultaneously.
  * possibility to use standard output or memory buffer. The latter provides
    less overhead and can be dumped to file on program exit.
  * limited interactive user interface using Curses or OpenCV imshow and the
    two libraries' prompt keyboard input to show frames in different stages
    of processing.
  * total elimination of debug code from production releases.
  * possibility to transparent observation and catch of memory leaks.
* Doxygen code [documentation](http://www.bamer.hu/feocaf).

## Language

Recent OpenCV releases use C++ for efficient image processing (or at least for front-end), this made C++ my language of choice. As C++11 has many important extensions such as threading and lambda functions, I have decided to use this variant. However, high-level C++ is restricted to organisation-like tasks, which helps understanding the control flow between threads.

In CPU-restricted environments nothing can beat pure loops and pointer arithmetic among block processing tasks, even if OpenCV offers efficient high-level building blocks. Custom functions can often provide faster solutions in one steps. In such cases I use custom algorithms.

Although modern C++ offers smart pointers, they introduce overhead, which I didn't want to have here, and allow lasier design of control flow. I think a strict thinking in multithreaded environment pays itself, and a memory leak detector helps control it, too.

## Compiling

I use g++ (Raspbian 4.8.2-21~rpi3rpi1) 4.8.2 and CMake 2.8.9 for compiling. The program does not compile without C++11 enabled, and it also needs some libraries. I have set the following variables:
* *CMAKE_CXX_FLAGS* = -std=c++11 -lncurses -DHAVE_LIBV4L -DHAVE_CAMV4L2 -lv4l2 -lv4l1
* *CMAKE_CXX_FLAGS_DEBUG* = -g3 -gdwarf-3 -O1

The debug flags are required by QtCreator 3.2.2 (Qt 4.8.6) for remote debugging.

## Design

### Aims

The framework consists of three parts, each realised in a separate thread:

* Capture loop and filtering sharp and/or still images. This is the main thread, the one started by the caller code.
* Processing thread providing actual calculations of the filtered frames. This is assumed to be much slower than a cycle in the first thread, so adequate message passing and status notification is required.
* Measurement thread to continuously acquiring sensor data. The framework was thought to support various sensors like compass, accelerometer, gyroscope, and provide an up-to-date measured value possibly processed by a Kalman filter. Dedicating a separate thread for this work would yield more frequent and precise measurements.

"Filtering sharp and/or still images" means the two features may be enabled independently from each other. A possible use-case of using both of them is automatic registration of regularly rearranged objects. From now on, the term *adequate frame* refers to a frame selected by the filtering thread for processing.

As processing may take much longer than a cycle in the filtering thread, not every *adequate frame* can be processed. After starting processing one, there are two cases, depending of the state of the option *-use-stale-frame*:

* The next *adequate frame* is stored and subsequent ones are discarded until the processing finishes. This requires less CPU usage, but delivers a (possibly) stale (outdated) frame for processing.
* The filtering thread continues filtering, and discards all unused frames. The last one identified just before the end of processing the previous one will be processed next.

Enabling still frame filtering will introduce repeated frames when the processing time is smaller than the period for which the scene is unchanged. This may not be desirable, so there is an option for discarding the repeated frames by prescribing a minimal duration for which the scene must change. The option is called *-still-change-time*.

For sake of simplicity, the processing thread is created using a lambda function, and has a lifetime of a single frame processing. The processing object may have the following states:

Enum state name   |Description
:-----------------|:----------
RESULT_NOIMAGE    |No current image processing, no thread object.
RESULT_PROCESSING |Image being processed, it has an active thread.
RESULT_FAIL       |The calculation could not be performed, because the image was not good enough. Inactive thread object.
RESULT_INCOMPLETE |The calculation was interrupted (return after setting finish). Inactive thread object.
RESULT_APPROXIMATE|Any-time algorithm was interrupted, or the image did not allow exact result. Inactive thread object.
RESULT_EXACT      |The result is exact. Inactive thread object.

Inactive thread object means it has nothing to do, but still waits for joining. This happens in the *status* method call. The framework supports limited processing time by using the *-handler-timeout* option. If it has a valid value, an asynchronously executed lambda function (in fact a separate thread without explicite join requirement) waits for the timeout, and then asks the actual processing to terminate. This may have the following results:
* If the option *-force-handler-exit* is enabled, the processing is supposed to end as soon as possible. The result is likely to be incomplete, or perhaps an any-time algorithm can deliver approximate result. Important is to keep the timeout.
* If this option is disabled, the request is not mandatory. Any-time algorithms will always return some result, and regular algorithms may end normally. Important is to deliver results.

To achieve this, the variable *finish* becomes true after the timeout, and the actual processing code should regurarly check it on appropriate execution spots.

### Classes

The framework consists of these classes:

Class name    |Defined in       |Description
:-------------|:----------------|:----------
Measurement   |still/measure.h  |Dummy base class for sensor data, no descendants. See more at class *PollSensors*.
Stopper       |util/util.h      |This is an auxiliary class providing stopper-like time measurements, ordering and similar tasks. It uses *std::chrono::high_resolution_clock* for measurements and calculations.
Arguments     |still_config.h.in|Note, the file still_config.h.in is processed by CMake to include CMake settings. This is a singleton class holding all the integer option values, associated arrays and methods to manipulate them.
_OptLimits    |still_config.h.in|Describes lower and upper bounds for integer options.
Debug         |util/util.h      |Class used to print execution steps with timing. The instance can be per-class basis or in a local scope and have an unique prefix. All operations, including declarations should be done by macros, so the conditional compiling can handle them. 
Showcase      |util/util.h      |This class provides a limited user interface. It provides an optional OpenCV window, listens OpenCV or Curses keystrokes. If debugging is enabled, it receives image display requests from Debug (see later).
StartStop     |util/util.h      |Base class providing a startable and stoppable function. This class cannot be instantiated. Subclasses must set a valid prefix in *_debug* object. See the section Debugging for more info about the *_debug*.
PollSensors   |still/measure.h  |This is a descendant of *StartStop*, intended for continuously acquiring sensor data. The FrameProcessor, or its subclass (holding the only PollSensors instance) could query these values to gain additional input for image processing. This feature is not implemented.
StillFilter   |still/still.h  |This is a descendant of *StartStop*,  implementing a framework for managing a modified VideoCapture stream and filtering out sharp images, which are fed into the handler for processing. This class manages the main framework operation. Its instance holds the actual FrameProcessor (or subclass) object passed to the constructor.
FrameProcessor|still/still.h    |Class to process the (still) images identified during capture. The images are considered to be sharp. The default implementation saves the images.
ProcessArgs   |still/still.h    |Contains all the arguments a FrameProcessor::process method call needs. 
SharpTile     |still/still.h    |Describes a sharp tile of the image, see the section Algorithms for more info.

### The main loop and messaging between threads

There is a big loop for frame grabbing and filtering in method *StillFilter::run*. The loop is long, but contains all the logic for executing the two filtering routines as well as handling stale or fresh frames, minimal required time in changing frames and downsampling for still checking. Breaking it into several function would require many variables to pass over or be placed as class members. To compensate its length and complexity I put verbose comments in the code.

There would be the possibility to implement the filters as a set of plugins, each written in a subclass or template instance. I have discarded this pattern, because a general solution would be complicated and my primary focus was performance and simple code. However, the frame checking algorithms are in separate methods *StillFilter::hasChanged* and *StillFilter::checkSharpness*.

Just near the loop begin I ask OpenCV to grab the frame. This happens always, even if we have a stored stale frame for the next processing. Omitting frame grabbing caused sometimes a problem. However, retrieving it occurs only when I really need it.

I check several *Arguments::opt...* variables, which are subject to changes runtime (see later). The ones I read more than once during a run of the loop I store in local variables to get a consistent behaviour during the particular run. I also need to invalidate variables used across the loop runs if possibly changing options influence their value. 

After it, I read a frame, either in grayscale if still check is performed, or direct in YCrCb in full resolution if not. Through the loop the *goOn* variable signs whether if the frame could be grabbed and retrieved, and if later it passed any of the desired tests. If once there is a failure, subsequent steps are skipped. If this happens, processing may still be started if we use stale frames and there is one saved.

The true value of member variable *StartStop::started* signs that filtering and normal operation is on in StillFilter (more info [here](http://www.bamer.hu/feocaf/classprojector_1_1StartStop.html)). I make it live somewhat longer when waiting for the processor to end, because the *StillFilter::cleanup* initiates processing finish. No filtering occurs from now on, but the thread cannot be joined while the processing runs.

Processing is started in the *FrameProcessor::process* method call. It spawns a new thread using a lambda function for the actual processing, and if timeout is enabled, an other lambda in an *std::async* to implement the timeout. The *FrameProcessor::status* method is used by the main loop to query the processing status. If it reaches some of the end statuses, the processor thread is definitely done, so it gets joined. The status is reset to *RESULT_NOIMAGE* to sign the processor is ready for the next frame.

I hand over *ProcessArgs* pointers to the *FrameProcessor*, which deletes them after processing is ready.

## Frame checking algorithms

Currently all image processing is performed using grayscale images (or only the Y component of the YCrCb color format) in 8-bit unsigned numbers.

My Raspberry Pi computer has a CPU with ArmV6 architecture, which lacks integer division. This was an important factor during algorithm design and implementation.

### Determining if the frame is identical to the previous one

This is quite simple, and runs on a (possibly donsampled) grayscale image. I take both frames, and compare them pixel-by-pixel, allowing a slight noise margin: if *abs(Pnow - Pprev) > Arguments::optStillNoiseThreshold* the number of differences (*nDiff*) is increased. However, doing so for all the pixels is slow and unnecessary. To overcome this, I sample the frames in an even, quasy-random way. I have an index relative to the frame begin (I require all the frames to store pixels continuously), which I increase by a constant modulo the array length. The increment is required to be a suitable relative prime to the length in order to avoid sample repeating. This method allows nearly arbitrary many samples to examine, because a suitable prime yields more-or-less even distribution pretty soon.

Thus, the frames are considered different if 

```nDiff > length * Arguments::optStillSamplingPercent * Arguments::optStillDeflectionPercent / 10000```

Where *length* is 

```width * height / 4 ^ -Arguments::optStillDownsampleExponent```

Comparing full-resolution frames costs, too. To overcome this, and prevent very small movements make the frames look different, I use downsampled frames for this check. Downsampling can happen by factors 2, 4 or 8. The option *-still-downsample-exponent* controls this. Thus the total number of observed pixels is 

```width * height / 4 ^ -Arguments::optStillDownsampleExponent * Arguments::optStillSamplingPercent / 100```

### Determining if the frame is sharp

This is a tough question. There are many images, especially shiny, smooth surfaces with big-radius curvatures under diffuse lighting, for which this is impossible. I remember an shiny stainless stell plate with embossed repetitive pattern which confused even my eyes, failing to focus on it.

There are many principles to solve it, most of them do some sort of edge detection or if there is much CPU power, 2D FFT. For me, even edge detection seemed too expensive, so after some experiments with Octave I found this algorithm. I use Y component of full resolution YCrCb encoded frames, as this format will be the subject of frame processing.

I require only a part of the image to be sharp. This means a measure computed for the whole image would probably fail by disappearing in image noise or falling below the decision threshold. To overcome this, I divide the image into smaller rectangles, *-sharp-tiles-per-side* pieces each side. However, this dividor is adjusted to make sure the rectangles don't become too small (having a side less then 16 pixels). This happens in *StillFilter::dividor*. For an image considered to be sharp I require some sharp sub-rectangles to be found. This count is controlled by the option *-sharp-tiles-req*.

Many sharpness detection algorithms begin with a (Gaussian) blur to cancel noise. I have found this step to be too expensive for me, so I decided to deal with noise indirectly. My approach (method *StillFilter::checkSharpness*) is counting the neighbouring pixels having big difference enough, and doing this vertically as well as horizontally. The experience shows that taking this two main directions lets me detect sharp lines or curves with any slope. This way I have to compute

```2 * (width - 1) * (height - 1)```

differences for the whole image. Each difference is compared against two limits, *-sharp-diff-low* and *-sharp-diff-high*, and if either is exceeded, any of the four variables counting horizontal and vertical differences for lower and upper limits is increased. This happens independently for the small rectangles.

For each (horizontal / vertical) direction in each rectangle, there are two criteria for sharpness: 
* the number of differences exceeding the lower limit must reach the corresponding rectangle side length
* the number of differences exceeding the higher limit must reach a given percentage (*-sharp-high-percent*) of the number of differences exceeding the lower limit.
If this holds for a rectangle for any direction, the high to low ratio (or the better if both directions match) together with the rectangle upper left corner and dimensions) are inserted in a *std::set<SharpTile>* for possible further usage during frame processing.

For some reason my webcam driver returns frames with the bottom line having a more-or-less uniform darker color, which introduces false positive sharp rectangles if the background is light enough. I let it happen because I don't want the algorithm to be specialized for a faulty driver.

My tests show that the algorithm handles average scenes containing edges correctly. Of course it is easy to show particular images without sharp edges but high gradient in brightness that make my algorithm fail. Image brightness and contrast also influences its behaviour, for example underexposed but sharp areas won't be found sharp. Exposure and lighting must be adjusted to help the algorithm.

## Configuration and options

CMake is responsible for preconfiguring all the possible options. Some of them affects compiling, but all the others can be overriden from command-line, or even changed during runtime. Integer options are checked against lower and upper bounds, and if outside, an exception is thrown.

### Options

Command-line options with CMake setting require an integer argument, but the ones without CMake setting work without argument. These are effective only runtime, and are used only for debugging.

For more information on the options related to still image and sharpness detection, refer to the part Algorithms.

Name in CMake|Command-line|Default value|Lower bound|Upper bound|Description
:------------|:-----------|------------:|----------:|----------:|:----------
-                        |-help                      |-            |- |-    |Displays the command-line options with their current settings, then exists.
-                        |-show-opts                 |-            |- |-    |Displays the current option values before run.
-                        |-use-curses                |-            |- |-    |Uses Curses to real-time display values, see part Curses output.
-                        |-show-window               |-            |- |-    |Shows a window to display frames during operations, see *DEBIMG*.
USE_NVWA                 |-                          |0            |0 |1    |If 1, use NVWA library to catch new-delete memory leaks. Changing it requires complete recompile.
DEBUG_OUTPUT             |-                          |0            |0 |1    |Debug output with timing info. Changing it requires complete recompile.
DEBUG_STDOUT             |-                          |0            |0 |1    |If 1, output goes to stdout, if 0, into DEBUG_LOC.
DEBUG_LOC                |-                          |/tmp/diag.log|- |-    |Debug output location.
OUTPUT_FILE_PREFIX       |-                          |/tmp/result_ |- |-    |Output file prefix including path.
VIDEO_NUM                |-video-num                 |0            |0 |9    |/dev/video[num]
GETCH_DELAY              |-getch-delay               |200          |10|5000 |Wait period in ms during getch in user interface. OpenCV *imshow* repeats displaying the frame for 5 times this value. This was important for me to reduce the load introduced by remote desktop image transfer.
HANDLER_TIMEOUT          |-handler-timeout           |0            |0 |2000 |Timeout in ms for handler processing, 0 if none. If enabled, after timeout the processing is asked to finish. The implementation may cancel processing or provide inaccurate results.
FORCE_HANDLER_EXIT       |-force-handler-exit        |0            |0 |1    |Force handler exit without a result on timeout or finish. If enabled, the above request is mandatory, processing must end as soon as possible.
USE_STALE_FRAME          |-use-stale-frame           |0            |0 |1    |If enabled, use stale frame stored duringh previous processing, otherwise wait for an appropriate one before beginning next processing.
STILL_DOWNSAMPLE_EXPONENT|-still-downsample-exponent |3            |0 |3    |Downsampling for still scene check happens at 2**exponent. 0 means no downsampling. Greater values allow slighter movements.
STILL_CHANGE_TIME        |-still-change-time         |500          |0 |10000|Time (ms) to consider a still image different from the previous one. This option prescribes a minimum time the scene must be moving before a still image is chosen for processing. 0 means no such requirement.
STILL_NOISE_THRESHOLD    |-still-noise-limit         |10           |1 |100  |Noise threshold when detecting still images. Pixels within this value difference are considered to be the same.
STILL_SAMPLING_INC       |-still-sample-inc          |97           |2 |4441 |Sampling increment for detecting still images. Must be relative prime to the image size.
STILL_SAMPLING_PERCENT   |-still-sample-percent      |10           |0 |20   |Percentage of image to compare for detecting still images.
STILL_DEFLECTION_PERCENT |-still-deflection-percent  |3            |0 |20   |Allowed deflection percentage for images considered to be the same.
SHARP_TILES_PER_SIDE     |-sharp-tiles-per-side      |10           |1 |40   |Number of tiles per image side for sharpness detection.
SHARP_DIFF_LOW           |-sharp-diff-low            |15           |1 |100  |Lower limit of adjacent brightness difference.
SHARP_DIFF_HIGH          |-sharp-diff-high           |30           |2 |100  |Upper limit of adjacent brightness difference.
SHARP_HIGH_PERCENT       |-sharp-high-percent        |60           |0 |100  |Required percentage of differences exceeding the higher limit to that exceeding only the lower limit.
SHARP_TILES_REQUIRED     |-sharp-tiles-req           |4            |0 |100  |Number of sharp tiles required for sharp image, 0 if check disabled.

### Principle of configuration

CMake processes the raw header file still_config.h.in to replace the placeholders between @ signs with the default values from CMakeLists.txt or user-overridden values after an interactive CMake run. The class Arguments is responsible for maintaining a set of static variables (beginning with *opt*) for all other parts of the program, filling with the defined values and optionally overriding them from command-line options. These static variables have public access and similar name to the above options. Their value may be changed runtime to allow change the actual operation or algorithm parameters.

Note, *USE_NVWA* and *DEBUG_OUTPUT* are macros only, they do not have corresponding variables. These control compilation.

The enum values in *Options* are used only during options processing to identify them. The file still_config.cpp contains the variable definitions for configuration or option management.

I use the *getopt_long_only* call to decode the command line arguments. This is not POSIX compliant, but this way I can get rid of some - signs (-: This function uses the *Arguments::options* array. The run-time-only options write values (1) direct into the static variables, which all have the default value of 0. For other options, which take integer arguments, the function returns the appropriate enum value. If the option is unknown, an *std::invalid_argument* is thrown.

Next, the *Arguments::optLimits* array is used to get the lower and upper bounds. If the lower limit is not lower than the upper limit the corresponding argument is not integer. This would require a switch, but I have no such option now.

If the actual integer value is outside the bounds, an *std::out_of_range* is thrown. Otherwise, the value is written into the appropriate static variable using *Arguments::optLimits.pointer*.

This mechanism saves a big switch.

## Debugging

Debugging multithreaded applications only in debugger is impossible. It is important to see what and where happens, what is the status of different threads at a time point. For this purpose I have designed a lightweight logging system, which is capable of displaying OpenCV *Mat* objects as well.

### Debug log and window

The logging system consists of
* a set of macros allowing conditional compilation of the whole feature.
* the *Debug* class, which allows
  * using arbitrary scopes such as global, class instances or code blocks
    * each scope has an own string prefix and a Stopper instance - the _debug *Debug* object holds it. Important! If the scope does not contain such a variable, the macros fail. Declaration is done using the *DEBDEC*, *DEBDECP* and *DEBPREF* macros. See [util.h](http://www.bamer.hu/feocaf/util_8h.html#define-members) for more details.
    * preceding each output in the log by the time in seconds elapsed since the last print in this scope and the scope prefix
    * printing character strings optionally with decimal or hexadecimal integers (*DEB1*, *DEB2*, *DEB2X*)
    * printing frames per seconds info, optionally also on the screen when Curses is enabled (*DEBFPS*)
    * registering locations (*DEBIMG* macro) with a one-digit ID where an arbitrary OpenCV *Mat* instance can be displayed in the window if the option *-show-window* is enabled.
  * receiving keystrokes from the user (*DEBKEY*). The keystrokes are then distributed to all the *Debug* instances to let the registered location with the same ID display its image the next time it is hit. This happens once per keystroke, so the image remains visible.
* the Showcase class, which is responsible of
  * receiving user keystrokes either from the OpenCV window if present, or from Curses, if enabled, or none at all. This feature is always on, so this class will always be compiled.
  * transmitting it to *Debug* (*DEBKEY*)
  * receiving the image to display from *Debug* if the corresponding *DEBIMG* location was hit after a keystroke. If *-show-window* is enabled, it displays it. By default, it displays an "eye" matrix.

Currently when *-show-window* is enabled, the raw frames read from the video stream can be displayed when the key 1 is hit.

Here is part of a sample debug output using stale frames. An earlier processing is in progress, now obtaining the new stale frame, and after finishing the old processing the stored stale one begins immediately:

```
0.000095 filter: 0 loop begin.
0.038853 filter: 1 frame grabbed.
0.000012 retrieve: colsp 0
0.019927 filter: 2 frame retrieved.
         filter: image summary and display req: n- i1
         filter n: 119 last: 0.059230 fps! 16.883336 avg: 0.041172 fps: 24.288158
0.000324 filter: 3 frame changed.
0.059228 proc: status: 0
0.000096 filter: 0 loop begin.
0.012072 filter: 1 frame grabbed.
0.000017 retrieve: colsp 0
0.017972 filter: 2 frame retrieved.
         filter: image summary and display req: n- i1
         filter n: 120 last: 0.030503 fps! 32.783661 avg: 0.041083 fps: 24.340716
0.001197 filter: 3 frame not changed, enough time spent in change 531
0.086407 filter: 4 big frame retrieved.
0.059464 filter: 5 sharpness ready, tiles: 7
0.000150 filter: 6 updated stale.
0.177419 proc: status: 0
0.000089 filter: 0 loop begin.
0.002111 filter: 1 frame grabbed.
         filter n: 121 last: 0.149474 fps! 6.690127 avg: 0.041979 fps: 23.821308
0.002400 proc: status: 0
0.000278 filter: 0 loop begin.
0.002104 filter: 1 frame grabbed.
         filter n: 122 last: 0.002401 fps! 416.493128 avg: 0.041655 fps: 24.006831
0.002405 proc: status: 0
0.000288 filter: 0 loop begin.
0.022373 filter: 1 frame grabbed.
         filter n: 123 last: 0.022678 fps! 44.095599 avg: 0.041501 fps: 24.096074
0.022674 proc: status: 0
0.000292 filter: 0 loop begin.
0.001768 filter: 1 frame grabbed.
         filter n: 124 last: 0.002079 fps! 481.000481 avg: 0.041183 fps: 24.282083
0.002186 proc: status: 0
0.000418 filter: 0 loop begin.
0.040437 filter: 1 frame grabbed.
         filter n: 125 last: 0.040876 fps! 24.464233 avg: 0.041180 fps: 24.283529
0.040752 proc: status: 0
0.000292 filter: 0 loop begin.
0.450536 doProc: JPEG ready.
0.024669 proc: processing ready, it took  0.768723
0.031762 filter: 1 frame grabbed.
         filter n: 126 last: 0.032071 fps! 31.180818 avg: 0.041108 fps: 24.326231
0.007396 proc: status: 4
0.000037 proc: status reset to noimage
0.000033 proc: status: joining proc thread...
0.000156 proc: status: deleted proc thread.
         proc n: 1 last: 0.780091 fps! 1.281902 avg: 0.780091 fps: 1.281902
0.000909 filter: 7 stale frame will be processed.
0.000067 filter: 0 loop begin.
0.003414 proc: processing...
0.000013 doProc: start
0.064231 filter: 1 frame grabbed.
0.029111 filter: 2 frame retrieved.
         filter: image summary and display req: n- i1
         filter n: 127 last: 0.094359 fps! 10.597823 avg: 0.041527 fps: 24.080609
0.000317 filter: 3 frame changed.
```

### Curses output

Curses output can be used to real-time monitor events and variables. For this purpose there are two sort of function, both effective only when *-use-curses* is enabled.

* cClear clears the screen. Should be called right before the first cPrintw in the cycle.
* cPrintw accepts a character string and as a second argument signed or unsigned int, signed or unsigned long or double. This is implemented using a template to let it distinguish between caller argument types. Simple overloading fails even for integers and floating point values.

### Saving images

Images can be saved on demand when the programmer implements it like in *FrameProcessor::doProcess*. A slight hint is the macro *OUTPUT_FILE_PREFIX* which defines a location (path and filename prefix) for saving.

### Catching memory leaks

I have found the [NVWA](http://sourceforge.net/projects/nvwa/) project, which contains a brilliant memory leak detector working by overloading C++ *new* and *delete* operators. It can be enabled by the *USE_NVWA* option in CMake. If disabled, neither the headers are compiled, nor the libraries are linked. I include the headers only in my code, so memory operations outside it seem to cause memory leaks (144 items by me). However, these are constant, and do not mean any problem. I have double-checked: introducing a memory leak in my code writes source file name and line number as well, which is not present in the fake memory leak reports.

## Performance

I have made all measurements on a single-core 700 Mhz Raspberry Pi B+ with factory settings (no overclock or overvoltage). I have compiled both OpenCV 3.0.0 and my routines with g++ (Raspbian 4.8.2-21~rpi3rpi1) 4.8.2 using *-O1*. The Logitech C250 USB webcam delivers 640x480 pixel video stream. This is converted and optionally downsampled into grayscale for still frame checking, or the same image is retrieved in full resolution YCrCb color format for sharpness and further processing.

### Filter algorithms

In the first set of measurements *FrameProcessor::doProcess* contained only a sleep. The *-use-stale-frame* option was set to 0 to force all grabbed frames go through the algorithms. Note, that *-still-sample-percent*=0 means still frame identification is off. I haven't carried out more measurements with different sharpness settings, because they do not really affect the CPU usage.

-still-sample-percent|Sharpness|Downsample exponent|FPS|CPU usage %
--------------------:|--------:|------------------:|--:|----------:
10|off|3|23.3|32
20|off|3|21.7|30
10|off|2|23.5|36
20|off|2|25.7|39
10|off|1|21.3|48
20|off|1|21.3|54
10|off|0|19  |50
20|off|0|20.3|74
10| on|3|22|60

### Sample *FrameProcessor::doProcess* implementation

This method has two parts.
* First, it uses OpenCV *mixChannels* to convert the YCrCb image to grayscale, and if there is sharp tiles information, highlight them. (More precisely, the remaining image parts are darkened.)
* Then it saves the result in JPEG format.

Average processing time (with tile information) was 0.13 s, that of JPEG save 0.16 s. I check the variable *finish* after the highlight loop and abort the processing if it is false.

### Combined performance

This measurement required the option *-still-change-time* to be set to 0 to let all subsequent still frames be used. I used high-contrast sharp still scene to let all the filter calls succeed.

*-use-stale-frame*|*StillFilter* FPS|*FrameProcessor* FPS|CPU usage %
-----------------:|----------------:|-------------------:|----------:
0|12.8|1.8|97
1|21.5|2.6|91

It's easy to see that by using stale frames the filter thread can run at a much higher pace than by reliyng only always on fresh frames, However, its filtering FPS value mean only the count of grabs, because it performs just as many filtering as frame processing.

Note, this CPU has only one core. Using a multicore CPU could result in much higher frame rate.
