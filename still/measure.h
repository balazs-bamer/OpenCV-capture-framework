/** @file
Classes intended for measurements on different hardware sensors. No specific behaviour implemented.

Copyleft Balázs Bámer, 2015.
*/

#ifndef PROJECTOR_MEASURE_H
#define PROJECTOR_MEASURE_H

#include<set>
#include"util.h"

#if USE_NVWA == 1
#include"debug_new.h"
#endif


namespace projector {
	
	/**
    Base class for sensor data.
    */
    class Measurement {
	protected:
		/**
		Measurement time point.
		*/
        Stopper timestamp;
	public:
		/**
		Does nothing now.
		*/
		Measurement() {};
    };

	/**
    Class for continuously acquiring sensor data.
    */
    class PollSensors : public StartStop {
    public:
		/**
		Only sets the debug prefix if needed.
		*/
		PollSensors() {
			DEBPREF("sensors");
		}

    protected:
        /**
        Writes only a debug line if needed.
        */
        void measureDummy();

        /**
        Do polling in separate thread.
        */
        virtual void run();

        /**
        Does nothing.
        */
        virtual void cleanup() {};
    };
}

#endif
