/*
  Copyright 2018-2021 Simon Vogl <svogl@voxel.at>

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/
#include <string>
#include <iostream>
#include <fstream>
#include <limits.h>


#include <boost/date_time/posix_time/posix_time.hpp>

#include <boost/log/trivial.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>

#if OCV_VERSION_MAJOR >= 3
#  include <opencv2/core.hpp>
#  include <opencv2/calib3d.hpp>
#else
#  include <opencv2/core/core.hpp>
#  include <opencv2/calib3d/calib3d.hpp>
#endif


#include <pcl/range_image/range_image_planar.h>

#include <pcl/PCLPointCloud2.h>

#include <pcl/conversions.h> // for toPCLPointCloud2

#include <pcl/sample_consensus/sac.h>
#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model.h>
#include <pcl/sample_consensus/sac_model_line.h>
#include <pcl/sample_consensus/sac_model_plane.h>
#include <pcl/sample_consensus/sac_model_cylinder.h>

#include <pcl/segmentation/region_growing.h>

#include <pcl/features/normal_3d.h>


#include <pcl/filters/extract_indices.h>
#include <pcl/filters/passthrough.h>
#include <pcl/features/normal_3d.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>

//hack on:
#include <pcl/io/pcd_io.h>


#include "toffy/3d/sampleConsensus.hpp"

using namespace toffy;
using namespace toffy::filters::f3d;
using namespace cv;
using namespace std;

#ifdef CM_DEBUG
// const bool dbg=true;
#else
// const bool dbg=false;
#endif

// typedef pcl::PointXYZ PointT;

// hack on:
Frame* inFrame;

bool SampleConsensus::filter(const Frame &in, Frame& out) 
{
    using namespace boost::posix_time;

inFrame = &out;

    ptime start = boost::posix_time::microsec_clock::local_time();
    ptime tick = boost::posix_time::microsec_clock::local_time();
    boost::posix_time::time_duration diff = tick - start;
    // pcl::PointIndices::Ptr inliers_plane (new pcl::PointIndices), inliers_cylinder (new pcl::PointIndices);

    if (!in.hasKey(this->in)) {
	    LOG(warning) << "Input cloud not found: " << this->in << ", filter  "
		     << id() << " not applied.";
    	return false;
    }

    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud;

    pcl::PointCloud<pcl::PointXYZ>::Ptr pi(new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr po(new pcl::PointCloud<pcl::PointXYZ>);

    bool success = getInputPoints(in, cloud);   
    if (!success) {
        // we complained already...
        return false;
    }

    switch ( type ) {
    case plane:
    	success = segmentPlane(cloud, pi, po);
	    break;
    case cylinder:
	    success = segmentCylinder(cloud, pi, po);
	    break;
    case regionGrowing:
	    success = runRegionGrowing(cloud, out);
	    break;
    case euclidean:
	    success = runEuclidean(cloud, out);
	    break;
    case line:
        // model.reset(new pcl::SampleConsensusModelLine<pcl::PointXYZ>(cloud));
        // break;
    default:
        LOG(error) << "cannot compute " << sampleTypeNames[type];
        return false;
    }


    if (outputPC2) {
        pcl::PCLPointCloud2::Ptr min;
        pcl::PCLPointCloud2::Ptr mout;
        min.reset(new pcl::PCLPointCloud2());
        mout.reset(new pcl::PCLPointCloud2());
        pcl::toPCLPointCloud2(*pi, *min);
        pcl::toPCLPointCloud2(*po, *mout);

        out.addData( this->inliers, min);
        out.addData( this->outliers, mout);

    } else {
        out.addData( this->inliers, pi);
        out.addData( this->outliers, po);                
    }


    tick = boost::posix_time::microsec_clock::local_time();
    diff = tick - start;
    LOG(info) << "td\t" << diff.total_milliseconds() << "\tfin" << endl;

    return true;
}

bool SampleConsensus::segmentPlane(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
				   pcl::PointCloud<pcl::PointXYZ>::Ptr& pi, 
				   pcl::PointCloud<pcl::PointXYZ>::Ptr& po)
{
    std::vector<int> inliers;

    pcl::SampleConsensusModelPlane<pcl::PointXYZ>::Ptr
        model_plane(new pcl::SampleConsensusModelPlane<pcl::PointXYZ>(cloud));
    
    pcl::RandomSampleConsensus<pcl::PointXYZ> ransac(model_plane, threshold );
    
    ransac.setDistanceThreshold(threshold);
    ransac.setMaxIterations(maxIters);

    bool success = ransac.computeModel(1);
    if (!success) {
	cout << " could not ransac - skip!" << endl;
	return false;
    }

    ransac.getInliers(inliers);

    if (inliers.size() ) {
	size_t idx=0;
	size_t pIdx=0;
	while ( pIdx < cloud->size() ) {
	    if ( pIdx == (unsigned int)inliers[idx]) {
		pi->push_back( cloud->at(pIdx));
		if (idx<inliers.size()) // or push a dummy value to inliers
		    idx++;
	    } else {
		po->push_back( cloud->at(pIdx));
	    }
	    pIdx++;
	}

    }
    Eigen::VectorXf coeffs;
    ransac.getModelCoefficients(coeffs);
    /*
    float roll, pitch, yaw;	
    Eigen::Affine3f A;
    pcl::getEulerAngles ( coeffs, roll, pitch, yaw);

    cout << "PLAN " << coeffs.transpose() << " " << roll << " " << pitch << " " << yaw <<  endl;
    */
    
    LOG(info) << "PLAN " << coeffs.transpose() << endl;
    return true;
}

// bool SampleConsensus::computeNormals(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
//     pcl::PointCloud<pcl::Normal>::Ptr& cloud_normals)
// ){

// }

bool SampleConsensus::segmentCylinder(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
				   pcl::PointCloud<pcl::PointXYZ>::Ptr& pi, 
				   pcl::PointCloud<pcl::PointXYZ>::Ptr& po)
{
    LOG(info) << __LINE__;
    pcl::PointCloud<pcl::Normal>::Ptr cloud_normals (new pcl::PointCloud<pcl::Normal>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered2 (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ> ());
    pcl::ModelCoefficients::Ptr coefficients_cylinder (new pcl::ModelCoefficients);
    pcl::PointIndices::Ptr inliers_cylinder (new pcl::PointIndices);
    pcl::SampleConsensusModel<pcl::PointXYZ>::Ptr model;

    pcl::SACSegmentationFromNormals<pcl::PointXYZ, pcl::Normal> seg; 
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    pcl::ExtractIndices<pcl::PointXYZ> extract;
    pcl::PassThrough<pcl::PointXYZ> pass;
    LOG(info) << __LINE__;

    // Build a passthrough filter to remove spurious NaNs and scene background
    pass.setInputCloud (cloud);
    pass.setFilterFieldName ("z");
    pass.setFilterLimits (0.75, 7.5);
    pass.filter (*cloud_filtered);

    // Build a passthrough filter to remove spurious NaNs and scene background
    pass.setInputCloud (cloud_filtered);
    pass.setFilterFieldName ("y");
    pass.setFilterLimits (-1.5, +1.5);
    pass.filter (*cloud_filtered2);

    LOG(debug) << "PointCloud after filtering has: " << cloud_filtered->size () << " data points." << std::endl;

if (inFrame) {
    inFrame->addData("fil2", cloud_filtered2);
}

    // Estimate point normals
    ne.setSearchMethod (tree);
    ne.setInputCloud (cloud_filtered2);
    // ne.setKSearch (50);
    ne.setRadiusSearch (0.1);
    ne.compute (*cloud_normals);
    LOG(info) << __LINE__ << " normals " << cloud_normals->size();

    // pcl::SACSegmentationFromNormals<PointT, pcl::Normal> seg; 
    //   seg.setNormalDistanceWeight (0.1);
  
//     LOG(info) << __LINE__;
//     model.reset(new pcl::SampleConsensusModelCylinder<pcl::PointXYZ, pcl::Normal>(cloud) );
//     LOG(info) << __LINE__;
//     ransac.setSampleConsensusModel(model);
//     LOG(info) << __LINE__;

    seg.setOptimizeCoefficients (true);
    seg.setModelType (pcl::SACMODEL_CYLINDER);
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setNormalDistanceWeight (0.05);
    seg.setMaxIterations (maxIters);
    seg.setDistanceThreshold (threshold);
    seg.setRadiusLimits (minRadius, maxRadius);
    seg.setInputCloud (cloud_filtered2);
    seg.setInputNormals (cloud_normals);

    // Obtain the cylinder inliers and coefficients
    //  seg.segment (*inliers_cylinder, *coefficients_cylinder);
    seg.segment (*inliers_cylinder, *coefficients_cylinder);
    std::cerr << "Cylinder coefficients: " << *coefficients_cylinder << std::endl;

    // Write the cylinder inliers to disk
    extract.setInputCloud (cloud_filtered);
    extract.setIndices (inliers_cylinder);
    extract.setNegative (false);
    extract.filter (*pi);
    
    if (pi->points.empty ()) {
	LOG(info) << "Can't find the cylindrical component." << std::endl;
    }  else {
	LOG(debug) << " cylinder " << pi->size();
    }

    extract.setInputCloud (cloud_filtered);
    extract.setIndices (inliers_cylinder);
    extract.setNegative (true);
    extract.filter (*po);

    LOG(info) << __LINE__ << " po " << po->size();

//  seg.getModel()->computeModelCoefficients (const Indices &samples, Eigen::VectorXf &model_coefficients) const =0

    // Eigen::VectorXf coeffs;
    // seg.getModel()->getModelCoefficients(coeffs);
    
    // LOG(info) << "CYL " << coeffs.transpose() << endl;

    return true;
}

static int cnt=0;

bool SampleConsensus::runRegionGrowing(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
Frame& out)
{
    pcl::search::Search<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);
    pcl::PointCloud <pcl::Normal>::Ptr normals (new pcl::PointCloud <pcl::Normal>);


    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered (new pcl::PointCloud<pcl::PointXYZ>);
    // pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered2 (new pcl::PointCloud<pcl::PointXYZ>);

    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> normal_estimator;
    pcl::PassThrough<pcl::PointXYZ> pass;
    pcl::ExtractIndices<pcl::PointXYZ> extract;

    // Build a passthrough filter to remove spurious NaNs and scene background
    pass.setInputCloud (cloud);
    pass.setFilterFieldName ("z");
    pass.setFilterLimits (0.75, 7.5);
    pass.filter (*cloud_filtered);

    // // Build a passthrough filter to remove spurious NaNs and scene background
    // pass.setInputCloud (cloud_filtered);
    // pass.setFilterFieldName ("y");
    // pass.setFilterLimits (-1.5, +1.5);
    // pass.filter (*cloud_filtered2);

    LOG(debug) << "PointCloud after filtering has: " << cloud_filtered->size () << " data points." << std::endl;

    normal_estimator.setSearchMethod (tree);
    normal_estimator.setInputCloud (cloud_filtered);
    normal_estimator.setKSearch (50);
    normal_estimator.compute (*normals);



    pcl::IndicesPtr indices (new std::vector <int>);
    pcl::removeNaNFromPointCloud(*cloud_filtered, *indices);

    pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> reg;
    reg.setMinClusterSize (50);
    reg.setMaxClusterSize (1000000);
    reg.setSearchMethod (tree);
    reg.setNumberOfNeighbours (30);
    reg.setInputCloud (cloud_filtered);
    reg.setIndices (indices);
    reg.setInputNormals (normals);
    reg.setSmoothnessThreshold (smoothnessThresh / 180.0 * M_PI);
    reg.setCurvatureThreshold (curvatureThresh);

    std::vector <pcl::PointIndices> clusters;
    reg.extract (clusters);

    LOG(debug) << "------------- " << cnt;
    std::cout << "Number of clusters is equal to " << clusters.size () << std::endl;
    if (clusters.size ()) {
        std::cout << "First cluster has " << clusters[0].indices.size () << " points." << std::endl;
        std::cout << "These are the indices of the points of the initial" <<
        std::endl << "cloud that belong to the first cluster:" << std::endl;
    } else {
        return false;
    }
    unsigned int i=0;
    pcl::PCDWriter _w;
    pcl::PointCloud<pcl::PointXYZ>::Ptr c (new pcl::PointCloud<pcl::PointXYZ>);

    extract.setInputCloud (cloud_filtered);
    extract.setNegative (false);

    while (i < clusters.size() ) {
        std::cout << "cluster " << i << " has " << clusters[i].indices.size() << std::endl;
        char buf[64];
        snprintf(buf,sizeof(buf),"clust_%d_%d.pcd", cnt, i);

        // todo: optimize me
        pcl::PointIndices::Ptr p(new pcl::PointIndices);
        *p=    clusters[i];

        extract.setIndices (p);
        extract.filter (*c);

        _w.write (std::string(buf), *c);
        i++;
    }
    cnt++;
    return true;
}

bool SampleConsensus::runEuclidean(pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
Frame& out)
{
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_f(new pcl::PointCloud<pcl::PointXYZ>());
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_filtered (cloud);
    // if (false) {
    //     // Create the filtering object: downsample the dataset using a leaf size of 1cm
    //     pcl::VoxelGrid<pcl::PointXYZ> vg;
    //     cloud_filtered = new pcl::PointCloud<pcl::PointXYZ>;
    //     vg.setInputCloud (cloud);
    //     vg.setLeafSize (0.01f, 0.01f, 0.01f);
    //     vg.filter (*cloud_filtered);
    //     std::cout << "PointCloud after filtering has: " << cloud_filtered->size ()  << " data points." << std::endl; //*
    // }

    // Create the segmentation object for the planar model and set all the parameters
    LOG(debug) << "starting up!";

    pcl::SACSegmentation<pcl::PointXYZ> seg;
    pcl::PointIndices::Ptr inliers (new pcl::PointIndices);
    pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients);
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud_plane (new pcl::PointCloud<pcl::PointXYZ> ());
    pcl::PCDWriter writer;
    seg.setOptimizeCoefficients (true);
    seg.setModelType (pcl::SACMODEL_PLANE);
    seg.setMethodType (pcl::SAC_RANSAC);
    seg.setMaxIterations (100);
    seg.setDistanceThreshold (0.04);

    unsigned int i=0;
    int nr_points = (int) cloud_filtered->size ();


    while (cloud_filtered->size () >  0.1 * nr_points)
    {
        LOG(debug) << "EUCLI segging! " << i;
        // Segment the largest planar component from the remaining cloud
        seg.setInputCloud (cloud_filtered);
        seg.segment (*inliers, *coefficients);
        if (inliers->indices.size () == 0)
        {
            std::cout << "Could not estimate a planar model for the given dataset." << std::endl;
            break;
        }

        // Extract the planar inliers from the input cloud
        pcl::ExtractIndices<pcl::PointXYZ> extract;
        extract.setInputCloud (cloud_filtered);
        extract.setIndices (inliers);
        extract.setNegative (false);

        // Get the points associated with the planar surface
        extract.filter (*cloud_plane);
        std::cout << "PointCloud representing the planar component: " << cloud_plane->size () << " data points." << std::endl;

        char buf[64];
        snprintf(buf,sizeof(buf),"clust_eucl_%d_%d.pcd", cnt, i);
        writer.write (std::string(buf), *cloud_plane);
        i++;

        // Remove the planar inliers, extract the rest
        extract.setNegative (true);
        extract.filter (*cloud_f);
        *cloud_filtered = *cloud_f;
    }

    cnt++;
    return true;
}

bool SampleConsensus::getInputPoints(const Frame& in, pcl::PointCloud<pcl::PointXYZ>::Ptr& cloud)
{
    pcl::RangeImagePlanar::Ptr p;
    matPtr img3d;

    try {
	pcl::PCLPointCloud2Ptr planar;
	planar = boost::any_cast<pcl::PCLPointCloud2Ptr>(in.getData(this->in));
	//     if (*planar != nullptr) {
        pcl::fromPCLPointCloud2(*planar, *cloud);
	//     } else {
	//         LOG(error) << "input cloud is null!";
	//         return false;
	//     }
    } catch(const boost::bad_any_cast &) {
	LOG(warning) <<
	    "Could not cast input - trying to convert from OCV cloud.";

	// 3d mat
	try {
	    matPtr img3d = boost::any_cast<matPtr>(in.getData(this->in));
	    cloud.reset(new pcl::PointCloud<pcl::PointXYZ>(/*width=*/img3d->cols, /*height=*/img3d->rows));
	    float *dptr;

	    for (int y=0; y< img3d->rows; y++) {
		for (int x = 0; x < img3d->cols; x++) {
		    dptr = img3d->ptr<float>(y, x);
		    pcl::PointXYZ& pt = cloud->at(x,y);
		    pt.x = dptr[0];
		    pt.y = dptr[1];
		    pt.z = dptr[2];
		}
	    }
	} catch (const boost::bad_any_cast &) {
	    LOG(warning) << "Could not cast input to matPtr ";

	    // try pointcloud:
	    try {
            cloud = boost::any_cast<pcl::PointCloud<pcl::PointXYZ>::Ptr>(in.getData(this->in));
            LOG(info) << "got cloud size " << cloud->size();
            LOG(info) << __LINE__;
	    } catch (const boost::bad_any_cast &) {
            LOG(warning) << "Could not cast input " << this->in << " to PointCloud<pcl::PointXYZ> "
                << this->in << ", filter  " << id() << " not applied.";
            return false;
	    }
    }
	LOG(debug) << "YUHUU!!!!";
    }
    return true;
}

/******************************************************************************************************
 */

boost::property_tree::ptree SampleConsensus::getConfig() const
{
    boost::property_tree::ptree pt;

    pt.put("inputs.cloud", in);

    pt.put("outputs.inliers", inliers);
    pt.put("outputs.outliers", outliers);

    pt.put("options.threshold", threshold);
    pt.put("options.maxIterations", maxIters);

    return pt;
}

void SampleConsensus::updateConfig(const boost::property_tree::ptree &pt) 
{
    using namespace boost::property_tree;

    LOG(debug) << __FUNCTION__ <<  " " << id();
    
    in = pt.get("inputs.cloud", in);

    inliers = pt.get("outputs.inliers", inliers);
    outliers = pt.get("outputs.outliers", outliers);


    outputPC2 = pt.get("options.outputPC2", outputPC2);
    threshold = pt.get("options.threshold", threshold);
    maxIters = pt.get("options.maxIterations", maxIters);

    // for cylinder:
    minRadius = pt.get("options.minRadius", minRadius);
    maxRadius = pt.get("options.maxRadius", maxRadius);

    // for regionGrowing:
    smoothnessThresh = pt.get("options.smoothnessThresh", smoothnessThresh);
    curvatureThresh = pt.get("options.curvatureThresh", curvatureThresh);
    // or use degrees..?
    // if (pt.find("options.smoothnessThreshDeg") != pt.end()) {
    //     smoothnessThresh = pt.get("options.smoothnessThreshDeg", 5.0) / 180.0 * M_PI;
    // }

    std::string sType = pt.get("options.type", "plane");
    if ( sType == "plane" ) {
        type = plane;
    } else     if ( sType == "cylinder" ) {
        type = cylinder;
    } else     if ( sType == "line" ) {
        type = line;
    } else     if ( sType == "regionGrowing" ) {
        type = regionGrowing;
    } else     if ( sType == "euclidean" ) {
        type = euclidean;
    } else {
    	LOG(error) << "unknown sample type! " << sType << " sticking to " << type;
    }
    LOG(debug) << "set up for " << sampleTypeNames[type] << 
        " iters " << maxIters << " thresh " << threshold
        << " outputPC2? " << outputPC2;
}
