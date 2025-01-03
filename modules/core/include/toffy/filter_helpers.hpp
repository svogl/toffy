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
#pragma once

/** misc helper functions for implementing filters. 
 * Include this in your cpp file, not in the header.
 */

 
#include <arpa/inet.h> // inet_aton

#include <boost/property_tree/xml_parser.hpp>
#include <boost/log/trivial.hpp>

// for silencing warnings of unused parameters, use UNUSED(param); as in:
#define UNUSED(expr) do { (void)(expr); } while (0)

/* enable nice(r) logging by providing a short-hand version
 */
#define LOG(lvl)    BOOST_LOG_TRIVIAL(lvl) << id() << "::" << __FUNCTION__ << "() : "

/** optionally get a value from the property tree if it exists.
 * @return true if key exists and the value has been set, false otherwise
 */
template<typename T> bool pt_optional_get(const boost::property_tree::ptree pt,
    const std::string& key, T& val) 
{
    boost::optional<T> opt = pt.get_optional<T>(key);
    if (opt.is_initialized()) {
        val = *opt;
        return true;
    } else {
        // BOOST_LOG_TRIVIAL(debug) << "pt_optional_get - key not set: " << key;
    }
    return false;
}

/** optionally get a value from the property tree if it exists.
 * @return true if key exists and the value has been set, false otherwise
 */
template<typename T> bool pt_optional_get_default(const boost::property_tree::ptree pt,
    const std::string& key, T& val, const T& defaultValue) 
{
    boost::optional<T> opt = pt.get_optional<T>(key);
    if (opt.is_initialized()) {
        val = *opt;
        return true;
    }
    val = defaultValue;
    return false;
}

static inline bool pt_optional_get_ipaddr(const boost::property_tree::ptree pt,
    const std::string& key, struct in_addr& inaddr, std::string defaultAddress) 
{
    boost::optional<std::string> opt = pt.get_optional<std::string>(key);
    std::string addr = defaultAddress;
    if (! opt.is_initialized() && defaultAddress.size() == 0) {
        return false;
    }
    if (opt.is_initialized()) {
        addr = *opt;
    }
    int success = inet_aton( addr.c_str(), &inaddr);
    if (!success) {
        BOOST_LOG_TRIVIAL(warning) << "pt_optional_get_ipaddr() could not parse entry " << key << " : " << addr;
    } else {
        BOOST_LOG_TRIVIAL(info) << "pt_optional_get_ipaddr set " << key << " " << addr;
    }
    return success == 1;
}
