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

#define HAVE_OFXJSON

#if defined(HAVE_OFXJSON)
  #include "ofxJSON.h"
#endif

#define MAX_TOUCH 10

class ofxEETI
{
public:

	~ofxEETI();
	ofxEETI();

	bool setup(const string& devname, int baudrate, bool parseSerialInThread=true);
	void start();
	void stop(bool wait=false);
	
	bool useCalibration(const string& filename);
	void startCalibration(const string& saveToFilename, int width, int height);
	void abortCalibration();

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
	
	// for calibration
	bool bUseCalibration;
	string calibrationFilename;
	int screenWidth;
	int screenHeight;
	bool bInCalibration;
	int calibrationPointIndex;
	ofVec2f calibrationPoint;
	ofMatrix3x3 calibrationMatrix;
#if defined(HAVE_OFXJSON)
	ofxJSONElement calibrationJson;
#endif
	

	bool initEETI();
	int waitForResponse(int nBytes, unsigned int timeout_ms);	// returns number of bytes available
	void threadFunction();
	void parsePacket(const unsigned char* buff);
	void update(ofEventArgs& args);
	void addEvent(const Touch& touch);

	void drawCalibration(ofEventArgs& args);
	void drawCross(const ofVec2f& p);
	void handleCalibrationTouch(Touch& touch);
	bool calcCalibrationCoeffs();
	void cacheCalibrationMatrix();
	float getScreenPointX(float x, float y);
	float getScreenPointY(float x, float y);
	
	// for debug
	void printBuffer(unsigned char* buffer, int count);
};

#endif /* defined(__ofxEETI__ofxEETI__) */
