/*
   Copyright 2018 Simon Vogl <svogl@voxel.at>
                  Angel Merino-Sastre <amerino@voxel.at>

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
#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>

#include <toffy/filter.hpp>
#include <toffy/detection/detectedObject.hpp>

/** @defgroup Tracking Tracking
 *
 * Tracking module
 *
 */

namespace toffy {
//! tracking namespace
namespace tracking {

/**
 * @brief distance where we consider an object changed position and it is not
 * a different object.
 *
 * @todo Move to config??
 */
//const double maxMergeDistance = 20.;

/**
 * @brief Object tracker filter
 * @ingroup Tracking
 *
 * For detailed information see \ref tracker_page description page
 *
 */
class CVTracker : public Filter
{
   public:
    static const std::string id_name;  ///< Filter identifier

    CVTracker();
    virtual ~CVTracker() {}

    virtual boost::property_tree::ptree getConfig() const;
    virtual void updateConfig(const boost::property_tree::ptree& pt);

    virtual bool filter(const Frame& in, Frame& out);

    /**
     * @brief getBlobs
     * @return
     *
     * @todo Where is this been called
     */
    const std::vector<toffy::detection::DetectedObject*>& getBlobs() const
    {
        return blobs;
    }

    cv::Ptr<cv::Tracker> tracker;
    cv::Rect2d bbox;

   private:
    std::string _in_vec,   ///< Name of the next list of detected blobs
        _in_fc,            ///< Camera frame counter
        _in_img;           ///< Image from where the new blobs where detected
    std::string _out_img,  ///< Name of the modified image
        _out_objects,      ///< List of tracked objects
        _out_count;        ///< # of detected objects
    double maxMergeDistance;

    static std::size_t _filter_counter;  ///< Internal filter counter

    unsigned int _fc;
    bool _render_image;

    std::vector<toffy::detection::DetectedObject*> blobs;

    /**
     * @brief Debug views
     * @param depth
     *
     */
    void showObjects(cv::Mat& depth);
};

}  // namespace tracking
}  // namespace toffy
