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
    std::map<std::string, std::pair<size_t, size_t>> index_cache;
    std::map<std::string, std::vector<int>> value_cache;
    bool index_loaded;

    struct IndexEntry {
        size_t data_offset;
        size_t value_count;

        IndexEntry() : data_offset(0), value_count(0) {}
        IndexEntry(size_t offset, size_t count) : data_offset(offset), value_count(count) {}
    };

    void load_index() {
        if (index_loaded) return;

        std::ifstream index_in(index_file, std::ios::binary);
        if (!index_in) {
            index_loaded = true;
            return;
        }

        size_t total_keys;
        index_in.read(reinterpret_cast<char*>(&total_keys), sizeof(total_keys));

        for (size_t i = 0; i < total_keys; i++) {
            size_t key_len;
            index_in.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

            std::string key(key_len, '\0');
            index_in.read(&key[0], key_len);

            IndexEntry entry;
            index_in.read(reinterpret_cast<char*>(&entry.data_offset), sizeof(entry.data_offset));
            index_in.read(reinterpret_cast<char*>(&entry.value_count), sizeof(entry.value_count));

            index_cache[key] = std::make_pair(entry.data_offset, entry.value_count);
        }

        index_in.close();
        index_loaded = true;
    }

    void save_index() {
        std::ofstream index_out(index_file, std::ios::binary);
        if (!index_out) return;

        size_t total_keys = index_cache.size();
        index_out.write(reinterpret_cast<const char*>(&total_keys), sizeof(total_keys));

        for (const auto& [key, pos_info] : index_cache) {
            size_t key_len = key.length();
            index_out.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            index_out.write(key.c_str(), key_len);
            index_out.write(reinterpret_cast<const char*>(&pos_info.first), sizeof(pos_info.first));
            index_out.write(reinterpret_cast<const char*>(&pos_info.second), sizeof(pos_info.second));
        }

        index_out.close();
    }

    std::vector<int> read_values(size_t offset, size_t count) {
        std::vector<int> values;
        if (count == 0) return values;

        std::ifstream data_in(data_file, std::ios::binary);
        if (!data_in) return values;

        data_in.seekg(offset);
        values.resize(count);

        for (size_t i = 0; i < count; i++) {
            data_in.read(reinterpret_cast<char*>(&values[i]), sizeof(values[i]));
        }

        data_in.close();
        return values;
    }

    void write_values(size_t offset, const std::vector<int>& values) {
        std::fstream data_out(data_file, std::ios::in | std::ios::out | std::ios::binary);
        if (!data_out) {
            std::ofstream create_file(data_file, std::ios::binary);
            create_file.close();
            data_out.open(data_file, std::ios::in | std::ios::out | std::ios::binary);
            if (!data_out) return;
        }

        data_out.seekp(offset);
        for (int value : values) {
            data_out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
        data_out.close();
    }

    size_t append_values(const std::vector<int>& values) {
        std::ofstream data_out(data_file, std::ios::app | std::ios::binary);
        if (!data_out) return 0;

        size_t offset = data_out.tellp();
        for (int value : values) {
            data_out.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }
        data_out.close();
        return offset;
    }

public:
    FileStorage(const std::string& base_name = "database")
        : data_file(base_name + ".dat"), index_file(base_name + ".idx"), index_loaded(false) {
        load_index();
    }

    ~FileStorage() {
        save_index();
    }

    void insert(const std::string& index, int value) {
        load_index();

        auto& cached_values = value_cache[index];
        if (cached_values.empty() && index_cache.find(index) != index_cache.end()) {
            auto pos_info = index_cache[index];
            cached_values = read_values(pos_info.first, pos_info.second);
        }

        auto it = std::lower_bound(cached_values.begin(), cached_values.end(), value);
        if (it == cached_values.end() || *it != value) {
            cached_values.insert(it, value);

            if (index_cache.find(index) == index_cache.end()) {
                size_t offset = append_values(cached_values);
                index_cache[index] = std::make_pair(offset, cached_values.size());
            } else {
                auto pos_info = index_cache[index];
                write_values(pos_info.first, cached_values);
                index_cache[index] = std::make_pair(pos_info.first, cached_values.size());
            }
        }
    }

    void remove(const std::string& index, int value) {
        load_index();

        if (index_cache.find(index) == index_cache.end()) return;

        auto& cached_values = value_cache[index];
        if (cached_values.empty()) {
            auto pos_info = index_cache[index];
            cached_values = read_values(pos_info.first, pos_info.second);
        }

        auto it = std::lower_bound(cached_values.begin(), cached_values.end(), value);
        if (it != cached_values.end() && *it == value) {
            cached_values.erase(it);

            if (cached_values.empty()) {
                index_cache.erase(index);
                value_cache.erase(index);
            } else {
                auto pos_info = index_cache[index];
                write_values(pos_info.first, cached_values);
                index_cache[index] = std::make_pair(pos_info.first, cached_values.size());
            }
        }
    }

    std::vector<int> find(const std::string& index) {
        load_index();

        if (index_cache.find(index) == index_cache.end()) {
            return std::vector<int>();
        }

        auto& cached_values = value_cache[index];
        if (!cached_values.empty()) {
            return cached_values;
        }

        auto pos_info = index_cache[index];
        cached_values = read_values(pos_info.first, pos_info.second);
        return cached_values;
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