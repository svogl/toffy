/*
   Copyright 2023 Simon Vogl <simon@voxel.at>

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

#include <boost/foreach.hpp>
#include <boost/log/trivial.hpp>

#if OCV_VERSION_MAJOR >= 3
#include <opencv2/calib3d.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#else
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#endif

#include <pcl/conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
// #include <pcl/PCLPointCloud2.h>

#include <toffy/3d/xyz2pcl.hpp>
#include <toffy/filter_helpers.hpp>

using namespace cv;
using namespace std;

using namespace toffy;
using namespace toffy::filters::f3d;
using namespace boost::log::trivial;

boost::property_tree::ptree Xyz2Pcl::getConfig() const {
  BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " " << id();
  boost::property_tree::ptree pt;

  pt = Filter::getConfig();

  pt.put("options.amplitudes", amplitudes);
  pt.put("options.pcl2", pcl2);
  pt.put("options.min", min);
  pt.put("options.max", max);

  pt.put("outputs.cloud", out_cloud);

  return pt;
}

void Xyz2Pcl::updateConfig(const boost::property_tree::ptree& pt) {
  using namespace boost::property_tree;

  Filter::updateConfig(pt);

  pt_optional_get_default(pt, "options.amplitudes", amplitudes, false);
  pt_optional_get_default(pt, "options.pcl2", pcl2, pcl2);
  pt_optional_get_default(pt, "options.min", min, min);
  pt_optional_get_default(pt, "options.max", max, max);
  pt_optional_get_default(pt, "outputs.cloud", out_cloud, out_cloud);
}

bool Xyz2Pcl::filter(const Frame& in, Frame& out) {
  BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " " << id();
  // try {
  {
    matPtr x = in.getMatPtr("x");
    matPtr y = in.getMatPtr("y");
    matPtr z = in.getMatPtr("z");

    // if (! out.hasKey(out_cloud)) {
    //     pcl::PointCloud<P>::Ptr cloud(new pcl::PointCloud<P>);
    //     out.addData(out_cloud, cloud);
    // }

    if (amplitudes) {
      BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " A!  " << id();
      matPtr a = in.getMatPtr("amplitudes");
      convertXyzA(in, out, x, y, z, a);
    } else {
      BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " nA!  " << id();
      convertXyz(in, out, x, y, z);
    }
  }
  // } catch () {
  //     return false;
  // }
  return true;
}

bool Xyz2Pcl::convertXyz(const Frame& in, Frame& out, toffy::matPtr mx,
                         toffy::matPtr my, toffy::matPtr mz) {
  typedef pcl::PointXYZ P;
  pcl::PointCloud<P>::Ptr cloud(new pcl::PointCloud<P>(mx->cols, mx->rows));
  pcl::PCLPointCloud2::Ptr p;
  for (int y = 0; y < mx->rows; y++) {
    short* px = mx->ptr<short>(y, 0);
    short* py = my->ptr<short>(y, 0);
    short* pz = mz->ptr<short>(y, 0);
    for (int x = 0; x < mx->cols; x++) {
      P& p = cloud->at(x, y);

      p.x = (*px++) / 1000.f;
      p.y = (*py++) / 1000.f;
      p.z = (*pz++) / 1000.f;
    }
  }
  if (pcl2) {
    pcl::PCLPointCloud2* cloud_filtered = new pcl::PCLPointCloud2();
    pcl::PCLPointCloud2Ptr cloud2(cloud_filtered);
    // cloud2.reset(new pcl::PointCloud<pcl::PointXYZ>(/*width=*/mx->cols,
                                                    // /*height=*/mx->rows));

    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " C2!  " << id();
    pcl::fromPCLPointCloud2(*cloud2, *cloud);

    out.addData(out_cloud, cloud2);
  } else {
    // cloudPtr..:
    BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " C!  " << id();
    out.addData(out_cloud, cloud);
  }
  return true;
}

bool Xyz2Pcl::convertXyzA(const Frame& in, Frame& out, toffy::matPtr mx,
                          toffy::matPtr my, toffy::matPtr mz,
                          toffy::matPtr ampl) {
  typedef pcl::PointXYZRGB P;
  pcl::PointCloud<P>::Ptr cloud(new pcl::PointCloud<P>);
  for (int y = 0; y < mx->rows; y++) {
    short* px = mx->ptr<short>(y, 0);
    short* py = my->ptr<short>(y, 0);
    short* pz = mz->ptr<short>(y, 0);
    short* pa = ampl->ptr<short>(y, 0);
    for (int x = 0; x < mx->cols; x++) {
      unsigned char a = ((*pa++) - min) / (max - min);
      P p((*px++) / 1000.f, (*py++) / 1000.f, (*pz++) / 1000.f);
      cloud->push_back(p);
    }
  }
  out.addData(out_cloud, cloud);
  return true;

  return true;
}