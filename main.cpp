#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <cstring>

class FileStorage {
private:
    std::string data_file;
    std::string index_file;
    std::map<std::string, size_t> index_cache;
    std::map<std::string, std::vector<int>> value_cache;
    bool index_loaded;
    size_t data_file_size;

    struct IndexEntry {
        size_t offset;
        size_t count;

        IndexEntry() : offset(0), count(0) {}
        IndexEntry(size_t o, size_t c) : offset(o), count(c) {}
    };

    void load_index() {
        if (index_loaded) return;

        std::ifstream file(index_file, std::ios::binary);
        if (!file) {
            index_loaded = true;
            return;
        }

        size_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        for (size_t i = 0; i < count; i++) {
            size_t key_len;
            file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

            std::string key(key_len, '\0');
            file.read(&key[0], key_len);

            IndexEntry entry;
            file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
            file.read(reinterpret_cast<char*>(&entry.count), sizeof(entry.count));

            index_cache[key] = entry.offset;
        }

        file.close();
        index_loaded = true;
    }

    void save_index() {
        std::ofstream file(index_file, std::ios::binary);
        if (!file) return;

        size_t count = index_cache.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& [key, offset] : index_cache) {
            auto it = value_cache.find(key);
            if (it == value_cache.end()) continue;

            size_t key_len = key.length();
            file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            file.write(key.c_str(), key_len);
            file.write(reinterpret_cast<const char*>(&offset), sizeof(offset));

            size_t value_count = it->second.size();
            file.write(reinterpret_cast<const char*>(&value_count), sizeof(value_count));
        }

        file.close();
    }

    std::vector<int> read_values(size_t offset, size_t count) {
        std::vector<int> values(count);
        if (count == 0) return values;

        std::ifstream file(data_file, std::ios::binary);
        if (!file) return std::vector<int>();

        file.seekg(offset);

        for (size_t i = 0; i < count; i++) {
            file.read(reinterpret_cast<char*>(&values[i]), sizeof(values[i]));
        }

        file.close();
        return values;
    }

    void write_values(size_t offset, const std::vector<int>& values) {
        std::fstream file(data_file, std::ios::in | std::ios::out | std::ios::binary);
        if (!file) {
            std::ofstream create_file(data_file, std::ios::binary);
            create_file.close();
            file.open(data_file, std::ios::in | std::ios::out | std::ios::binary);
            if (!file) return;
        }

        file.seekp(offset);

        for (int value : values) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        file.close();
    }

    void append_values(const std::vector<int>& values, size_t& offset, size_t& count) {
        std::ofstream file(data_file, std::ios::app | std::ios::binary);
        if (!file) return;

        offset = file.tellp();
        count = values.size();

        for (int value : values) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        file.close();
    }

public:
    FileStorage(const std::string& base_name = "database")
        : data_file(base_name + ".dat"), index_file(base_name + ".idx"),
          index_loaded(false), data_file_size(0) {}

    ~FileStorage() {
        save_index();
    }

    void insert(const std::string& index, int value) {
        load_index();

        auto& values = value_cache[index];
        if (values.empty() && index_cache.find(index) != index_cache.end()) {
            size_t offset = index_cache[index];
            auto it = value_cache.find(index);
            if (it != value_cache.end()) {
                values = it->second;
            } else {
                size_t count = 0;
                std::ifstream check_file(data_file, std::ios::binary);
                if (check_file) {
                    check_file.seekg(offset - sizeof(size_t));
                    check_file.read(reinterpret_cast<char*>(&count), sizeof(count));
                    check_file.close();
                }
                values = read_values(offset, count);
            }
        }

        auto it = std::lower_bound(values.begin(), values.end(), value);
        if (it == values.end() || *it != value) {
            values.insert(it, value);

            if (index_cache.find(index) == index_cache.end()) {
                size_t offset, count;
                append_values(values, offset, count);
                index_cache[index] = offset;
            } else {
                size_t offset = index_cache[index];
                write_values(offset, values);
            }
        }
    }

    void remove(const std::string& index, int value) {
        load_index();

        if (index_cache.find(index) == index_cache.end()) return;

        auto& values = value_cache[index];
        if (values.empty()) {
            size_t offset = index_cache[index];
            size_t count = 0;
            std::ifstream check_file(data_file, std::ios::binary);
            if (check_file) {
                check_file.seekg(offset - sizeof(size_t));
                check_file.read(reinterpret_cast<char*>(&count), sizeof(count));
                check_file.close();
            }
            values = read_values(offset, count);
        }

        auto it = std::lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            values.erase(it);

            if (values.empty()) {
                index_cache.erase(index);
                value_cache.erase(index);
            } else {
                size_t offset = index_cache[index];
                write_values(offset, values);
            }
        }
    }

    std::vector<int> find(const std::string& index) {
        load_index();

        if (index_cache.find(index) == index_cache.end()) {
            return std::vector<int>();
        }

        auto& values = value_cache[index];
        if (!values.empty()) {
            return values;
        }

        size_t offset = index_cache[index];
        size_t count = 0;
        std::ifstream check_file(data_file, std::ios::binary);
        if (check_file) {
            check_file.seekg(offset - sizeof(size_t));
            check_file.read(reinterpret_cast<char*>(&count), sizeof(count));
            check_file.close();
        }

        values = read_values(offset, count);
        return values;
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