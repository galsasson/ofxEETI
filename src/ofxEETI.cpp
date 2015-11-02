//
//  ofxEETI.cpp
//  ofxEETI
//
//  Created by Gal Sasson on 10/30/15.
//
//

#include "ofxEETI.h"

#define DEBUG_SERIAL

static unsigned char eeti_alive[3] = { 0x0a, 0x01, 'A' };
static unsigned char eeti_fwver[3] = { 0x0a, 0x01, 'D' };
static unsigned char eeti_ctrlr[3] = { 0x0a, 0x01, 'E' };


ofxEETI::~ofxEETI()
{
	stop(true);
	if (serial.isInitialized()) {
		serial.close();
	}
}

ofxEETI::ofxEETI()
{
	bInitialized = false;
	bThreadRunning = false;
	
	for (int i=0; i<MAX_TOUCH; i++) {
		touches[i].bDown = false;
		touches[i].id = i;
		touches[i].x=0;
		touches[i].y=0;
		touches[i].bReport = false;
	}
}

bool ofxEETI::setup(const string &devname, int baudrate)
{
	ofLogNotice("ofxEETI") << "openning touch device: " << devname << "@"<<baudrate<<"bps";
	bool success = serial.setup(devname, baudrate);
	if (success) {
		if (initEETI()) {
			ofLogNotice("ofxEETI") << "found EETI eGalax touch panel at "<<devname;
		}
		else {
			ofLogError("ofxEETI") << "device at "<<devname<<" is not an EETI eGalax touch panel";
			serial.close();
			return false;
		}
	}
	else {
		ofLogError("ofxETTI") << "error openning device";
		return false;
	}
	
	return true;
}


void ofxEETI::start()
{
	if (bThreadRunning) {
		ofLogWarning("ofxEETI") << "start called while thread already running";
		return;
	}

	if (!bInitialized) {
		ofLogError("ofxEETI") << "EETI touch panel is not initialized";
		return;
	}

	bThreadRunning = true;
	thread = std::thread(&ofxEETI::threadFunction, this);

//	ofAddListener(ofEvents().update, this, &ofxEETI::update, OF_EVENT_ORDER_AFTER_APP);
}

void ofxEETI::stop(bool wait)
{
	if (!bThreadRunning) {
		return;
	}

	bThreadRunning = false;
	if (wait) {
		thread.join();
	}

//	ofRemoveListener(ofEvents().update, this, &ofxEETI::update, OF_EVENT_ORDER_AFTER_APP);
}

bool ofxEETI::initEETI()
{
	usleep(200000);
	serial.flush();
	
	unsigned char buff[1024];	// should be more than enough
	
	serial.writeBytes(eeti_alive, 3);
	// wait for response
	int timeout=100;
	while (serial.available()<3 && timeout-->0) {
		usleep(1000);
	}
	
	if (timeout==0) {
		ofLogError("ofxEETI") << "timeout waiting for touch panel response";
		return false;
	}
	
	int length = serial.available();
	serial.readBytes(buff, length);
	printf("alive response: \n");
	for (int i=0; i<length; i++) {
		printf("0x%0x ", buff[i]);
	}
	printf("\n");
	if (buff[0] != eeti_alive[0] ||
		buff[1] != eeti_alive[1] ||
		buff[2] != eeti_alive[2]) {
		return false;
	}
	
	bInitialized = true;
	return true;
}

void ofxEETI::threadFunction()
{
	unsigned char* buff = (unsigned char*)malloc(6);

	while (bThreadRunning) {
		while (serial.available() >= 6) {
			int count = serial.readBytes(buff, 6);
			parsePacket(buff);

#ifdef DEBUG_SERIAL
			for (int i=0; i<count; i++) {
				printf("0x%0x ", buff[i]);
			}
			printf("\n");
#endif

		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	free(buff);
}

void ofxEETI::parsePacket(const unsigned char *buff)
{
	int id = (buff[5]&0xf) - 1;
	int x = buff[1]<<7 | buff[2];
	int y = buff[3]<<7 | buff[4];
	bool bDown = buff[0]&0x1;

	Touch& touch = touches[id];
	if (bDown) {
		if (touch.bDown == false) {
			// this is a new touch (touch down)
			touch.x = x;
			touch.y = y;
			touch.bDown = true;
			touch.type = ofTouchEventArgs::down;
			touch.bReport = true;
			addEvent(touch);
		}
		else {
			// touch already exist (touch move)
			if (touch.x == x &&
				touch.y == y) {
				// same place, do nothing
				return;
			}
			else {
				touch.x = x;
				touch.y = y;
				touch.bReport = true;
				touch.type = ofTouchEventArgs::move;
				addEvent(touch);
			}
		}
	}
	else {
		if (touch.bDown == false) {
			// touch up for touch we don't have
			ofLogWarning("ofxEETI") << "warning: got touch up for touch we don't have";
			return;
		}
		else {
			// normal touch up
			touch.bDown = false;
			touch.type = ofTouchEventArgs::up;
			touch.x = x;
			touch.y = y;
			touch.bReport = true;
			addEvent(touch);
		}
	}
}

void ofxEETI::update()
{
	touchMutex.lock();
	vector<Touch> sendEvents = events;
	events.clear();
	touchMutex.unlock();
	
	for (Touch& event: sendEvents) {
		ofNotifyEvent(eventTouch, event, this);
	}
}

void ofxEETI::update(ofEventArgs &args)
{
	return update();
}

void ofxEETI::addEvent(const Touch& touch)
{
	touchMutex.lock();
	events.push_back(touch);
	touchMutex.unlock();
}
