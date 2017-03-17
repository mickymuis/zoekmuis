/*
 * Websearch - imgcompare.c
 * 
 * Part of homework 4 for MIR course
 * Micky Faas
 *
 * Functions for comparing image histograms
 * This code is taken from the second homework assignment MIR,
 * with some minor modifications
 */
#include "opencvlib/imgproc/imgproc.hpp"
#include "opencvlib/highgui/highgui.hpp"
#include "opencvlib/features2d/features2d.hpp"
#include "opencvlib/core/core.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <iostream>
#include <dirent.h>

using namespace std; 

std::vector<string> load_list(const string& fname)
{
    std::vector<string> ret;
    ifstream fobj(fname.c_str());
    if (!fobj.good()) { cerr << "File `" << fname << "' not found!\n"; exit(-1); }
    string line;
    while (getline(fobj, line)) 
    {
            ret.push_back(line);
    }
    return ret;
}

int main(int argc, char * argv[] )
{	
    //compute the HISTfeature of query image
    cv::Mat image_query = cv::imread( argv[1], 1 );

    if( image_query.data == NULL ) { cerr << "Cannot load `" << argv[1] << "' Wrong file type?" << endl; return -1; }

    cv::Mat query_hist;
    //convert to HSV
    cv::cvtColor( image_query, image_query, cv::COLOR_BGR2HSV );
    // Using 50 bins for hue and 60 for saturation
    int h_bins = 50; int s_bins = 60;
    int histSize[] = { h_bins, s_bins };

    // hue varies from 0 to 179, saturation from 0 to 255
    float h_ranges[] = { 0, 180 };
    float s_ranges[] = { 0, 256 };

    const float* ranges[] = { h_ranges, s_ranges };

    // Use the o-th and 1-st channels
    int channels[] = { 0, 1 };
    cv::calcHist( &image_query, 1, channels, cv::Mat(), query_hist, 2, histSize, ranges, true, false );
    cv::normalize( query_hist, query_hist, 0, 1, cv::NORM_MINMAX, -1, cv::Mat() );
    
    //compute the HISTfeatures of database images
    std::vector<std::string> Name_list;
    std::vector<cv::Mat>dataset_features;
    DIR * dir;
    struct dirent * ptr;
    dir = opendir(argv[2]);
    
    while((ptr = readdir(dir)) != NULL)
    {
        if( ptr->d_name[0] == '.' )
            continue;
        
        //cout<<argv[2]+Name_list[n]<<endl;
        
        cv::Mat data_image = cv::imread(string(argv[2])+string(ptr->d_name),1);

        if( data_image.data == NULL ) 
            continue;
        
        Name_list.push_back(ptr->d_name);

        cv::Mat data_image_hist;
        cv::cvtColor( data_image, data_image, cv::COLOR_BGR2HSV );
        cv::calcHist( &data_image, 1, channels, cv::Mat(), data_image_hist, 2, histSize, ranges, true, false );
        cv::normalize( data_image_hist, data_image_hist, 0, 1, cv::NORM_MINMAX, -1, cv::Mat() );
        dataset_features.push_back(data_image_hist);

     
     //cout<< ptr->d_name <<endl;
    }
    closedir(dir);
    
    //calculate the similarity, apply the histogram comparison methods
    cv::Mat distance_result;
    for( int i = 0; i < Name_list.size(); i++ )
    {
            float similarity = compareHist( query_hist, dataset_features[i], 3 );
            distance_result.push_back(similarity);
            //cerr<<i<<" "<<similarity<<endl;
    }

    //knn search to rank the retrieval result
    ofstream rank_list( argv[3], ios::out|ios::trunc);
    //rank_list<<"<p><img src="<<argv[1]<<" width="<<image_query.cols*128/image_query.rows<<" height=128 /><br></p>"<<endl;	

    cv::Mat Opoint =cv::Mat( 1, 1, CV_32FC1, cv::Scalar::all(0.0) );
    cv::BruteForceMatcher< cv::L2<float> > matcher;

    // based on k nearest neighbours match
    std::vector< std::vector<cv::DMatch> > matches;
    matcher.knnMatch(Opoint,distance_result,matches, Name_list.size());   

    //re-rank the return retrieval list
    for( int n=0; n<Name_list.size(); n++ )
    {			
            cv::Mat image=cv::imread(argv[2]+Name_list[matches[0][n].trainIdx],1);
            rank_list << Name_list[matches[0][n].trainIdx] << endl;
    }
    return 0;
}
