//
//  ofxEETI.h
//  ofxEETI
//
//  Created by Gal Sasson on 10/30/15.
//
//

#ifndef __ofxEETI__ofxEETI__
#define __ofxEETI__ofxEETI__

#include <stdio.h>
#include "ofMain.h"

#define MAX_TOUCH 16

class ofxEETI
{
public:

	~ofxEETI();
	ofxEETI();

	bool setup(const string& devname, int baudrate);
	void start();
	void stop(bool wait=false);

	struct Touch {
		bool bDown;
		ofTouchEventArgs::Type type;
		int id;
		int x;
		int y;
		bool bReport;
	};

	ofEvent<Touch> eventTouch;
	
	void update();

private:
	ofSerial serial;
	std::thread thread;
	bool bInitialized;
	bool bThreadRunning;
	Touch touches[MAX_TOUCH];
	vector<Touch> events;
	std::mutex touchMutex;


	bool initEETI();
	void threadFunction();
	void parsePacket(const unsigned char* buff);
	void update(ofEventArgs& args);
	void addEvent(const Touch& touch);
};

#endif /* defined(__ofxEETI__ofxEETI__) */
