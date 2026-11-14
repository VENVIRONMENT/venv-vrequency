// venv-vrequency — audio input: a short recording for the "listen" feature.
#pragma once
#include <string>
#include <vector>

// Record mono audio from the default input.
// Returns frames captured; 0 with `err` set when nothing could be recorded.
int captureMono(std::vector<float> &dst, int rate, double seconds, std::string &err);
