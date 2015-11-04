#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){
	ofClear(0);
	ofFill();
	ofSetFrameRate(60);
	ofSetDrawBitmapMode(OF_BITMAPMODE_SIMPLE);
	bFullScreen = false;
	fbo.allocate(2160, 3840, GL_RGBA);
	fbo.begin();
	ofClear(0, 255);
	fbo.end();

	
	
	// setup the touch panel
	touchPanel.setup("/dev/cu.usbserial", 19200);	// open touch device
	touchPanel.useCalibration("touch_calib.json");	// use calibration file
	
	// add listener to get touch events
	ofAddListener(touchPanel.eventTouch, this, &ofApp::onEETITouchEvent);
	
	// call this to start receiving events
	touchPanel.start();
}

//--------------------------------------------------------------
void ofApp::update(){
}

//--------------------------------------------------------------
void ofApp::draw(){
	ofSetColor(255);
	fbo.draw(0, 0, ofGetWidth(), ofGetHeight());
	
	ofDrawBitmapString("'RETURN': clear\n'f': fullscreen toggle\n'c': calibration mode", 10, 18);
	
	ofScale(6, 6);
	ofDrawBitmapString("Touches: "+ofToString(touchPanel.numTouches), 30/6, (ofGetHeight()-30)/6);
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
	if (key == OF_KEY_RETURN) {
		printf("==============================\n");
		fbo.begin();
		ofClear(0, 255);
		fbo.end();
	}
	else if (key == 'f') {
		bFullScreen = !bFullScreen;
		ofSetFullscreen(bFullScreen);
		if (!bFullScreen) {
			ofSetWindowShape(1024, 768);
		}
	}
	else if (key == 'c') {
		// enter calibration mode
		touchPanel.startCalibration("touch_calib.json", fbo.getWidth(), fbo.getHeight());
	}
}

//--------------------------------------------------------------
void ofApp::onEETITouchEvent(ofxEETI::Touch &args)
{
	fbo.begin();
	ofFill();
	
	ofSetColor(ofColor::fromHsb((args.id*30)%255, 255, 255));
	if (args.type == ofTouchEventArgs::down) {
		ofDrawCircle(args.x, args.y, 20);
	}
	else if (args.type == ofTouchEventArgs::move) {
		ofDrawCircle(args.x, args.y, 5);
	}
	else if (args.type == ofTouchEventArgs::up) {
		ofDrawCircle(args.x, args.y, 10);
	}
	
	fbo.end();
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
