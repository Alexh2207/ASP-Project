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
	float temperature;
	int humidity;
	long light;
	float ph;
	int tds;
	int water_lev;
	int red;
	int green;
	int blue;
} values;

#define SERVER_ADDR "192.168.1.146"
char msg_rec[5000];
int socket_fd;
struct sockaddr_in server_addr, client_addr;
Thread_queue<values> val_q;

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
	std::cout << "MSG_RECEIVED: " << msg->get_payload_str() << std::endl;

	ofstream mqtt_log("/var/log/mqtt_log.txt");

	time_t now = time(0); // get current dat/time with respect to system.
	char* dt = ctime(&now); // convert it into string.

	mqtt_log << "[MQTT] Message received" << ": " << msg->get_payload_str() << " at " << dt << endl;
	Json::Value root;
	Json::Reader reader;
	if(!reader.parse(msg->get_payload_str(), root)){
		cerr << "Error parsing the JSON" << endl;
	}

	mqtt_log.close();

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

	while (true) {

		//Wait until ordered to connect

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

			//Send data
			values val = val_q.back_clear();

			std::string msg = "";

			msg = "{\"temperature\": " + std::to_string(val.temperature) + ",\"humidity\": " + std::to_string(val.humidity) + ",\"light\": " + to_string(val.light) + ",\"ph\": " + std::to_string(val.ph) +",\"tds\": " + std::to_string(val.tds) + ",\"waterLevel\": " + std::to_string(val.water_lev) + ",\"RGB_Red\":" + std::to_string(val.red) + ",\"RGB_Green\":" + std::to_string(val.green) +",\"RGB_Blue\": " + std::to_string(val.blue) + "}";

			std::cout << "Sending data..." << msg << std::endl;

			client.publish("v1/devices/me/telemetry", msg);

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

	while(true){
		int size = recvfrom(socket_fd, (char *)msg_rec, 5000, MSG_WAITALL, (struct sockaddr *) &client_addr, &len);

		msg_rec[size] = '\0';

		cout << "Message is: " << msg_rec << endl;

		time_t now = time(0); // get current dat/time with respect to system.
		char* dt = ctime(&now); // convert it into string.

		myfile << "[UDP] Message received" << ": " << msg_rec << " at " << dt << endl;

		values result;

		sscanf(msg_rec,"%ld %d,%d,%d %d %f %d %d %f", &(result.light), &(result.red), &(result.green), &(result.blue), &(result.humidity), &(result.temperature), &(result.water_lev), &(result.tds), &(result.ph));

		val_q.push(result);

	}
}

void signalHandler( int signum ) {

	printf("Ending program\n");
	//client.disconnect(2000);

	close(socket_fd);
	cout << "The file has been closed successfully!" << endl;
	exit(0);

}
