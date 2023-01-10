#include "variables.h"

namespace base {
	std::map<std::string, std::string> GlobVars::list_of_params;
	std::string GlobVars::proxy_host = "", 
				GlobVars::user_agent = "";
	int  GlobVars::device_memory = 0;
	bool GlobVars::battery_charging = true;
	bool GlobVars::file_exists = false;
	double GlobVars::battery_percentage = 0.0,
		   GlobVars::battery_charging_time = 0.0, 
		   GlobVars::battery_discharging_time = 0.0,
		   GlobVars::dom_complete = 0.0,
		   GlobVars::load_event_end = 0.0;

	std::string GlobVars::getExecutableDirectory() {
		char buffer[MAX_PATH];
        GetModuleFileNameA(NULL, buffer, MAX_PATH);
        std::string::size_type pos = std::string(buffer).find_last_of("\\/");
		return std::string(buffer).substr(0, pos).append("\\config.ini");
	}

	bool GlobVars::FileExists(std::string location) {
		std::string predefined_file_location;
		if (location == "") {
			predefined_file_location = getExecutableDirectory();
		} else {
			predefined_file_location = location;
		}
		FILE *file_pointer = fopen(predefined_file_location.c_str(), "r");
		if (file_pointer != NULL) {
			fclose(file_pointer);
			return true;
		}
		return false;
	}

	bool GlobVars::updateVars() {
		std::string filepath = getExecutableDirectory();
		std::ifstream config_file(filepath);
		
		if (config_file.is_open()) {
			std::string input, key, value;
			while (std::getline(config_file, input)) {	
				std::istringstream iss(input);				
				std::getline(iss, key, '=');
				std::getline(iss, value);
				GlobVars::list_of_params.insert(std::pair<std::string, std::string>(key, value));
				// if (key == "ProxyHost") GlobVars::proxy_host = value;
				// if (key == "UserAgent") GlobVars::user_agent = value;
				// if (key == "DeviceMemory") GlobVars::device_memory = stoi(value);
				// if (key == "BatteryCharging") value == "true" ? GlobVars::battery_charging = true : GlobVars::battery_charging = false;
				// if (key == "BatteryChargeTime") GlobVars::battery_charging_time = stod(value);
				// if (key == "BatteryDischargingTime") GlobVars::battery_discharging_time = stod(value);
				// if (key == "BatteryStatus") GlobVars::battery_percentage = stod(value);
				// if (key == "DomComplete")	GlobVars::dom_complete = stod(value);
				// if (key == "LoadEventEnd")	GlobVars::load_event_end = stod(value);
			}
		}
		return false;
	}

	std::string GlobVars::getProxyHost(){
		return proxy_host;
	}

	std::string GlobVars::getUserAgent(){
		return user_agent;
	}
	
	int GlobVars::getDevMem(){
		return std::atoi(list_of_params["DeviceMemory"].c_str());
		//return device_memory;
	}
	
	bool GlobVars::getBatteryChargingStatus(){
		return battery_charging;
	}

	double GlobVars::getBatteryChargeTime(){
		return battery_charging_time;
	}

	double GlobVars::getBatteryDischargeTime(){
		return battery_discharging_time;
	}

	double GlobVars::getBatteryStatus(){
		return battery_percentage;
	}

	double GlobVars::getDomComplete(){
		return dom_complete;
	}

	double GlobVars::getLoadEventEnd(){
		return load_event_end;
	}

} // namespace base