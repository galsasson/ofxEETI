//
//  ofxEETI.cpp
//  ofxEETI
//
//  Created by Gal Sasson on 10/30/15.
//
//

#include "ofxEETI.h"

//#define DEBUG_SERIAL

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
	bRunning = false;
	
	for (int i=0; i<MAX_TOUCH; i++) {
		touches[i].bDown = false;
		touches[i].id = i;
		touches[i].x=0;
		touches[i].y=0;
		touches[i].bReport = false;
	}
}

bool ofxEETI::setup(const string &devname, int baudrate, bool parseSerialInThread)
{
	ofLogNotice("ofxEETI") << "openning touch device: " << devname << " @ "<<baudrate<<"bps";
	bool success = serial.setup(devname, baudrate);
	if (success) {
		if (initEETI()) {
			ofLogNotice("ofxEETI") << "EETI eGalax touch panel initialized successfully";
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
	
	bParseSerialInThread = parseSerialInThread;
	return true;
}


void ofxEETI::start()
{
	if (bRunning) {
		ofLogWarning("ofxEETI") << "start called while thread already running";
		return;
	}

	if (!bInitialized) {
		ofLogError("ofxEETI") << "EETI touch panel is not initialized";
		return;
	}

	ofAddListener(ofEvents().update, this, &ofxEETI::update, OF_EVENT_ORDER_AFTER_APP);
	if (bParseSerialInThread) {
		thread = std::thread(&ofxEETI::threadFunction, this);
	}
	
	bRunning = true;
}

void ofxEETI::stop(bool wait)
{
	if (!bRunning) {
		return;
	}

	bRunning = false;
	if (bParseSerialInThread) {
		if (wait) {
			thread.join();
		}
	}
	ofRemoveListener(ofEvents().update, this, &ofxEETI::update, OF_EVENT_ORDER_AFTER_APP);
}

bool ofxEETI::initEETI()
{
	usleep(200000);
	serial.flush();
	
	unsigned char buff[1024];	// should be more than enough
	
	// send alive message
	serial.writeBytes(eeti_alive, 3);
	// wait for response
	if (waitForResponse(3, 500) < 3) {
		return false;
	}
	
	int count = serial.available();
	serial.readBytes(buff, count);
#ifdef DEBUG_SERIAL
	printf("eeti_alive response: ");
	printBuffer(buff, count);
#endif
	if (buff[0] != eeti_alive[0] ||
		buff[1] != eeti_alive[1] ||
		buff[2] != eeti_alive[2]) {
		return false;
	}
	ofLogNotice("ofxEETI") << "Found EETI eGalax touch panel";
	
	// get firmware version
	serial.writeBytes(eeti_fwver, 3);
	if (waitForResponse(3, 500) < 3) {
		return false;
	}
	
	usleep(50000);
	count = serial.available();
	serial.readBytes(buff, count);
#ifdef DEBUG_SERIAL
	printf("eeti_fwver response: ");
	printBuffer(buff, count);
#endif
	stringstream fwver;
	for (int i=0; i<(int)buff[1]-1; i++) {
		fwver << (char)buff[3+i];
	}
	ofLogNotice("ofxEETI") << "Firmware version: "<<fwver.str();

	
	// get controller type
	serial.writeBytes(eeti_ctrlr, 3);
	if (waitForResponse(3, 500) < 3) {
		return false;
	}
	
	usleep(50000);
	count = serial.available();
	serial.readBytes(buff, count);
#ifdef DEBUG_SERIAL
	printf("eeti_ctrlr response: ");
	printBuffer(buff, count);
#endif
	stringstream ctrlr;
	for (int i=0; i<(int)buff[1]-1; i++) {
		ctrlr << (char)buff[3+i];
	}
	ofLogNotice("ofxEETI") << "Controller type: "<<ctrlr.str();
	
	bInitialized = true;
	return true;
}

int ofxEETI::waitForResponse(int nBytes, unsigned int timeout_ms)
{
	int timeout=timeout_ms;
	int bytes;
	while ((bytes = serial.available())<nBytes && timeout-->0) {
		usleep(1000);
	}
	
	if (timeout==0) {
		ofLogError("ofxEETI") << "timeout waiting for touch panel response";
		return bytes;
	}
	
	return bytes;
}

void ofxEETI::threadFunction()
{
	unsigned char buff[1024];
	
	while (bRunning) {
		while (serial.available() >= 6) {
			int count = serial.readBytes(buff, 6);
#ifdef DEBUG_SERIAL
			printBuffer(buff, count);
#endif
			parsePacket(buff);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
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

void ofxEETI::update(ofEventArgs &args)
{
	if (!bParseSerialInThread) {
		// read serial
		unsigned char buff[1024];
		while (serial.available() >= 6) {
			int count = serial.readBytes(buff, 6);
	#ifdef DEBUG_SERIAL
			printBuffer(buff, count);
	#endif
			parsePacket(buff);
		}
	}
	// send events
	touchMutex.lock();
	vector<Touch> sendEvents = events;
	events.clear();
	touchMutex.unlock();
	
	for (Touch& event: sendEvents) {
		ofNotifyEvent(eventTouch, event, this);
	}
}

void ofxEETI::addEvent(const Touch& touch)
{
	touchMutex.lock();
	events.push_back(touch);
	touchMutex.unlock();
}

void ofxEETI::printBuffer(unsigned char* buffer, int count)
{
	for (int i=0; i<count; i++) {
		printf("0x%0x ", buffer[i]);
	}
	printf("\n");
}
