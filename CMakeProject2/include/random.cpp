#include <random>
#include <ctime>

// Function to generate a random float number
float randomf() {
	// Create a random device as a seed
	static std::random_device rd;

	// Initialize a Mersenne Twister random number generator with the seed
	static std::mt19937 gen(rd());

	// Create a uniform distribution for floats between 0.0 and 1.0
	static std::uniform_real_distribution<float> dis(0.0f, 1.0f);

	// Generate and return a random float
	return dis(gen);
}

// Overloaded function to generate a random float within a specific range
float randomf(float min, float max) {
	// Create a random device as a seed
	static std::random_device rd;

	// Initialize a Mersenne Twister random number generator with the seed
	static std::mt19937 gen(rd());

	// Create a uniform distribution for floats between min and max
	std::uniform_real_distribution<float> dis(min, max);

	// Generate and return a random float within the specified range
	return dis(gen);
}