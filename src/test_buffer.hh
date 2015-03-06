#include <iostream>
#include <fstream>
#include <sstream>
#include <valarray>
#include <limits>
#include <cmath>
#include <complex>
#include <ostream>
#include <functional>
#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <random>

using namespace factory;

template<class T>
void test_buffer() {

	std::default_random_engine generator;
	std::uniform_int_distribution<T> distribution(std::numeric_limits<T>::min(),std::numeric_limits<T>::max());
	auto dice = std::bind(distribution, generator);

	const int32_t CHUNK_SIZE = 13;
	const int32_t MAX_SIZE_POWER = 12;

	for (int32_t i=0; i<=MAX_SIZE_POWER; ++i) {
		int32_t size = 2 << i;
		std::vector<T> input(size);
		for (int32_t j=0; j<size; ++j)
			input[j] = dice();
		Buffer<T> buf(CHUNK_SIZE);
		if (!buf.empty()) {
			std::stringstream msg;
			msg << std::string(__func__) + ". Buffer is not empty before write. ";
			msg << buf.size();
			throw std::runtime_error(msg.str());
		}
		buf.write(&input[0], input.size());
		if (buf.size() != input.size()) {
			std::stringstream msg;
			msg << std::string(__func__) + ". Buffer size is not equal to input size. ";
			msg << buf.size() << " != " << input.size();
			throw std::runtime_error(msg.str());
		}
		std::vector<T> output(size);
		buf.read(&output[0], output.size());
		if (!buf.empty()) {
			std::stringstream msg;
			msg << std::string(__func__) + ". Buffer is not empty after read. ";
			msg << buf.size();
			throw std::runtime_error(msg.str());
		}
		for (int32_t j=0; j<size; ++j)
			if (input[j] != output[j])
				throw std::runtime_error(std::string(__func__) + ". Input and output does not match.");
	}
}

struct App {
	int run(int, char**) {
		try {
			test_buffer<char>();
			test_buffer<unsigned char>();
		} catch (std::exception& e) {
			std::cerr << e.what() << std::endl;
			return 1;
		}
		return 0;
	}
};