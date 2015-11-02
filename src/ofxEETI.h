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

#define MAX_TOUCH 10

class ofxEETI
{
public:

	~ofxEETI();
	ofxEETI();

	bool setup(const string& devname, int baudrate, bool parseSerialInThread=true);
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

private:
	ofSerial serial;
	std::thread thread;
	bool bInitialized;
	bool bRunning;
	bool bParseSerialInThread;
	Touch touches[MAX_TOUCH];
	vector<Touch> events;
	std::mutex touchMutex;


	bool initEETI();
	int waitForResponse(int nBytes, unsigned int timeout_ms);	// returns number of bytes available
	void threadFunction();
	void parsePacket(const unsigned char* buff);
	void update(ofEventArgs& args);
	void addEvent(const Touch& touch);
	
	// for debug
	void printBuffer(unsigned char* buffer, int count);
};

#endif /* defined(__ofxEETI__ofxEETI__) */
