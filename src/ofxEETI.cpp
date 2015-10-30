//
//  ofxEETI.cpp
//  ofxEETI
//
//  Created by Gal Sasson on 10/30/15.
//
//

#include "ofxEETI.h"

//#define DEBUG_SERIAL

ofxEETI::~ofxEETI()
{
	stop(true);
	if (serial.isInitialized()) {
		serial.close();
	}
}

ofxEETI::ofxEETI()
{
	bThreadRunning = false;
}

bool ofxEETI::setup(const string &devname, int baudrate)
{
	ofLogNotice("ofxEETI") << "openning touch device: " << devname << "@"<<baudrate<<"bps";
	bool success = serial.setup(devname, baudrate);
	if (success) {
		ofLogNotice("ofxEETI") << "device opened successfully";
	}
	else {
		ofLogError("ofxETTI") << "error openning device";
	}
	return success;
}


void ofxEETI::start()
{
	if (bThreadRunning) {
		ofLogWarning("ofxEETI") << "start called while thread already running";
		return;
	}

	if (!serial.isInitialized()) {
		ofLogError("ofxEETI") << "start called before setup (serial is not initialized)";
		return;
	}

	bThreadRunning = true;
	thread = std::thread(&ofxEETI::threadFunction, this);

	ofAddListener(ofEvents().update, this, &ofxEETI::update);
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

	ofRemoveListener(ofEvents().update, this, &ofxEETI::update);
}

void ofxEETI::threadFunction()
{
	unsigned char* buff = (unsigned char*)malloc(6);

	while (bThreadRunning) {
		if (serial.available() >= 6) {
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
	int id = buff[5];
	int x = buff[1]<<7 | buff[2];
	int y = buff[3]<<7 | buff[4];
	bool bDown = buff[0]&0x1;

	map<int, ofxEETITouch*>::iterator it = touches.find(id);
	if (bDown) {
		if (it == touches.end()) {
			// this is a new touch (touch down)
			ofxEETITouch* touch = new ofxEETITouch();
			touch->id = id;
			touch->x = x;
			touch->y = y;
			touches[id] = touch;

			// add an event for touch down
			addEvent(ofTouchEventArgs::down, id, x, y);
		}
		else {
			// touch already exist (touch move)
			if (it->second->x == x &&
				it->second->y == y) {
				// same place, do nothing
				return;
			}
			else {
				it->second->x = x;
				it->second->y = y;
				addEvent(ofTouchEventArgs::move, id, x, y);
			}
		}
	}
	else {
		if (it == touches.end()) {
			// touch up for touch we don't have
			ofLogWarning("ofxEETI") << "warning: got touch up for touch we don't have";
			return;
		}
		else {
			// delete this touch
//			delete it->second;
			touches.erase(it, it);
			addEvent(ofTouchEventArgs::up, id, x, y);
		}
	}
}

void ofxEETI::addEvent(ofTouchEventArgs::Type type, int id, float x, float y)
{
	ofTouchEventArgs* event = new ofTouchEventArgs();
	event->type == type;
	event->id = id;
	event->x = x;
	event->y = y;

	eventsMutex.lock();
	pendingEvents.push_back(event);
	eventsMutex.unlock();
}

void ofxEETI::update(ofEventArgs &args)
{
	eventsMutex.lock();
	vector<ofTouchEventArgs*> events = pendingEvents;
	pendingEvents.clear();
	eventsMutex.unlock();

	for (ofTouchEventArgs* event: events) {
		ofNotifyEvent(eventTouch, *event, this);
		delete event;
	}
}

