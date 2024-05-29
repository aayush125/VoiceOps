#pragma once
#include <fstream>
#include <iostream>
#include <vector>

bool savePicture(const std::vector<unsigned char>& pictureData, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(pictureData.data()), pictureData.size());
    file.close();

    return true;
}