#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <cstring>

namespace fs = std::filesystem;

class FileStorage {
private:
    std::string base_dir;

    std::string sanitize_filename(const std::string& index) {
        std::string sanitized;
        for (char c : index) {
            if (std::isalnum(c) || c == '_' || c == '-') {
                sanitized += c;
            } else {
                sanitized += '_';
            }
        }
        return sanitized;
    }

    std::string get_file_path(const std::string& index) {
        return base_dir + "/" + sanitize_filename(index) + ".dat";
    }

    std::vector<int> read_values(const std::string& index) {
        std::vector<int> values;
        std::string filepath = get_file_path(index);

        if (!fs::exists(filepath)) {
            return values;
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            return values;
        }

        int count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        values.resize(count);
        for (int i = 0; i < count; i++) {
            file.read(reinterpret_cast<char*>(&values[i]), sizeof(values[i]));
        }

        file.close();
        return values;
    }

    void write_values(const std::string& index, const std::vector<int>& values) {
        std::string filepath = get_file_path(index);

        std::ofstream file(filepath, std::ios::binary);
        if (!file) {
            return;
        }

        int count = values.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (int value : values) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        file.close();
    }

public:
    FileStorage(const std::string& dir = "database") : base_dir(dir) {
        if (!fs::exists(base_dir)) {
            fs::create_directory(base_dir);
        }
    }

    void insert(const std::string& index, int value) {
        std::vector<int> values = read_values(index);

        auto it = std::lower_bound(values.begin(), values.end(), value);
        if (it == values.end() || *it != value) {
            values.insert(it, value);
            write_values(index, values);
        }
    }

    void remove(const std::string& index, int value) {
        std::vector<int> values = read_values(index);

        auto it = std::lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            values.erase(it);
            if (values.empty()) {
                std::string filepath = get_file_path(index);
                if (fs::exists(filepath)) {
                    fs::remove(filepath);
                }
            } else {
                write_values(index, values);
            }
        }
    }

    std::vector<int> find(const std::string& index) {
        return read_values(index);
    }
};

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    FileStorage storage;

    int n;
    std::cin >> n;
    std::cin.ignore();

    for (int i = 0; i < n; i++) {
        std::string line;
        std::getline(std::cin, line);

        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "insert") {
            std::string index;
            int value;
            iss >> index >> value;
            storage.insert(index, value);
        } else if (command == "delete") {
            std::string index;
            int value;
            iss >> index >> value;
            storage.remove(index, value);
        } else if (command == "find") {
            std::string index;
            iss >> index;
            std::vector<int> values = storage.find(index);

            if (values.empty()) {
                std::cout << "null" << std::endl;
            } else {
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) std::cout << " ";
                    std::cout << values[j];
                }
                std::cout << std::endl;
            }
        }
    }

    return 0;
}