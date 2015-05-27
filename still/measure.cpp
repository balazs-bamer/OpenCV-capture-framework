#include"measure.h"

#if USE_NVWA == 1
#include"debug_new.h"
#endif


using namespace projector;

void PollSensors::run() {
    // FIXME later perhaps only 1
    std::chrono::milliseconds dura(1000);
    while(started) {
        std::this_thread::sleep_for(dura);
        measureDummy();
    }
}

/**
Each property to measure will have a class with circular buffer to store
the measurements.
Actual values will be computed on insert along with the average insert period.
This way the properties can be queried on a time point in the past representing
the grab time of a processed frame.
*/
void PollSensors::measureDummy() {
	DEB1("dummy ready.");
}
