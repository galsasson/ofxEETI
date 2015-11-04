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
	bInCalibration = false;
	bUseCalibration = false;
	numTouches = 0;
	
	for (int i=0; i<MAX_TOUCH; i++) {
		touches[i].bDown = false;
		touches[i].type = ofTouchEventArgs::cancel;
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

bool ofxEETI::useCalibration(const string &filename)
{
#if defined(HAVE_OFXJSON)
	if (!calibrationJson.open(filename)) {
		return false;
	}
	
	cacheCalibrationMatrix();
	bUseCalibration = true;
	return true;
#else
	ofLogError("ofxEETI") << "calibration disabled, #define HAVE_OFXJSON in the head of ofxEETI.h (or in project settings) to enable";
	return false;
#endif
}

void ofxEETI::startCalibration(const string& filename, int width, int height)
{
#if defined(HAVE_OFXJSON)
	if (!bInitialized ||
		!bRunning ||
		bInCalibration) {
		return;
	}
	
	calibrationFilename = filename;
	screenWidth = width;
	screenHeight = height;
	bInCalibration = true;
	bUseCalibration = false;
	calibrationPointIndex = 0;

	calibrationJson.clear();
	calibrationJson["screen_points"][0]["x"] = 30;
	calibrationJson["screen_points"][0]["y"] = 30;
	calibrationJson["screen_points"][1]["x"] = screenWidth-30;
	calibrationJson["screen_points"][1]["y"] = 30;
	calibrationJson["screen_points"][2]["x"] = screenWidth-30;
	calibrationJson["screen_points"][2]["y"] = screenHeight-30;
	calibrationJson["screen_points"][3]["x"] = 30;
	calibrationJson["screen_points"][3]["y"] = screenHeight-30;

	ofAddListener(ofEvents().draw, this, &ofxEETI::drawCalibration, OF_EVENT_ORDER_AFTER_APP);
#else
	ofLogError("ofxEETI") << "calibration disabled, #define HAVE_OFXJSON in the head of ofxEETI.h (or in project settings) to enable";
	return false;
#endif
}

void ofxEETI::abortCalibration()
{
	if (!bInCalibration) {
		return;
	}
	
	bInCalibration = false;
	ofRemoveListener(ofEvents().draw, this, &ofxEETI::drawCalibration, OF_EVENT_ORDER_AFTER_APP);
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
	int tx = buff[1]<<7 | buff[2];
	int ty = buff[3]<<7 | buff[4];
	bool bDown = buff[0]&0x1;
	
	int x = tx;
	int y = ty;
	if (bUseCalibration) {
		x = getScreenPointX(tx, ty);
		y = getScreenPointY(tx, ty);
	}

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
		if (event.type == ofTouchEventArgs::down) {
			numTouches++;
		}
		else if (event.type == ofTouchEventArgs::up) {
			numTouches--;
		}
		
		ofNotifyEvent(eventTouch, event, this);
		
		if (bInCalibration) {
			handleCalibrationTouch(event);
		}
	}
}

void ofxEETI::addEvent(const Touch& touch)
{
	touchMutex.lock();
	events.push_back(touch);
	touchMutex.unlock();
}

void ofxEETI::drawCalibration(ofEventArgs& args)
{
	ofSetColor(0, 128);
	ofFill();
	ofDrawRectangle(0, 0, screenWidth, screenHeight);
	ofSetColor(255);
	ofNoFill();
	ofSetLineWidth(1);
	ofDrawRectangle(0, 0, screenWidth, screenHeight);
	
	switch (calibrationPointIndex) {
		case 0:
			drawCross(ofVec2f(30, 30));
			break;
		case 1:
			drawCross(ofVec2f(screenWidth-30, 30));
			break;
		case 2:
			drawCross(ofVec2f(screenWidth-30, screenHeight-30));
			break;
		case 3:
			drawCross(ofVec2f(30, screenHeight-30));
			break;
	}
}

void ofxEETI::handleCalibrationTouch(Touch& touch)
{
#if defined(HAVE_OFXJSON)
	if (touch.id != 0) {
		return;
	}
	
	if (touch.type == ofTouchEventArgs::down) {
		calibrationPoint = ofVec2f(touch.x, touch.y);
	}
	else if (touch.type == ofTouchEventArgs::move) {
		calibrationPoint = ofVec2f(touch.x, touch.y).interpolate(calibrationPoint, 0.5);
	}
	else if (touch.type == ofTouchEventArgs::up) {
		// save point in calibration
		calibrationJson["touch_points"][calibrationPointIndex]["x"] = calibrationPoint.x;
		calibrationJson["touch_points"][calibrationPointIndex]["y"] = calibrationPoint.y;
		calibrationPointIndex++;
		if (calibrationPointIndex>3) {
			if (calcCalibrationCoeffs()) {
				calibrationJson.save(calibrationFilename, true);
				cacheCalibrationMatrix();
				bUseCalibration = true;
			}
			else {
				ofLogError("ofxEETI") << "error calculating matrix coefficients";
			}
			// done with calibration
			ofLogNotice("ofxEETI") << "calibration completed and saved to: "<<calibrationFilename;
			abortCalibration();
		}
	}
#else
	ofLogError("ofxEETI") << "calibration disabled, #define HAVE_OFXJSON in the head of ofxEETI.h (or in project settings) to enable";
	return;
#endif
}

bool ofxEETI::calcCalibrationCoeffs()
{
#if defined(HAVE_OFXJSON)
	ofVec2f screenPoints[3];
	ofVec2f touchPoints[3];
	for (int i=0; i<3; i++) {
		screenPoints[i] = ofVec2f(calibrationJson["screen_points"][i]["x"].asFloat(),
								  calibrationJson["screen_points"][i]["y"].asFloat());
		touchPoints[i] = ofVec2f(calibrationJson["touch_points"][i]["x"].asFloat(),
								  calibrationJson["touch_points"][i]["y"].asFloat());
	}
	
	calibrationJson["matrix"]["divider"] = 
		((touchPoints[0].x-touchPoints[2].x) * (touchPoints[1].y-touchPoints[2].y)) -
		((touchPoints[1].x-touchPoints[2].x) * (touchPoints[0].y-touchPoints[2].y));
		
		if( calibrationJson["matrix"]["divider"].asFloat() == 0 )
		{
			return false;
		}
		else
		{
			calibrationJson["matrix"]["An"] = ((screenPoints[0].x - screenPoints[2].x) * (touchPoints[1].y - touchPoints[2].y)) -
			((screenPoints[1].x - screenPoints[2].x) * (touchPoints[0].y - touchPoints[2].y)) ;
			
			calibrationJson["matrix"]["Bn"] = ((touchPoints[0].x - touchPoints[2].x) * (screenPoints[1].x - screenPoints[2].x)) -
			((screenPoints[0].x - screenPoints[2].x) * (touchPoints[1].x - touchPoints[2].x)) ;
			
			calibrationJson["matrix"]["Cn"] = (touchPoints[2].x * screenPoints[1].x - touchPoints[1].x * screenPoints[2].x) * touchPoints[0].y +
			(touchPoints[0].x * screenPoints[2].x - touchPoints[2].x * screenPoints[0].x) * touchPoints[1].y +
			(touchPoints[1].x * screenPoints[0].x - touchPoints[0].x * screenPoints[1].x) * touchPoints[2].y ;
			
			calibrationJson["matrix"]["Dn"] = ((screenPoints[0].y - screenPoints[2].y) * (touchPoints[1].y - touchPoints[2].y)) -
			((screenPoints[1].y - screenPoints[2].y) * (touchPoints[0].y - touchPoints[2].y)) ;
			
			calibrationJson["matrix"]["En"] = ((touchPoints[0].x - touchPoints[2].x) * (screenPoints[1].y - screenPoints[2].y)) -
			((screenPoints[0].y - screenPoints[2].y) * (touchPoints[1].x - touchPoints[2].x)) ;
			
			calibrationJson["matrix"]["Fn"] = (touchPoints[2].x * screenPoints[1].y - touchPoints[1].x * screenPoints[2].y) * touchPoints[0].y +
			(touchPoints[0].x * screenPoints[2].y - touchPoints[2].x * screenPoints[0].y) * touchPoints[1].y +
			(touchPoints[1].x * screenPoints[0].y - touchPoints[0].x * screenPoints[1].y) * touchPoints[2].y ;
		}
		
		return true;
#else
	ofLogError("ofxEETI") << "calibration disabled, #define HAVE_OFXJSON in the head of ofxEETI.h (or in project settings) to enable";
	return false;
#endif
}

void ofxEETI::cacheCalibrationMatrix()
{
#if defined(HAVE_OFXJSON)
	calibrationMatrix.a = calibrationJson["matrix"]["An"].asFloat();
	calibrationMatrix.b = calibrationJson["matrix"]["Bn"].asFloat();
	calibrationMatrix.c = calibrationJson["matrix"]["Cn"].asFloat();
	calibrationMatrix.d = calibrationJson["matrix"]["Dn"].asFloat();
	calibrationMatrix.e = calibrationJson["matrix"]["En"].asFloat();
	calibrationMatrix.f = calibrationJson["matrix"]["Fn"].asFloat();
	calibrationMatrix.i = calibrationJson["matrix"]["divider"].asFloat();
#else
	ofLogError("ofxEETI") << "calibration disabled, #define HAVE_OFXJSON in the head of ofxEETI.h (or in project settings) to enable";
	return;
#endif
}

float ofxEETI::getScreenPointX(float x, float y)
{
	if (calibrationMatrix.i == 0) {
		return 0;
	}

	return ((calibrationMatrix.a * x) + (calibrationMatrix.b * y) + calibrationMatrix.c) / calibrationMatrix.i;
}

float ofxEETI::getScreenPointY(float x, float y)
{
	if (calibrationMatrix.i == 0) {
		return 0;
	}

	return ((calibrationMatrix.d * x) + (calibrationMatrix.e * y) + calibrationMatrix.f) / calibrationMatrix.i;
}

void ofxEETI::drawCross(const ofVec2f& p)
{
	ofDrawArrow(p, p+ofVec2f(-15, 0));
	ofDrawArrow(p, p+ofVec2f(15, 0));
	ofDrawArrow(p, p+ofVec2f(0, -15));
	ofDrawArrow(p, p+ofVec2f(0, 15));
}

void ofxEETI::printBuffer(unsigned char* buffer, int count)
{
	for (int i=0; i<count; i++) {
		printf("0x%0x ", buffer[i]);
	}
	printf("\n");
}
