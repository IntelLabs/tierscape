#include "utils.h"


// Function to read the file and extract the last integer from each line into a vector
std::vector<int> readLastColumnIntegers(const std::string& filename) {
    std::vector<int> lastColumnValues;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return lastColumnValues; // Return empty vector if file cannot be opened
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int val;
        // Skip the first three integers, read and store the fourth one
        for (int i = 0; i < 3; ++i) {
            if (!(iss >> val)) {
                std::cerr << "Error reading line: " << line << std::endl;
                return lastColumnValues; // Return empty vector on error
            }
        }
        if (iss >> val) {
            lastColumnValues.push_back(val);
        } else {
            std::cerr << "Error reading line: " << line << std::endl;
            return lastColumnValues; // Return empty vector on error
        }
    }

    file.close();
    return lastColumnValues;
}
