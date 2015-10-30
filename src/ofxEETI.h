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

static const unsigned char eeti_alive[3] = { 0x0a, 0x01, 'A' };
static const unsigned char eeti_fwver[3] = { 0x0a, 0x01, 'D' };
static const unsigned char eeti_ctrlr[3] = { 0x0a, 0x01, 'E' };

class ofxEETI
{
public:

	~ofxEETI();
	ofxEETI();

	bool setup(const string& devname, int baudrate);
	void start();
	void stop(bool wait=false);

	ofEvent<ofTouchEventArgs> eventTouch;

private:
	ofSerial serial;
	std::thread thread;
	bool bThreadRunning;
	void threadFunction();

	void parsePacket(const unsigned char* buff);

	struct ofxEETITouch {
		int id;
		int x;
		int y;
	};

	map<int, ofxEETITouch*> touches;
	std::mutex eventsMutex;
	vector<ofTouchEventArgs*> pendingEvents;
	void addEvent(ofTouchEventArgs::Type type, int id, float x, float y);

	void update(ofEventArgs& args);

};

#endif /* defined(__ofxEETI__ofxEETI__) */
