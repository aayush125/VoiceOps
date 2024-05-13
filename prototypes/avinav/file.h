#pragma once
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define MAX_PACKET_SIZE 1500

enum PacketType {
	PACKET_TYPE_STRING = 1,
	PACKET_TYPE_TEXT = 2,
	PACKET_TYPE_PICTURE = 3
};

struct Packet {
	uint32_t packetType;
	uint32_t length;
	char data[MAX_PACKET_SIZE];
};

std::vector<unsigned char> loadPicture(const std::string& fileName) {
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << fileName << std::endl;
		return {};
	}

	std::streamsize fileSize = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<unsigned char> pictureData(fileSize);
	file.read(reinterpret_cast<char*>(pictureData.data()), fileSize);

	file.close();

	return pictureData;
}

