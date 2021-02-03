#include <iostream>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/imgproc/types_c.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/ml/ml.hpp>

#include "toffy/skeletonizers/K3MSkeletonizer.hpp"

using namespace std;
using namespace cv;
//using namespace rapidjson;

//Weight for K3M
//128 01 02
// 64 XX 04
// 32 16 08

K3MSkeletonizer::K3MSkeletonizer() {
    memset(A0, 0, sizeof(A0));
    memset(A1, 0, sizeof(A1));
    memset(A2, 0, sizeof(A2));
    memset(A3, 0, sizeof(A3));
    memset(A4, 0, sizeof(A4));
    memset(A5, 0, sizeof(A5));
    init();
}
K3MSkeletonizer::~K3MSkeletonizer()
{
	Borders.release();
}

void K3MSkeletonizer::init(){
    //Lookup Table for borders marking
    unsigned char cA0[] = {3,6,7,12,14,15,24,28,30,31,48,56,60,62,63,96,112,120,124,
			   126,127,129,131,135,143,159,191,192,193,195,199,207,209,223,224,
                          225,227,231,239,240,241,243,247,248,249,251,252,253,254};
    //Lookup Tables for Phases 2-6
    unsigned char cA1[] = {7,14,28,56,112,131,193,224};
    unsigned char cA2[] = {7,14,15,28,30,56,60,112,120,131,135,193,195,224,225,240};
    unsigned char cA3[] = {7,14,15,28,30,31,56,60,62,112,120,124,131,135,143,193,195,
                          199,224,225,227,240,241,248};
    unsigned char cA4[] = {7,14,15,28,30,31,56,60,62,63,112,120,124,126,131,135,143,159
                          ,193,195,199,207,224,225,227,231,240,241,243,248,249,252};
    unsigned char cA5[] = {7,14,15,28,30,31,56,60,62,63,112,120,124,126,131,135,143,159
                          ,191,193,195,199,207,224,225,227,231,239,240,241,243,248,249
                          ,251,252,254};
    unsigned char cA1px[] = {3, 6, 7, 12, 14, 15, 24, 28, 30, 31, 48, 56,
			     60, 62, 63, 96, 112, 120, 124, 126, 127, 129, 131,
			     135, 143, 159, 191, 192, 193, 195, 199, 207, 223,
			     224, 225, 227, 231, 239, 240, 241, 243, 247, 248,
			     249, 251, 252, 253, 254};
    size_t i =0;

    for (i=0;i<sizeof(cA0);i++) {
        A0[cA0[i]] = true;
    }

    for (i=0;i<sizeof(cA1);i++) {
        A1[cA1[i]] = true;
    }

    for (i=0;i<sizeof(cA2);i++) {
        A2[cA2[i]] = true;
    }

    for (i=0;i<sizeof(cA3);i++) {
        A3[cA3[i]] = true;
    }

    for (i=0;i<sizeof(cA4);i++) {
        A4[cA4[i]] = true;
    }

    for (i=0;i<sizeof(cA5);i++) {
        A5[cA5[i]] = true;
    }

    for (i=0;i<sizeof(cA1px);i++) {
        A6[cA1px[i]] = true;
    }

    Debugging = false;
}


void K3MSkeletonizer::skeletonize(const cv::Mat& lines, cv::Mat& skel){

    //imshow("k3m PRE LINES", lines );
    //imshow("k3m PRE LINES>>", lines>0 );

    //Copy the src mat to the dest one and make the border Mat;
    lines.copyTo(Borders);
    lines.copyTo(Input);
    Borders.copyTo(skel);
    if(Debugging){
        cout << "K3MSkeletonizer : Computing thickness"<< endl;
    }
    computeThickness(Input);
    if(Debugging){
        cout << "K3MSkeletonizer : Skeletonizing" <<endl;
    }
    Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));

    //make a closing to remove small hole
    //dilate(skel, skel, element);
    //erode(skel, skel, element);

    //imshow("k3m PRE SKEL", skel>0 );

    bool modified;
    //int iter=0;
    do{
        modified = false;
        //PHASE 0 MARK BORDER
        markborder(skel);

        //cout<<"phase 1" << endl;
        modified |= phase(skel,&A1[0]);
        //cout<<"phase 2" << endl;
        modified |= phase(skel,&A2[0]);
        //cout<<"phase 3" << endl;
        modified |= phase(skel,&A3[0]);
        //<<"phase 4" << endl;
        modified |= phase(skel,&A4[0]);
        //cout<<"phase 5" << endl;
        modified |= phase(skel,&A5[0]);

	
	//string n("0 k3m_skel");
	//n[0] = 'A'+iter;
	//imshow(n, skel > 0);
	//iter++;

    }while(modified);
    if(Debugging){
        cout << "K3MSkeletonizer : Prepare for Tracing"<< endl;
    }
    readyToTrace(skel);
    if(Debugging){
        cout << "K3MSkeletonizer : Skeletonization finished" << endl;
    }
}
/*
void K3MSkeletonizer::configure(const rapidjson::Value &configObject)
{
    Debugging = configObject["Debug"].GetBool();
}
*/
Skeletonizer *K3MSkeletonizer::Create()
{
    return new K3MSkeletonizer();
}

void K3MSkeletonizer::computeThickness(const Mat& in) {
    distanceTransform(in, thickness, CV_DIST_L2, 3);
#if 0
    // morphol. thinning, based on
    // http://felix.abecassis.me/2011/09/opencv-morphological-skeleton/
    Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(3, 3));

    Mat lines = in.clone();
    lines.setTo(Scalar(1),in);

    thickness.create(lines.rows, lines.cols, CV_8UC1);
    cv::Mat temp(lines.size(), CV_8UC1);
    thickness.setTo(Scalar(0));

    bool done;
    int count=0;
    do {
        thickness += lines;
        cv::erode(lines, lines, element, Point(-1,-1),1,
		  BORDER_CONSTANT, Scalar(0) );

        double max;
        cv::minMaxLoc(lines, 0, &max);
        done = (max == 0);
	if (max==1 && count>500) {
	    cout << "HOPPALA!!! mx " << max << endl;
	    imshow("lines" , lines>0);
	    imshow("thickness" , thickness);
	    waitKey(100);
	    done=true;
	}
	count++;
    } while (!done);
#endif
    thickness.convertTo(thickness,CV_8UC1);
    //imshow("thickness" , thickness*16);
}

void K3MSkeletonizer::markborder(const cv::Mat &mat){
    //Go through the picture
    const unsigned char* cur=0;
    const unsigned char* prev=0;
    const unsigned char* next=0;
    //Rows
    for(int i = 0; i < mat.rows;i++){
        //get lines ptr
        if((i-1) >= 0) prev = mat.ptr<unsigned char>(i-1);
        cur = mat.ptr<unsigned char>(i);
        if((i+1) < mat.rows )next = mat.ptr<unsigned char>(i+1);
        //Columns
        for(int j = 0; j < mat.cols;j++){
            //is a object pix?
            if(!cur[j]){
                Borders.at<uchar>(i,j) = 0;
                continue;
            }
            //Calculate the weight
            int weight = 0;
            //Previous column
            if((j-1) >= 0){
                if((i-1) >= 0 && prev[j-1]) weight += 128;
                if(cur[j-1]) weight += 64;
                if((i+1) < mat.rows && next[j-1])weight += 32;
            }
            //next Column
            if((j+1) < mat.cols){
                if((i-1) >= 0 && prev[j+1]) weight += 2;
                if(cur[j+1]) weight += 4;
                if((i+1) < mat.rows && next[j+1])weight += 8;
            }
            //current column
            if((i-1) > 0 && prev[j]) weight++;
            if((i+1) < mat.rows && next[j]) weight += 16;
            //If weight in A0 then it's a border

            if(A0[weight]){
                Borders.at<uchar>(i,j) = 1;
            }
            else{
                Borders.at<uchar>(i,j) = 0;
            }
        }
    }
}

bool K3MSkeletonizer::phase(cv::Mat mat, bool *lookup){
    //Go through the picture
    const unsigned char* cur;
    const unsigned char* prev;
    const unsigned char* next;
    bool toReturn = false;
    //Rows
    for(int i = 0; i < mat.rows;i++){
        //get line ptr
        if((i-1) > 0) prev = mat.ptr<unsigned char>(i-1);
        cur = mat.ptr<unsigned char>(i);
        if((i+1) < mat.rows )next = mat.ptr<unsigned char>(i+1);
        //Columns
        for(int j = 0; j < mat.cols;j++){
            //is a border pix?
            if(!Borders.at<uchar>(i,j)){
                continue;
            }
            //Calculate the weight
            int weight = 0;
            //Previous column
            if((j-1) > 0){
                if((i-1) > 0 && prev[j-1]) weight += 128;
                if(cur[j-1]) weight += 64;
                if((i+1) < mat.rows && next[j-1])weight += 32;
            }
            //next Column
            if((j+1) < mat.cols){
                if((i-1) > 0 && prev[j+1]) weight += 2;
                if(cur[j+1]) weight += 4;
                if((i+1) < mat.rows && next[j+1])weight += 8;
            }
            //current column
            if((i-1) > 0 && prev[j]) weight++;
            if((i+1) < mat.rows && next[j]) weight += 16;
            //if weight is in lookup then it's to delete
            if(lookup[weight]){
                toReturn = true;
                mat.at<uchar>(i,j) = 0;
            }
        }
    }
    return toReturn;
}

/*
 * Search for patern around the current pixel, then remove it if it's found
 * patterns searched for
 * (o = object, b = background, c = current, ? = don't care)
 *  b ? ? | ? ? b | ? o ? | ? o ?
 *  ? c o | o c ? | o c ? | ? c o
 *  ? o ? | ? o ? | ? ? b | b ? ?
 *--------------------------------
 *  ? o ? | ? o ? | ? ? ? | ? o ?
 *  o c ? | ? c o | o c o | o c o
 *  ? o ? | ? o ? | ? o ? | ? ? ?
 *||
 */
void K3MSkeletonizer::RemoveTee(cv::Mat mat){
    //Go through the picture
    const unsigned char* cur;
    const unsigned char* prev;
    const unsigned char* next;
    //Rows
    for(int i = 1; i < mat.rows-1;i++){
        //get line ptr
        prev = mat.ptr<unsigned char>(i-1);
        cur = mat.ptr<unsigned char>(i);
        next = mat.ptr<unsigned char>(i+1);
        //Columns
        for(int j = 0; j < mat.cols;j++){
            //is a object pix?
            if(!cur[j]){
                continue;
            }
            //Check for the pattern
            if((cur[j+1] && prev[j] && cur[j-1])
            || (cur[j+1] && next[j] && cur[j-1])
            || (next[j] && prev[j] && cur[j-1])
            || (next[j] && prev[j] && cur[j+1])){
                mat.at<uchar>(i,j) = 0;
            }
            else{
                mat.at<uchar>(i,j) = 1;
            }
        }
    }
}

void K3MSkeletonizer::RemoveCorners(cv::Mat mat){
    //Go through the picture
    const unsigned char* cur;
    const unsigned char* prev;
    const unsigned char* next;
    //Rows
    for(int i = 1; i < mat.rows-1;i++){
        //get line ptr
        prev = mat.ptr<unsigned char>(i-1);
        cur = mat.ptr<unsigned char>(i);
        next = mat.ptr<unsigned char>(i+1);
        //Columns
        for(int j = 0; j < mat.cols;j++){
            //is a object pix?
            if(!cur[j]){
                continue;
            }
            //Check for the pattern
            if((!prev[j-1] && next[j] && cur[j+1])
            || (!prev[j+1] && next[j] && cur[j-1])
            || (!next[j-1] && prev[j] && cur[j+1])
            || (!next[j+1] && prev[j] && cur[j-1])){
                mat.at<uchar>(i,j) = 0;
            }
            else{
                mat.at<uchar>(i,j) = 1;
            }
        }
    }
}

void K3MSkeletonizer::readyToTrace(cv::Mat &mat)
{
    //Go through the picture
    const unsigned char* cur;

    //Rows
    for(int i = 1; i < mat.rows-1;i++){
        //get line ptr
        cur = mat.ptr<unsigned char>(i);
        //Columns
        for(int j = 0; j < mat.cols;j++){
            //is a object pix?
            if(!cur[j]){
                continue;
            }
            mat.at<uchar>(i,j) = 1;
        }
    }
}