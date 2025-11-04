#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <sstream>
#include <cstring>

class FileStorage {
private:
    std::string data_file;
    std::string index_file;
    std::map<std::string, std::vector<int>> cache;

    void save_data() {
        std::ofstream data_out(data_file, std::ios::binary);
        std::ofstream index_out(index_file, std::ios::binary);

        if (!data_out || !index_out) return;

        size_t total_keys = cache.size();
        index_out.write(reinterpret_cast<const char*>(&total_keys), sizeof(total_keys));

        for (const auto& [key, values] : cache) {
            size_t key_len = key.length();
            index_out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            index_out.write(key.c_str(), key_len);

            size_t value_count = values.size();
            data_out.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));
            for (int value : values) {
                data_out.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }
        }

        data_out.close();
        index_out.close();
    }

    void load_data() {
        std::ifstream data_in(data_file, std::ios::binary);
        std::ifstream index_in(index_file, std::ios::binary);

        if (!data_in || !index_in) return;

        size_t total_keys;
        index_in.read(reinterpret_cast<char*>(&total_keys), sizeof(total_keys));

        for (size_t i = 0; i < total_keys; i++) {
            size_t key_len;
            index_in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

            std::string key(key_len, '\0');
            index_in.read(&key[0], key_len);

            size_t value_count;
            data_in.read(reinterpret_cast<char*>(&value_count), sizeof(value_count));

            std::vector<int> values(value_count);
            for (size_t j = 0; j < value_count; j++) {
                data_in.read(reinterpret_cast<char*>(&values[j]), sizeof(values[j]));
            }

            cache[key] = values;
        }

        data_in.close();
        index_in.close();
    }

public:
    FileStorage(const std::string& base_name = "database")
        : data_file(base_name + ".dat"), index_file(base_name + ".idx") {
        load_data();
    }

    ~FileStorage() {
        save_data();
    }

    void insert(const std::string& index, int value) {
        auto& values = cache[index];

        auto it = std::lower_bound(values.begin(), values.end(), value);
        if (it == values.end() || *it != value) {
            values.insert(it, value);
        }
    }

    void remove(const std::string& index, int value) {
        auto it = cache.find(index);
        if (it == cache.end()) return;

        auto& values = it->second;
        auto vit = std::lower_bound(values.begin(), values.end(), value);
        if (vit != values.end() && *vit == value) {
            values.erase(vit);
            if (values.empty()) {
                cache.erase(it);
            }
        }
    }

    std::vector<int> find(const std::string& index) {
        auto it = cache.find(index);
        if (it == cache.end()) {
            return std::vector<int>();
        }
        return it->second;
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