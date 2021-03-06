// IPPRAssignment.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <string>
#include "highgui/highgui.hpp"
#include "core/core.hpp"
#include "opencv2/imgproc.hpp" // threshold header file
#include <vector>
#include "objdetect.hpp"
#include <opencv2/video.hpp>	// for background subtractor
#include <opencv2/features2d.hpp>

#include "Blob.h"

using namespace cv;
using namespace std;

const Scalar SCALAR_BLACK = Scalar(0.0, 0.0, 0.0);
const Scalar SCALAR_WHITE = Scalar(255.0, 255.0, 255.0);
const Scalar SCALAR_YELLOW = Scalar(0.0, 255.0, 255.0);
const Scalar SCALAR_GREEN = Scalar(0.0, 200.0, 0.0);
const Scalar SCALAR_RED = Scalar(0.0, 0.0, 255.0);
const Scalar SCALAR_BLUE = Scalar(200.0, 0.0, 0.0);

// 1. resize interest region
// 2. bg subtraction
// 3. get contour and convex hull
// 4. make it a blob but no need to track
// 5. From the blob width and height, determine the type of vehicle
// 6. repeat the process until video end

Mat drawAndShowContours(Size imageSize, vector<Blob> blobs, string strImageName) {

	Mat image(imageSize, CV_8UC3, SCALAR_BLACK);

	vector<vector<Point> > contours;

	for (auto &blob : blobs) {
		contours.push_back(blob.currentContour); //get all contours from blob
	}

	drawContours(image, contours, -1, SCALAR_WHITE, -1);

	/*
	* Show blob image
	*/
	//imshow(strImageName, image);

	return image;
}

Mat drawAndShowContours(Size imageSize, vector<vector<Point> > contours, string strImageName) {
	//vector<vector<Point> > contours is the found object
	Mat image(imageSize, CV_8UC3, SCALAR_BLACK);

	drawContours(image, contours, -1, SCALAR_WHITE, -1);

	/*
	* Show blob image
	*/
	//imshow(strImageName, image);

	return image;
}

//background subtaction variable
//fg = foreground

Mat fgMaskMOG2;
Mat fgMaskKNN;
Mat fgMaskGMG;

//MOG and GMG is not stable and is available in contrib opencv
Ptr<BackgroundSubtractor> pMOG2;
Ptr<BackgroundSubtractor> pKNN;

vector<Blob> currentFrameKNNblob;
vector<Blob> currentFrameMOG2blob;

int main()
{
	VideoCapture camera;
	//camera = VideoCapture(0);
	camera = VideoCapture("car.mp4");

	/*
	 * Save as video output
	 */
	/*
	int frame_width = static_cast<int>(camera.get(CAP_PROP_FRAME_WIDTH)); //get the width of frames of the video
	int frame_height = static_cast<int>(camera.get(CAP_PROP_FRAME_HEIGHT)); //get the height of frames of the video

	Size frame_size(frame_width, frame_height);
	int frames_per_second = 30;

	VideoWriter outputVideo("D:/output.avi", VideoWriter::fourcc('M', 'J', 'P', 'G'),
		frames_per_second, frame_size, true);

	*/

	pKNN = createBackgroundSubtractorKNN(500, 300);
	pMOG2 = createBackgroundSubtractorMOG2(500, 0);

	bool cameraResponding = false;

	Mat temp;

	camera.read(temp);

	Mat original = temp.clone();
	Mat mask(temp.size(), CV_8UC3, SCALAR_BLACK);

	int mask_minimum_col = mask.size().width * 2 / 5;
	int mask_maximum_row = mask.size().height - (mask.size().height * 2 / 5);
	for (int row = 0; row < mask.rows; row++) {
		for (int col = 0; col < mask.cols; col++) {
			if (col > mask_minimum_col && row < mask_maximum_row) {
				mask.at<Vec3b>(row, col)[0] = 1;
				mask.at<Vec3b>(row, col)[1] = 1;
				mask.at<Vec3b>(row, col)[2] = 1;
			}
		}
	}

	while (camera.isOpened()) {
		Mat tempImage;
		Mat image;

		//specify interest region		
		camera.read(tempImage);

		// reach to the end of the video file
		if (tempImage.empty()) {
			break;
		}

		//processing start here
		Mat original = tempImage.clone();

		tempImage.copyTo(image, mask);
		//only masked image is resized
		resize(image, image, Size(640, 360));
		Mat Original = image.clone();

		//convert clone image to greyscale
		cvtColor(image, image, COLOR_BGR2GRAY);

		//blur clone image using gaussian
		GaussianBlur(image, image, Size(5, 5), 0);

		pMOG2->apply(image, fgMaskMOG2, 0.90);
		pKNN->apply(image, fgMaskKNN, 0.90);

		//threshold(image, image, 30, 255.0, THRESH_BINARY);
		//Mat imgThreshCopy = image.clone();

		/*
		* Show Background subtracted original image, MOG2 applied image and KNN applied image
		*/
		//imshow("Original", Original);
		//imshow("MOG2", fgMaskMOG2);
		//imshow("KNN", fgMaskKNN);
		//imshow("Thresh image", imgThreshCopy);

		//structuring element for morphological operation
		Mat structuringElement3x3 = getStructuringElement(MORPH_RECT, Size(3, 3));
		Mat structuringElement5x5 = getStructuringElement(MORPH_RECT, Size(5, 5));
		Mat structuringElement7x7 = getStructuringElement(MORPH_RECT, Size(7, 7));
		Mat structuringElement15x15 = getStructuringElement(MORPH_RECT, Size(15, 15));

		//dilate and closing
		for (unsigned int i = 0; i < 10; i++) {
			dilate(image, image, structuringElement15x15);
			dilate(image, image, structuringElement15x15);
			erode(image, image, structuringElement15x15);
		}

		//clone thresh image for further processing

		//find contour on clone thresh image
		vector<vector<Point> > contoursKNN;
		findContours(fgMaskKNN, contoursKNN, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		drawAndShowContours(image.size(), contoursKNN, "imgContoursKNN");

		vector<vector<Point> > contoursMOG2;
		findContours(fgMaskMOG2, contoursMOG2, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		drawAndShowContours(image.size(), contoursMOG2, "imgContoursMOG2");

		//use convex hull on contours
		vector<vector<Point> > convexHullsKNN(contoursKNN.size());

		for (unsigned int i = 0; i < contoursKNN.size(); i++) {
			convexHull(contoursKNN[i], convexHullsKNN[i]);
		}

		vector<vector<Point> > convexHullsMOG2(contoursMOG2.size());

		for (unsigned int i = 0; i < contoursMOG2.size(); i++) {
			convexHull(contoursMOG2[i], convexHullsMOG2[i]);
		}

		Mat KNNConvex = drawAndShowContours(image.size(), convexHullsKNN, "imgConvexHullsKNN");
		Mat MOG2Convex = drawAndShowContours(image.size(), convexHullsMOG2, "imgConvexHullsMOG2");

		//for (unsigned int i = 0; i < 10; i++) {
		//	dilate(KNNConvex, KNNConvex, structuringElement3x3);
		//	dilate(KNNConvex, KNNConvex, structuringElement7x7);
		//	erode(KNNConvex, KNNConvex, structuringElement7x7);

		//	dilate(MOG2Convex, MOG2Convex, structuringElement3x3);
		//	dilate(MOG2Convex, MOG2Convex, structuringElement7x7);
		//	erode(MOG2Convex, MOG2Convex, structuringElement7x7);
		//}

		//imshow("KNN test", KNNConvex);
		//imshow("MOG2 test", MOG2Convex);

		//construct blob
		for (auto &convexHull : convexHullsKNN) {
			Blob possibleBlob(convexHull);

			if (possibleBlob.currentBoundingRect.area() > 400 && // get convexhull bounding rect area
				possibleBlob.dblCurrentAspectRatio > 0.2 && // Aspect ratio with 0.2 - 4.0
				possibleBlob.dblCurrentAspectRatio < 4.0 &&
				possibleBlob.currentBoundingRect.width > 30 && //with width and height > 30
				possibleBlob.currentBoundingRect.height > 30 &&
				possibleBlob.dblCurrentDiagonalSize > 60.0 && //diagonal size or magnitude length
				(contourArea(possibleBlob.currentContour) / (double)possibleBlob.currentBoundingRect.area()) > 0.50)
				//contour area / contour bounding rect area < 0.5
			{
				currentFrameKNNblob.push_back(possibleBlob);
			}
		}

		for (auto &convexHull : convexHullsMOG2) {
			Blob possibleBlob(convexHull);

			if (possibleBlob.currentBoundingRect.area() > 400 &&
				possibleBlob.dblCurrentAspectRatio > 0.2 &&
				possibleBlob.dblCurrentAspectRatio < 4.0 &&
				possibleBlob.currentBoundingRect.width > 30 &&
				possibleBlob.currentBoundingRect.height > 30 &&
				possibleBlob.dblCurrentDiagonalSize > 60.0 &&
				(contourArea(possibleBlob.currentContour) / (double)possibleBlob.currentBoundingRect.area()) > 0.50) {
				currentFrameMOG2blob.push_back(possibleBlob);
			}
		}

		Mat KNNBlob = drawAndShowContours(image.size(), currentFrameKNNblob, "imgCurrentFrameBlobsKNN");
		Mat MOG2Blob = drawAndShowContours(image.size(), currentFrameMOG2blob, "imgCurrentFrameBlobsMOG2");

		////morphology closing on blob image
		//for (unsigned int i = 0; i < 10; i++) {
		//	//dilate(KNNConvex, KNNConvex, structuringElement3x3);
		//	dilate(KNNBlob, KNNBlob, structuringElement15x15);
		//	erode(KNNBlob, KNNBlob, structuringElement15x15);

		//	//dilate(MOG2Convex, MOG2Convex, structuringElement3x3);
		//	dilate(MOG2Blob, MOG2Blob, structuringElement15x15);
		//	erode(MOG2Blob, MOG2Blob, structuringElement15x15);
		//}

		//imshow("KNN Blob test", KNNBlob);
		//imshow("MOG2 Blob test", MOG2Blob);

		//try {
		//	findContours(KNNBlob, contoursBlobKNN, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		//}
		//catch (Exception e) {
		//	cerr << e.msg << endl; // output exception message
		//}

		//classifying
		Mat GreyKNNBlob;
		Mat GreyMOG2Blob;

		cvtColor(KNNBlob, GreyKNNBlob, COLOR_BGR2GRAY);
		cvtColor(MOG2Blob, GreyMOG2Blob, COLOR_BGR2GRAY);

		vector<vector<Point> > contoursBlobKNN;
		findContours(GreyKNNBlob, contoursBlobKNN, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		vector<vector<Point> > contoursBlobMOG2;
		findContours(GreyMOG2Blob, contoursBlobMOG2, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		vector<Rect> rectBlobKNN;
		vector<Rect> rectBlobMOG2;

		for (unsigned int i = 0; i < contoursBlobKNN.size(); i++) {
			rectBlobKNN.push_back(boundingRect(contoursBlobKNN[i]));
		}

		for (unsigned int i = 0; i < contoursBlobMOG2.size(); i++) {
			rectBlobMOG2.push_back(boundingRect(contoursBlobMOG2[i]));
		}

		int KNN_car = 0;
		int KNN_truck = 0;
		int KNN_motorbikes = 0;

		int MOG2_car = 0;
		int MOG2_truck = 0;
		int MOG2_motorbikes = 0;

		int truckHeight = GreyKNNBlob.size().height / 6 * 2;
		int truckHeightInFocal = GreyKNNBlob.size().height / 40;

		int focalWidthMin = GreyKNNBlob.size().width / 6 * 4;
		int focalWidthMax = GreyKNNBlob.size().width / 11 * 9;

		//classification
		for (unsigned int i = 0; i < rectBlobKNN.size(); i++) {

			if (rectBlobKNN[i].x > focalWidthMin && rectBlobKNN[i].x < focalWidthMax) { //check within focal
				if (truckHeight - rectBlobKNN[i].y > truckHeightInFocal) { //truck have lower y value
					KNN_truck++;
				}
				else { //car have higher y value
					KNN_car++;
				}
			}
			else if(rectBlobKNN[i].x > focalWidthMax){ //check rhs of focal - detection where truck 
				if (rectBlobKNN[i].y > truckHeight) {
					KNN_car++;
				}
				else if (rectBlobKNN[i].y - truckHeight < truckHeightInFocal) { //truck height is not exceeding truck height
					KNN_truck++;
				}
				else {
					KNN_car++;
				}
			}
			else { //lhs of focal - plain detection using height
				if (rectBlobKNN[i].y > truckHeight) { 
					KNN_car++;
				}
				else {
					KNN_truck++;
				}
			}
		}

		for (unsigned int i = 0; i < rectBlobMOG2.size(); i++) {

			if (rectBlobMOG2[i].x > focalWidthMin && rectBlobMOG2[i].x < focalWidthMax) { //check within focal
				if (truckHeight - rectBlobMOG2[i].y > truckHeightInFocal) { //truck have lower y value
					MOG2_truck++;
				}
				else { //car have higher y value
					MOG2_car++;
				}
			}
			else if (rectBlobMOG2[i].x > focalWidthMax) { //check rhs of focal - detection where truck 
				if (rectBlobMOG2[i].y - truckHeight < truckHeightInFocal) { //truck height is not exceeding truck height
					MOG2_truck++;
				}
				else {
					MOG2_car++;
				}
			}
			else { //lhs of focal - plain detection using height
				if (rectBlobMOG2[i].y > truckHeight) {
					MOG2_car++;
				}
				else {
					MOG2_truck++;
				}
			}
		}

		//for drawing display
		int KNNTextPosition = original.size().width / 5 * 0 + 50;
		int MOG2TextPosition = original.size().width / 5 * 1 + 50;

		int imageTruckHeight = original.size().height / 6 * 2;
		int imageFocalWidthMin = original.size().width / 6 * 4;
		int imageFocalWidthMax = original.size().width / 11 * 9;

		//display data in original image
		//interest region
		line(original, Point(mask_minimum_col, 0), Point(mask_minimum_col, mask_maximum_row), SCALAR_YELLOW, 3);
		line(original, Point(mask_minimum_col, mask_maximum_row), Point(original.size().width, mask_maximum_row), SCALAR_YELLOW, 3);

		//detecting rules region
		//line(original, Point(mask_minimum_col, imageTruckHeight), Point(original.size().width, imageTruckHeight), SCALAR_YELLOW, 3);
		//line(original, Point(imageFocalWidthMin, imageTruckHeight), Point(imageFocalWidthMin, original.size().height), SCALAR_YELLOW, 3);
		//line(original, Point(imageFocalWidthMax, imageTruckHeight), Point(imageFocalWidthMax, original.size().height), SCALAR_YELLOW, 3);

		putText(original, "KNN Object detected: " + to_string(rectBlobKNN.size()), Point(KNNTextPosition, 40), 1, 1, SCALAR_RED, 2);
		putText(original, "Car: " + to_string(KNN_car), Point(KNNTextPosition, 55), 1, 1, SCALAR_RED, 2);
		putText(original, "Truck: " + to_string(KNN_truck), Point(KNNTextPosition, 70), 1, 1, SCALAR_RED, 2);
		//putText(original, "Motorbikes: " + to_string(KNN_motorbikes), Point(KNNTextPosition, 85), 1, 1, SCALAR_RED, 2);

		putText(original, "MOG2 Object detected: " + to_string(rectBlobMOG2.size()), Point(MOG2TextPosition, 40), 1, 1, SCALAR_RED, 2);
		putText(original, "Car: " + to_string(MOG2_car), Point(MOG2TextPosition, 55), 1, 1, SCALAR_RED, 2);
		putText(original, "Truck: " + to_string(MOG2_truck), Point(MOG2TextPosition, 70), 1, 1, SCALAR_RED, 2);
		//putText(original, "Motorbikes: " + to_string(MOG2_motorbikes), Point(MOG2TextPosition, 85), 1, 1, SCALAR_RED, 2);

		Mat OriginalSizeGreyKNNBlob;
		Mat OriginalSizeGreyMOG2Blob;

		resize(GreyKNNBlob, OriginalSizeGreyKNNBlob, Size(GreyKNNBlob.cols * 3, GreyKNNBlob.rows * 3));
		resize(GreyMOG2Blob, OriginalSizeGreyMOG2Blob, Size(GreyMOG2Blob.cols * 3, GreyMOG2Blob.rows * 3));

		//imshow("Original KNN Blob", OriginalSizeGreyKNNBlob);
		//imshow("Original MOG2 Blob", OriginalSizeGreyMOG2Blob);

		vector<vector<Point> > originalContoursBlobKNN;
		findContours(OriginalSizeGreyKNNBlob, originalContoursBlobKNN, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		vector<vector<Point> > originalContoursBlobMOG2;
		findContours(OriginalSizeGreyMOG2Blob, originalContoursBlobMOG2, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		//vector<Rect> originalRectBlobKNN;
		//vector<Rect> originalRectBlobMOG2;

		for (unsigned int i = 0; i < originalContoursBlobKNN.size(); i++) {
			//originalRectBlobKNN.push_back(boundingRect(originalContoursBlobKNN[i]));
			rectangle(original, boundingRect(originalContoursBlobKNN[i]), SCALAR_BLUE);
		}

		for (unsigned int i = 0; i < originalContoursBlobMOG2.size(); i++) {
			//originalRectBlobMOG2.push_back(boundingRect(originalContoursBlobMOG2[i]));
			rectangle(original, boundingRect(originalContoursBlobMOG2[i]), SCALAR_GREEN);
		}

		imshow("original", original);

		/*
		 * Write to video writer 
		 */
		//outputVideo.write(original);

		currentFrameKNNblob.clear();
		currentFrameMOG2blob.clear();

		int key = (waitKey(30) & 0xFF);
		cout << key;
		if (key == 27 || key == 'q') { //27 == ESC
			break;
		}
	}
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
