#pragma once

#include <iostream>
#include <vector>
#include <windows.h>

std::tuple<std::vector<BYTE>, int, int, int> get_screenshot();

void save_to_file(std::tuple<std::vector<BYTE>, int, int, int>);