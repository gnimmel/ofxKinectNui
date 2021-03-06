/******************************************************************/
/**
 * @file	ofxKinectNui.cpp
 * @brief	addon of openFrameworks for Kinect Official Sensor
 * @note
 * @todo
 * @bug	
 * @reference	ofxKinectNuiPlayer.cpp created by arturo: 14/12/2010
 *
 * @author	sadmb
 * @date	Oct. 18, 2011
 */
/******************************************************************/
#include "ofxKinectNuiPlayer.h"

//---------------------------------------------------------------------------
ofxKinectNuiPlayer::ofxKinectNuiPlayer(){
	f = NULL;
	filename = "";
	skeletons = NULL;
	skeletonPoints = NULL;
	fps = 30;

	bPlay = false;
	bLoop = false;
	bVideo = false;
	bDepth = false;
	bLabel = false;
	bSkeleton = false;
	bUsesTexture = false;
	ofxBase3DVideo::initLookups();
}

//---------------------------------------------------------------------------
ofxKinectNuiPlayer::~ofxKinectNuiPlayer() {
	close();

	if(videoTexture.isAllocated()){
		videoTexture.clear();
	}
	if(depthTexture.isAllocated()){
		depthTexture.clear();
	}
	if(labelTexture.isAllocated()){
		labelTexture.clear();
	}

	if(videoPixels.isAllocated()){
		videoPixels.clear();
	}
	if(distancePixels.isAllocated()){
		distancePixels.clear();
	}
	if(depthPixels.isAllocated()){
		depthPixels.clear();
	}
	if(labelPixels.isAllocated()){
		labelPixels.clear();
	}
	if(calibratedVideoPixels.isAllocated()){
		calibratedVideoPixels.clear();
	}

	if(skeletonPoints != NULL){
		delete[] skeletonPoints[0];
		skeletonPoints[0] = NULL;
		delete[] skeletonPoints;
		skeletonPoints = NULL;
	}
	if(skeletons != NULL){
		delete[] skeletons;
		skeletons = NULL;

	}
	bPlay = false;
	bLoop = false;
	bVideo = false;
	bDepth = false;
	bLabel = false;
	bSkeleton = false;
	bUsesTexture = false;
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::setUsesTexture(bool bUse){
	bUsesTexture = bUse;
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::setup(	const string & file, bool useTexture /*= true*/){
	bUsesTexture = useTexture;
	f = fopen(ofToDataPath(file).c_str(), "rb");
	filename = file;

	unsigned char dst = 0;
	fread(&dst, sizeof(char), 1, f);

	bVideo = (bool)((dst >> 3) & 0x7);
	bDepth = (bool)((dst >> 2) & 0x7);
	bLabel = (bool)((dst >> 1) & 0x7);
	bSkeleton = (bool)(dst & 0x7);
	

	NUI_IMAGE_RESOLUTION videoResolution;
	NUI_IMAGE_RESOLUTION depthResolution;

	fread(&videoResolution, sizeof(int), 1, f);
	fread(&depthResolution, sizeof(int), 1, f);
	
	fread(&nearClippingDistance, sizeof(unsigned short), 1, f);
	fread(&farClippingDistance, sizeof(unsigned short), 1, f);
	ofxBase3DVideo::initLookups(nearClippingDistance, farClippingDistance);

	switch(videoResolution){
	case NUI_IMAGE_RESOLUTION_1280x1024:
		width = 1280;
		height = 1024;
		break;
	case NUI_IMAGE_RESOLUTION_640x480:
		width = 640;
		height = 480;
		break;
	case NUI_IMAGE_RESOLUTION_320x240:
	case NUI_IMAGE_RESOLUTION_80x60:
	default:
		string error = "Invalid video resolution: select 1280x1024 or 640x480.";
		ofLog(OF_LOG_ERROR, "ofxKinectNuiPlayer: " + error);
		return;
	}
	switch(depthResolution){
	case NUI_IMAGE_RESOLUTION_640x480:
		depthWidth = 640;
		depthHeight = 480;
		break;
	case NUI_IMAGE_RESOLUTION_320x240:
		depthWidth = 320;
		depthHeight = 240;
		break;
	case NUI_IMAGE_RESOLUTION_80x60:
		depthWidth = 80;
		depthHeight = 60;
		break;
	case NUI_IMAGE_RESOLUTION_1280x1024:
	default:
		string error = "Invalid depth resolution: select 320x240, 80x60 or you must disable grabLabel when you select 640x480.";
		ofLog(OF_LOG_ERROR, "ofxKinectNuiPlayer: " + error);
		return;
	}
	
	ofxBase3DVideo::initLookups();

	if(bVideo){
		if(!videoPixels.isAllocated()){
			videoPixels.allocate(width, height, OF_PIXELS_RGB);
		}
		memset(videoPixels.getPixels(), 0, width * height * 3);
	}
	if(bDepth){
		if(!distancePixels.isAllocated()){
			distancePixels.allocate(depthWidth, depthHeight, OF_PIXELS_MONO);
		}
		memset(distancePixels.getPixels(), 0, depthWidth * depthHeight);

		if(!depthPixels.isAllocated()){
			depthPixels.allocate(depthWidth, depthHeight, OF_PIXELS_MONO);
		}
		memset(depthPixels.getPixels(), 0, depthWidth * depthHeight);
	}
	if(bLabel){
		if(!labelPixels.isAllocated()){
			labelPixels.allocate(depthWidth, depthHeight, OF_PIXELS_RGBA);
		}
		memset(labelPixels.getPixels(), 0, depthWidth * depthHeight * 4);
	}
	if(bVideo && bDepth){
		if(!calibratedVideoPixels.isAllocated()){
			calibratedVideoPixels.allocate(depthWidth, depthHeight, OF_PIXELS_RGB);
		}
		memset(calibratedVideoPixels.getPixels(), 0, depthWidth * depthHeight * 3);
	}

	if(!videoTexture.isAllocated() && bUsesTexture && bVideo){
		videoTexture.allocate(width, height, GL_RGB);
	}
	if(!depthTexture.isAllocated() && bUsesTexture && bDepth){
		depthTexture.allocate(depthWidth, depthHeight, GL_LUMINANCE);
	}
	if(!labelTexture.isAllocated() && bUsesTexture && bLabel){
		labelTexture.allocate(depthWidth, depthHeight, GL_RGBA);
	}

	if(bSkeleton){
		if(skeletons == NULL){
			skeletons = new float[kinect::nui::SkeletonFrame::SKELETON_COUNT * kinect::nui::SkeletonData::POSITION_COUNT * 3];
		}
		for(int i = 0; i < kinect::nui::SkeletonFrame::SKELETON_COUNT * kinect::nui::SkeletonData::POSITION_COUNT * 3; i++){
			skeletons[i] = -1.0f;
		}

		if(skeletonPoints == NULL){
			skeletonPoints = new ofPoint*[kinect::nui::SkeletonFrame::SKELETON_COUNT];
			skeletonPoints[0] = new ofPoint[kinect::nui::SkeletonFrame::SKELETON_COUNT * kinect::nui::SkeletonData::POSITION_COUNT];
			for(int i = 0; i < kinect::nui::SkeletonFrame::SKELETON_COUNT; i++){
				skeletonPoints[i] = skeletonPoints[0] + i * kinect::nui::SkeletonData::POSITION_COUNT;
			}
		}
	}
	lastFrameTime = ofGetElapsedTimeMillis();

}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::close(){
	if(!f){
		return;
	}
	fclose(f);
	f = NULL;
	filename = "";

	bUsesTexture = false;
	bPlay = false;
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::play(){
	bPlay = true;
	lastFrameTime = ofGetElapsedTimeMillis();
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::stop(){
	bPlay = false;
	f = fopen(ofToDataPath(filename).c_str(), "rb");
	unsigned char dst = 0;
	fread(&dst, sizeof(char), 1, f);
	int dst2 = 0;
	fread(&dst2, sizeof(int), 1, f);
	fread(&dst2, sizeof(int), 1, f);
	unsigned short bit3 = 0;
	fread(&bit3, sizeof(unsigned short), 1, f);
	fread(&bit3, sizeof(unsigned short), 1, f);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::pause(){
	bPlay = false;
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::update(){
	if(!f || !bPlay){
		return;
	}

	int currentFrameTime = ofGetElapsedTimeMillis();
	if((currentFrameTime-lastFrameTime)<(1000./fps)){
		return;
	}
	lastFrameTime = currentFrameTime;

	fread(&fps, sizeof(float), 1, f);
	if(bVideo){
		fread(videoPixels.getPixels(), sizeof(unsigned char), width * height * 3, f);
	}
	if(bDepth){
		fread(distancePixels.getPixels(), sizeof(unsigned short), depthWidth * depthHeight, f);
	}
	if(bVideo && bDepth){
		fread(calibratedVideoPixels.getPixels(), sizeof(unsigned char), depthWidth * depthHeight * 3, f);
	}
	if(bLabel){
		fread(labelPixels.getPixels(), sizeof(unsigned char), depthWidth * depthHeight * 4, f);
	}
	if(bSkeleton){
		fread(skeletons, sizeof(float), kinect::nui::SkeletonFrame::SKELETON_COUNT * kinect::nui::SkeletonData::POSITION_COUNT * 3, f);
		for(int j = 0; j < kinect::nui::SkeletonData::POSITION_COUNT; j++){
			for(int i = 0; i < kinect::nui::SkeletonFrame::SKELETON_COUNT; i++){
				skeletonPoints[i][j].x = skeletons[(i * kinect::nui::SkeletonData::POSITION_COUNT + j) * 3];
				skeletonPoints[i][j].y = skeletons[(i * kinect::nui::SkeletonData::POSITION_COUNT + j) * 3 + 1];
				skeletonPoints[i][j].z = skeletons[(i * kinect::nui::SkeletonData::POSITION_COUNT + j) * 3 + 2];
			}
		}
	}

	int n = depthWidth * depthHeight;
	if(bIsDepthNearValueWhite){
		for(int i = 0; i < n; i++){
			depthPixels[i] = depthPixelsLookupNearWhite[distancePixels[i]];
		}
	}else{
		for(int i = 0; i < n; i++){
			depthPixels[i] = depthPixelsLookupFarWhite[distancePixels[i]];
		}
	}

	if(bUsesTexture){
		if(bVideo){
			videoTexture.loadData(videoPixels);
		}
		if(bDepth){
			depthTexture.loadData(getDepthPixels());
		}
		if(bLabel){
			labelTexture.loadData(labelPixels);
		}
	}
	bIsFrameNew = true;

	// loop?
	if(bLoop && std::feof(f) > 0) {
		f = fopen(ofToDataPath(filename).c_str(), "rb");
		unsigned char dst = 0;
		fread(&dst, sizeof(char), 1, f);
		int dst2 = 0;
		fread(&dst2, sizeof(int), 1, f);
		fread(&dst2, sizeof(int), 1, f);
		unsigned short bit3 = 0;
		fread(&bit3, sizeof(unsigned short), 1, f);
		fread(&bit3, sizeof(unsigned short), 1, f);
	}
	
}


//---------------------------------------------------------------------------
bool ofxKinectNuiPlayer::isFrameNew(){
	return bIsFrameNew;
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::draw(float x, float y, float w, float h){
	if(bUsesTexture && bVideo) {
		videoTexture.draw(x, y, w, h);
	}else{
		ofLog(OF_LOG_WARNING, "ofxKinectNuiPlayer: You should set UsesTexture and Video true");
	}
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::draw(float x, float y){
	draw(x, y, width, height);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::draw(const ofPoint & point){
	draw(point.x, point.y);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::draw(const ofPoint & point, float w, float h){
	draw(point.x, point.y, w, h);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::draw(const ofRectangle & rect){
	draw(rect.x, rect.y, rect.width, rect.height);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawDepth(float x, float y, float w, float h){
	if(bUsesTexture && bDepth) {
		depthTexture.draw(x, y, w, h);
	}else{
		ofLog(OF_LOG_WARNING, "ofxKinectNuiPlayer: You should set UsesTexture and Depth true");
	}
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawDepth(float x, float y){
	depthTexture.draw(x, y, depthWidth, depthHeight);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawDepth(const ofPoint & point){
	drawDepth(point.x, point.y);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawDepth(const ofPoint & point, float w, float h){
	drawDepth(point.x, point.y, w, h);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawDepth(const ofRectangle & rect){
	drawDepth(rect.x, rect.y, rect.width, rect.height);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawSkeleton(float x, float y, float w, float h){
	if(bSkeleton){
		for(int i = 0; i < kinect::nui::SkeletonFrame::SKELETON_COUNT; i++){
			if((int)skeletonPoints[i][0].x < 0 && (int)skeletonPoints[i][0].y < 0 && (int)skeletonPoints[i][0].z < 0){
				continue;
			}
			ofPushMatrix();
			ofTranslate(x, y);
			ofPushStyle();
			ofSetColor(255 * (int)pow(-1.0, i + 1), 255 * (int)pow(-1.0, i), 255 * (int)pow(-1.0, i + 1));
			ofNoFill();
			ofSetLineWidth(4);

			// HEAD
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HIP_CENTER], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SPINE], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SPINE], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_CENTER], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_CENTER], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HEAD], w, h));
		
			// BODY_LEFT
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_CENTER], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_LEFT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_LEFT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ELBOW_LEFT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ELBOW_LEFT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_WRIST_LEFT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_WRIST_LEFT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HAND_LEFT], w, h));

			// BODY_RIGHT
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_CENTER], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_RIGHT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_SHOULDER_RIGHT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ELBOW_RIGHT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ELBOW_RIGHT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_WRIST_RIGHT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_WRIST_RIGHT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HAND_RIGHT], w, h));
		
			// LEG_LEFT
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HIP_CENTER], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HIP_LEFT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HIP_LEFT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_KNEE_LEFT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_KNEE_LEFT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ANKLE_LEFT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ANKLE_LEFT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_FOOT_LEFT], w, h));

			// LEG_RIGHT
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HIP_CENTER], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HIP_RIGHT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_HIP_RIGHT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_KNEE_RIGHT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_KNEE_RIGHT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ANKLE_RIGHT], w, h));
			ofLine(calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_ANKLE_RIGHT], w, h), calcScaledSkeletonPoint(skeletonPoints[i][NUI_SKELETON_POSITION_FOOT_RIGHT], w, h));
		
		
			ofSetColor(255 * (int)pow(-1.0, i + 1), 255 * (int)pow(-1.0, i + 1), 255 * (int)pow(-1.0, i));
			ofSetLineWidth(0);
			ofFill();
			for(int j = 0; j < kinect::nui::SkeletonData::POSITION_COUNT; j++){
				ofCircle(calcScaledSkeletonPoint(skeletonPoints[i][j], w, h), 5);
			}
			ofPopStyle();
			ofPopMatrix();
		}
	}else{
		ofLog(OF_LOG_WARNING, "ofxKinectNuiPlayer: You should set Skeleton true");
	}
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawSkeleton(float x, float y){
	drawSkeleton(x, y, width, height);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawSkeleton(const ofPoint & point){
	drawSkeleton(point.x, point.y);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawSkeleton(const ofPoint & point, float w, float h){
	drawSkeleton(point.x, point.y, w, h);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawSkeleton(const ofRectangle & rect){
	drawSkeleton(rect.x, rect.y, rect.width, rect.height);
}


//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawLabel(float x, float y, float w, float h){
	if(bUsesTexture && bLabel) {
		labelTexture.draw(x, y, w, h);
	}else{
		ofLog(OF_LOG_WARNING, "ofxKinectNuiPlayer: You should set UsesTexture and Label true");
	}
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawLabel(float x, float y){
	drawLabel(x, y, width, height);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawLabel(const ofPoint & point){
	drawLabel(point.x, point.y);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawLabel(const ofPoint & point, float w, float h){
	drawLabel(point.x, point.y, w, h);
}

//---------------------------------------------------------------------------
void ofxKinectNuiPlayer::drawLabel(const ofRectangle & rect){
	drawLabel(rect.x, rect.y, rect.width, rect.height);
}

//---------------------------------------------------------------------------
ofPixels& ofxKinectNuiPlayer::getVideoPixels(){
	return videoPixels;
}

//---------------------------------------------------------------------------
ofPixels& ofxKinectNuiPlayer::getDepthPixels(){
	return depthPixels;
}

//---------------------------------------------------------------------------
ofPixels& ofxKinectNuiPlayer::getLabelPixels(){
	return labelPixels;
}

//---------------------------------------------------------------------------
ofShortPixels& ofxKinectNuiPlayer::getDistancePixels(){
	return distancePixels;
}

//---------------------------------------------------------------------------
ofPixels& ofxKinectNuiPlayer::getCalibratedVideoPixels(){
	return calibratedVideoPixels;
}

//---------------------------------------------------------------------------
ofTexture& ofxKinectNuiPlayer::getVideoTextureReference(){
	return videoTexture;
}

//---------------------------------------------------------------------------
ofTexture & ofxKinectNuiPlayer::getDepthTextureReference(){
	return depthTexture;
}

//---------------------------------------------------------------------------
ofTexture & ofxKinectNuiPlayer::getLabelTextureReference(){
	return labelTexture;
}

//---------------------------------------------------------------------------
float ofxKinectNuiPlayer::getWidth(){
	return (float)width;
}

//---------------------------------------------------------------------------
float ofxKinectNuiPlayer::getHeight(){
	return (float)height;
}

//---------------------------------------------------------------------------
float ofxKinectNuiPlayer::getDepthWidth(){
	return (float)depthWidth;
}

//---------------------------------------------------------------------------
float ofxKinectNuiPlayer::getDepthHeight(){
	return (float)depthHeight;
}

//---------------------------------------------------------------------------
float ofxKinectNuiPlayer::getDistanceAt(int x, int y) {
	return distancePixels[y * depthWidth + x];
}

//---------------------------------------------------------------------------
float ofxKinectNuiPlayer::getDistanceAt(ofPoint p) {
	return getDistanceAt((int)p.x, (int)p.y);
}

//---------------------------------------------------------------------------
ofVec3f ofxKinectNuiPlayer::getWorldCoordinateFor(int depthX, int depthY) {
	const double depthZ = distancePixels[depthWidth * depthX + depthY]/1000.0;
	return ofxBase3DVideo::getWorldCoordinateFor(depthX, depthY, depthZ);
}

//---------------------------------------------------------------------------
ofVec3f ofxKinectNuiPlayer::getWorldCoordinateFor(ofPoint p) {
	return getWorldCoordinateFor((int)p.x, (int)p.y);
}

//---------------------------------------------------------------------------
ofPoint ofxKinectNuiPlayer::calcScaledSkeletonPoint(const ofPoint& skeletonPoint, float width, float height){
	float px = min((skeletonPoint.x * width) + 0.5f, (float)width);
	float py = min((skeletonPoint.y * height) + 0.5f, (float)height);
	return ofPoint(px, py);
}

