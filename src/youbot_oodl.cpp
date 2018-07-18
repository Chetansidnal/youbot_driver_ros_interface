/******************************************************************************
 * Copyright (c) 2011
 * Locomotec
 *
 * Author:
 * Sebastian Blumenthal
 *
 *
 * This software is published under a dual-license: GNU Lesser General Public
 * License LGPL 2.1 and BSD license. The dual-license implies that users of this
 * code may choose which terms they prefer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * * Neither the name of Locomotec nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License LGPL as
 * published by the Free Software Foundation, either version 2.1 of the
 * License, or (at your option) any later version or the BSD license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License LGPL and the BSD license for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License LGPL and BSD license along with this program.
 *
 ******************************************************************************/

#include "youbot_driver_ros_interface/YouBotOODLWrapper.h"
#include <diagnose/diaglib.hpp>  //added diagnose

int main(int argc, char **argv)
{
	ros::init(argc, argv, "youbot_oodl_driver");
	ros::NodeHandle n;

	string config_json;
	n.getParam("config_json", config_json);
	std::cout<<"file path read .........-----"<<config_json<<endl;
	diaglib prodiag;
	prodiag.start_publishing("Node Has started", config_json);
	
  	youbot::Logger::toConsole = false;
  	youbot::Logger::toFile = false;
  	youbot::Logger::toROS = true;
	prodiag.update("S2-000-001", "Node has started ");
	
	youBot::YouBotOODLWrapper youBot(n);
	std::vector<std::string> armNames;


	/* configuration */
	bool youBotHasBase;
	bool youBotHasArms;
	double youBotDriverCycleFrequencyInHz;	//the driver recives commands and publishes them with a fixed frequency
	n.param("youBotHasBase", youBotHasBase, true);
	n.param("youBotHasArms", youBotHasArms, true);
	n.param("youBotDriverCycleFrequencyInHz", youBotDriverCycleFrequencyInHz, 50.0);
	n.param<std::string>("youBotConfigurationFilePath", youBot.youBotConfiguration.configurationFilePath, mkstr(YOUBOT_CONFIGURATIONS_DIR));
	n.param<std::string>("youBotBaseName", youBot.youBotConfiguration.baseConfiguration.baseID, "youbot-base");


	// Retrieve all defined arm names from the launch file params
	int i = 1;
	std::stringstream armNameParam;
	armNameParam << "youBotArmName" << i; // youBotArmName1 is first checked param... then youBotArmName2, etc.
	while (n.hasParam(armNameParam.str())) {
		prodiag.update("S2-000-002", "Retrieve all defined arm names from the launch file params");
		std::string armName;
		n.getParam(armNameParam.str(), armName);
		armNames.push_back(armName);
		armNameParam.str("");
		armNameParam << "youBotArmName" <<  (++i);
	}

    ros::ServiceServer reconnectService = n.advertiseService("reconnect", &youBot::YouBotOODLWrapper::reconnectCallback, &youBot);
	std::string rosinfo_string=youBot.youBotConfiguration.configurationFilePath.c_str();
	rosinfo_string = "Configuration file path:"+ rosinfo_string; //std::str(youBot.youBotConfiguration.configurationFilePath.c_str());
	prodiag.update("S2-000-003", rosinfo_string);
	ROS_INFO("Configuration file path: %s", youBot.youBotConfiguration.configurationFilePath.c_str());
	try {
		youbot::EthercatMaster::getInstance("youbot-ethercat.cfg", youBot.youBotConfiguration.configurationFilePath);
	} catch (std::exception& e)	{
		rosinfo_string = e.what();
		rosinfo_string = "No EtherCAT connection:"+ rosinfo_string;// std::str(e.what());
		prodiag.update("E2-000-004", rosinfo_string);
		ROS_ERROR("No EtherCAT connection:");
		ROS_FATAL("%s", e.what());
		return 0;
	}
	prodiag.update("S2-000-005", "checking if youBotHasBase or youBotHasArms  ");
    ROS_ASSERT((youBotHasBase == true) || (youBotHasArms == true)); // At least one should be true, otherwise nothing to be started.
    if (youBotHasBase == true)
    {
        youBot.initializeBase(youBot.youBotConfiguration.baseConfiguration.baseID);
		prodiag.update("S2-000-006", " Base Initialized ");
    }
	else {prodiag.update("E2-000-006", " Base not Initialized "); }

	if (youBotHasArms == true) {
		std::vector<std::string>::iterator armNameIter;
		for (armNameIter = armNames.begin(); armNameIter != armNames.end(); ++armNameIter) {
			youBot.initializeArm(*armNameIter);
		}
		prodiag.update("S2-000-007", " Arm Initialized ");
	}
	else {prodiag.update("E2-000-007", " ARM not Initialized "); }
 

    /* coordination */
    ros::Rate rate(youBotDriverCycleFrequencyInHz); //Input and output at the same time... (in Hz)
    while (n.ok())
    {
        ros::spinOnce();
        youBot.computeOODLSensorReadings();
        youBot.publishOODLSensorReadings();
        youBot.publishArmAndBaseDiagnostics(2.0);    //publish only every 2 seconds
        rate.sleep();
		// prodiag.update("S2-000-008", " compute and publish ODDLs sensor readings; publish arm,base diagnositics");
    }

    youBot.stop();
	prodiag.update("S2-000-009", " YouBot stopped  ");
    return 0;
}

