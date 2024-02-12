/**
  ******************************************************************************
  * @file   main.cpp
  * @author Alejandro Hontanilla Belinchón (a.hontanillab@alumnos.upm.es)
  * @brief  Main program and Display writing functions.
  *
  * @note   End-of-degree work.
  ******************************************************************************
*/

#include <thread>
#include <iostream>
#include <atomic>
#include <queue>
#include <mutex>
#include <csignal>
#include <mqtt/client.h>
#include <mqtt/async_client.h>
#include <json/json.h>
#include <termios.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>
#include "thread_queue.h"

std::string address = "ssl://srv-iot.diatel.upm.es:8883",
		client_name = "98pZ8Y2TOyMnghqLcqKr";

mqtt::async_client client(address, client_name);

typedef struct{
	float temperatureMax;
	float temperatureMin;
	float temperatureMean;
	int humidityMean;
	int humidityMax;
	int humidityMin;
	long lightMax;
	long lightMin;
	long lightMean;
	float ph;
	int tds;
	int water_lev;
	int red;
	int green;
	int blue;
} values;

typedef struct{
	int lightAct;
	int humidIsOn;
	int airCondIsOn;
	int valveIsOpen;
	int phControllerIsOn;
} actuators_t;


#define SERVER_ADDR "192.168.50.246"

const uint8_t humid_act = 0xFD;
const uint8_t temp_act = 0xFC;
const uint8_t waterlev_act = 0xFB;
const uint8_t tds_act = 0xFA;
const uint8_t ph_act = 0xF9;
const uint8_t light_act = 0xF8;

char msg_rec[5000];
int socket_fd;
struct sockaddr_in clients[255];
struct sockaddr_in server_addr, client_addr;
Thread_queue<values> val_q;
Thread_queue<int> msg_rec_notif;
Thread_queue<actuators_t> act_q;
int client_count = 0;

using namespace std;


void mqtt_thread();
void udp_thread();
void signalHandler( int signum );

int main() {

	signal(SIGINT, signalHandler);

	std::thread mqtt_thread_ctrl(mqtt_thread);
	std::thread udp_thread_ctrl(udp_thread);

	mqtt_thread_ctrl.join();
	udp_thread_ctrl.join();

	return 0;
}

void message_callback(mqtt::const_message_ptr msg) {
	std::cout << "[MQTT] Message received: " << msg->get_payload_str() << std::endl;

	Json::Value root;
	Json::Reader reader;
	if(!reader.parse(msg->get_payload_str(), root)){
		cerr << "Error parsing the JSON" << endl;
	}
	Json::FastWriter fastWriter;
	std::string method = fastWriter.write(root["method"]);
	std::string comparation = "\"setwaterLevel\"\n";

	if(method.compare("\"setwaterLevel\"\n") == 0 ){
		cout << "Setting water actuator to:" << root["params"]["mode"] << endl;
		uint8_t packet[] = {waterlev_act,root["params"]["mode"].asInt()};
		socklen_t len;

		len = sizeof(client_addr);
		for(int i=0; i < client_count; i++){
			sendto(socket_fd, (char *) packet, 2, MSG_CONFIRM, (const struct sockaddr*) &clients[i], len);
		}

	}else if(method.compare("\"setTDS\"\n") == 0){
		cout << "Setting TDS actuator to: " << root["params"]["mode"] << endl;
		uint8_t packet[] = {tds_act,root["params"]["mode"].asInt()};
		socklen_t len;

		len = sizeof(client_addr);
		for(int i=0; i < client_count; i++){
			sendto(socket_fd, (char *) packet, 2, MSG_CONFIRM, (const struct sockaddr*) &clients[i], len);
		}
	}else if(method.compare("\"setpH\"\n") == 0){
		cout << "Setting PH actuator to: " << root["params"]["mode"] << endl;
		uint8_t packet[] = {ph_act,root["params"]["mode"].asInt()};
		socklen_t len;

		len = sizeof(client_addr);
		sendto(socket_fd, (char *) packet, 2, MSG_CONFIRM, (const struct sockaddr*) &client_addr, len);
	}else if(method.compare("\"setTemperature\"\n") == 0){
		cout << "Setting Temperature actuator to: " << root["params"]["mode"] << endl;
		uint8_t packet[] = {temp_act,root["params"]["mode"].asInt()};
		socklen_t len;

		len = sizeof(client_addr);
		for(int i=0; i < client_count; i++){
			sendto(socket_fd, (char *) packet, 2, MSG_CONFIRM, (const struct sockaddr*) &clients[i], len);
		}
	}else if(method.compare("\"setHumidity\"\n") == 0){
		cout << "Setting Humidity actuator to: " << root["params"]["mode"] << endl;
		uint8_t packet[] = {humid_act,root["params"]["mode"].asInt()};
		socklen_t len;

		len = sizeof(client_addr);
		for(int i=0; i < client_count; i++){
			sendto(socket_fd, (char *) packet, 2, MSG_CONFIRM, (const struct sockaddr*) &clients[i], len);
		}
	}else if(method.compare("\"setLight\"\n") == 0){
		cout << "Setting Light actuator to: " << root["params"]["mode"] << endl;
		uint8_t packet[] = {light_act,root["params"]["mode"].asInt()};
		socklen_t len;

		len = sizeof(client_addr);
		for(int i=0; i < client_count; i++){
			sendto(socket_fd, (char *) packet, 2, MSG_CONFIRM, (const struct sockaddr*) &clients[i], len);
		}
	}else{
		cout << "RPC Not recognised" << endl;
	}


}

void mqtt_thread() {

	auto sslopts = mqtt::ssl_options_builder()
						   .error_handler([](const std::string& msg) {
							   std::cerr << "SSL Error: " << msg << std::endl;
						   }).enable_server_cert_auth(false)
						   .finalize();

	auto conOps =
			mqtt::connect_options_builder().user_name(client_name).connect_timeout(
					std::chrono::seconds(2)).keep_alive_interval(
					std::chrono::milliseconds(500)).clean_session(true).ssl(std::move(sslopts)).finalize();

	client.set_message_callback(message_callback);

	bool connected = false;
	Json::Value telemetry_object;
	Json::FastWriter fastWriter;
	std::string msg;
	int actuators;

	while (true) {

		//Wait until ordered to connect
		actuators = 0;
		//Connect
		if (!connected) {

			if (client.connect(conOps)->wait_for(1000)) {
				std::cout << "Connected Successfully" << std::endl;
				client.subscribe("v1/devices/me/rpc/request/+", 0);
				connected = true;
			} else {
				std::cerr << "Error in connection" << std::endl;
			}

		}

		if (connected) {

			actuators = msg_rec_notif.pop(2);

			if(actuators == 1){
				//Send data
				values val = val_q.back_clear();

				std::string msg = "";

				msg = "{\"temperature_mean\": " + std::to_string(val.temperatureMean) + ",\"temperature_max\": " + std::to_string(val.temperatureMax) + ",\"temperature_min\": " + std::to_string(val.temperatureMin) + ",\"humidity_mean\": " + std::to_string(val.humidityMean) + ",\"humidity_min\": " + std::to_string(val.humidityMin) + ",\"humidity_max\": " + std::to_string(val.humidityMax) + ",\"lightMin\": " + to_string(val.lightMin) + ",\"lightMax\": " + to_string(val.lightMax) + ",\"lightMean\": " + to_string(val.lightMean) + ",\"ph\": " + std::to_string(val.ph) +",\"tds\": " + std::to_string(val.tds) + ",\"waterLevel\": " + std::to_string(val.water_lev) + ",\"RGB_Red\":" + std::to_string(val.red) + ",\"RGB_Green\":" + std::to_string(val.green) +",\"RGB_Blue\": " + std::to_string(val.blue) + "}";

				std::cout << "Sending data..." << msg << std::endl;
				client.publish("v1/devices/me/telemetry", msg);

			}else if(actuators == 2){
				actuators_t act = act_q.back_clear();
				std::string msg = "";

				msg = "{\"light\": " + std::to_string(act.lightAct) + ",\"air_cond\": " + std::to_string(act.airCondIsOn) + ",\"humidifier\": " + std::to_string(act.humidIsOn) + ",\"ph_control\": " + std::to_string(act.phControllerIsOn) + ",\"valve\": " + std::to_string(act.valveIsOpen) + "}";

				std::cout << "Sending data..." << msg << std::endl;
				client.publish("v1/devices/me/telemetry", msg);

			}

		}

		std::this_thread::sleep_for(std::chrono::seconds(1));

	}
}

void udp_thread(){


	socket_fd = socket(AF_INET, SOCK_DGRAM, 0);

	ofstream myfile("/var/log/messages_received_udp.txt");

	//Create a socket
	if(socket_fd == -1){
		cout << "Could not create socket" << endl;
		return;

	} else {
		cout << "Socket creation successful!" << endl;
	}

	memset(&server_addr,0,sizeof(server_addr));
	memset(&client_addr,0,sizeof(server_addr));

	//Configure the Server
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
	server_addr.sin_port = htons(9999);

	//Bind the IP Address
	int bind_result = bind(socket_fd, (const struct sockaddr*) &server_addr, sizeof(server_addr));

	if(bind_result == -1){

		cout << "Error binding the address" << endl;
		close(socket_fd);
		return;

	} else {
		cout << "Binding successful!" << endl;
	}

	socklen_t len;

	len = sizeof(client_addr);

	int i = 0;
	float tempMax = -5000, tempMin = 500, tempMean = 0;
	int humidMax = 0, humidMean = 0, humidMin = 100;
	long lightMax = 0, lightMean = 0, lightMin = 100000;

	while(true){
		int size = recvfrom(socket_fd, (char *)msg_rec, 5000, MSG_WAITALL, (struct sockaddr *) &client_addr, &len);

		bool addClient = true;
		for(int i = 0; i < 255; i++){
			if(client_addr.sin_addr.s_addr == clients[i].sin_addr.s_addr){
				cout << "Client found" << endl;
				addClient = false;
			}
		}
		if(addClient){
			cout << "Client Not Found" << endl;
			clients[client_count++] = client_addr;
		}

		msg_rec[size] = '\0';

		cout << "[UDP] Message received: " << msg_rec << endl;

		time_t now = time(0); // get current dat/time with respect to system.
		char* dt = ctime(&now); // convert it into string.

		myfile << "[UDP] Message received" << ": " << msg_rec << " at " << dt << endl;

		cout << "First character: " << hex << (int) msg_rec[0] << endl;

		if(msg_rec[0] == 0x2a){
			char dummy;
			actuators_t acts;

			cout << "Sendign actuator info" << endl;

			sscanf(msg_rec,"%c %d %d %d %d %d", &dummy, &(acts.lightAct),&(acts.humidIsOn), &(acts.airCondIsOn), &(acts.valveIsOpen), &(acts.phControllerIsOn));



			act_q.push(acts);

			msg_rec_notif.push(2);

		}else{
			float templ;
			int humidl;
			long lightl;

			values result;

			sscanf(msg_rec,"%ld %d,%d,%d %d %f %d %d %f", &(lightl), &(result.red), &(result.green), &(result.blue), &(humidl), &(templ), &(result.water_lev), &(result.tds), &(result.ph));

			tempMean += templ;
			if(templ > tempMax)
				tempMax = templ;
			if(templ < tempMin)
				tempMin = templ;

			humidMean += humidl;
			if(humidl < humidMin)
				humidMin = humidl;
			if(humidl > humidMax)
				humidMax = humidl;

			lightMean += lightl;
			if(lightl < lightMin)
				lightMin = lightl;
			if(lightl > lightMax)
				lightMax = lightl;

			i++;

			if(i==3){
				cout << std::to_string(tempMean) << endl;
				result.temperatureMax = tempMax;
				result.temperatureMin = tempMin;
				result.temperatureMean = tempMean / i;
				result.humidityMax = humidMax;
				result.humidityMin = humidMin;
				result.humidityMean = humidMean / i;
				result.lightMax = lightMax;
				result.lightMin = lightMin;
				result.lightMean = lightMean;
				val_q.push(result);
				msg_rec_notif.push(1);
				i = 0;
				tempMax = -5000, tempMin = 500, tempMean = 0;
				humidMax = 0, humidMean = 0, humidMin = 100;
			}
		}
	}
}

void signalHandler( int signum ) {

	printf("Ending program\n");
	client.disconnect(2000);

	close(socket_fd);
	cout << "The file has been closed successfully!" << endl;
	exit(0);

}
