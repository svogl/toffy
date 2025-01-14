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

#include "toffy/frame.hpp"

#include <boost/log/trivial.hpp>

using namespace toffy;

Frame::Frame() : data(), meta() {}

Frame::Frame(const Frame& f) : data(f.data), meta(f.meta) {}

Frame::~Frame()
{
    data.clear();
    meta.clear();
}

bool Frame::hasKey(std::string key) const
{
    if (data.find(key) != data.end()) return true;
    return false;
}

boost::any Frame::getData(const std::string& key) const
{
    // BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << ", key: " << key;
    boost::any out;
    if (data.find(key) != data.end()) {
        try {
            out = data.at(key);
        } catch (std::out_of_range& e) {
            BOOST_LOG_TRIVIAL(warning)
                << "Frame::getData(): Could not find key " << key;
        }
    } else {
        BOOST_LOG_TRIVIAL(info)
            << "Frame::getData(): Could not find key " << key;
    }
    return out;
}

void Frame::addData(std::string key, boost::any v, SlotDataType dt)
{
    data[key] = v;
    meta[key] = dt;
}

void Frame::addData(std::string key, boost::any v, SlotDataType dt,
                    const std::string& description)
{
    data[key] = v;
    meta[key] = dt;
    desc[key] = description;
}

bool toffy::Frame::removeData(std::string key)
{
    if (data.find(key) != data.end()) {
        data.erase(data.find(key));
        return true;
    } else
        return false;
}

void Frame::clearData() { return data.clear(); }


void Frame::info(std::vector<SlotInfo>& fields) const
{
    auto it = data.begin();
    while (it != data.end()) {
        Frame::SlotInfo si;
        si.key = it->first;
        si.dt = getDataType(si.key);
        si.description = getDescription(si.key);
        fields.push_back(si);
        it++;
    }
}
