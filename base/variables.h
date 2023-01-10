// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VARIABLES_H
#define VARIABLES_H

#include "Windows.h"
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

namespace base {
	class GlobVars {
		private:

			// config vars
			static bool file_exists;

			static std::string proxy_host;
			static std::string user_agent;
			static int device_memory;
			static bool battery_charging;
			static double battery_percentage;
			static double battery_charging_time;
			static double battery_discharging_time;
			static double dom_complete;			
			static double load_event_end;

		public:
			static std::map<std::string, std::string> list_of_params;
			static std::string getProxyHost();
			static std::string getUserAgent();
			static int getDevMem();
			//static bool FileExists();
			static bool FileExists(std::string location);
			static bool getBatteryChargingStatus();
			static double getBatteryChargeTime();
			static double getBatteryDischargeTime();
			static double getBatteryStatus();
			static double getDomComplete();
			static double getLoadEventEnd();

			static std::string getExecutableDirectory();
			static bool updateVars();

			//~GlobVars();
	};

} // namespace base

#endif