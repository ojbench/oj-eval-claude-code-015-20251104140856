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

    struct IndexEntry {
        std::string key;
        size_t offset;
        size_t size;

        IndexEntry() : offset(0), size(0) {}
        IndexEntry(const std::string& k, size_t o, size_t s) : key(k), offset(o), size(s) {}
    };

    void save_index(const std::map<std::string, IndexEntry>& index) {
        std::ofstream file(index_file, std::ios::binary);
        if (!file) return;

        size_t count = index.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (const auto& [key, entry] : index) {
            size_t key_len = key.length();
            file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            file.write(key.c_str(), key_len);
            file.write(reinterpret_cast<const char*>(&entry.offset), sizeof(entry.offset));
            file.write(reinterpret_cast<const char*>(&entry.size), sizeof(entry.size));
        }

        file.close();
    }

    std::map<std::string, IndexEntry> load_index() {
        std::map<std::string, IndexEntry> index;
        std::ifstream file(index_file, std::ios::binary);
        if (!file) return index;

        size_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        for (size_t i = 0; i < count; i++) {
            size_t key_len;
            file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));

            std::string key(key_len, '\0');
            file.read(&key[0], key_len);

            IndexEntry entry;
            file.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
            file.read(reinterpret_cast<char*>(&entry.size), sizeof(entry.size));
            entry.key = key;

            index[key] = entry;
        }

        file.close();
        return index;
    }

    std::vector<int> read_values(size_t offset, size_t size) {
        std::vector<int> values;
        if (size == 0) return values;

        std::ifstream file(data_file, std::ios::binary);
        if (!file) return values;

        file.seekg(offset);

        size_t count;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));

        values.resize(count);
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

        size_t count = values.size();
        file.write(reinterpret_cast<const char*>(&count), sizeof(count));

        for (int value : values) {
            file.write(reinterpret_cast<const char*>(&value), sizeof(value));
        }

        file.close();
    }

public:
    FileStorage(const std::string& base_name = "database")
        : data_file(base_name + ".dat"), index_file(base_name + ".idx") {}

    void insert(const std::string& index, int value) {
        auto idx = load_index();

        std::vector<int> values;
        size_t old_offset = 0;
        size_t old_size = 0;

        if (idx.find(index) != idx.end()) {
            const auto& entry = idx[index];
            old_offset = entry.offset;
            old_size = entry.size;
            values = read_values(old_offset, old_size);
        }

        auto it = std::lower_bound(values.begin(), values.end(), value);
        if (it == values.end() || *it != value) {
            values.insert(it, value);

            size_t new_size = sizeof(size_t) + values.size() * sizeof(int);

            if (old_size >= new_size) {
                write_values(old_offset, values);
            } else {
                std::ofstream data(data_file, std::ios::app | std::ios::binary);
                size_t new_offset = data.tellp();
                data.close();

                write_values(new_offset, values);
                idx[index] = IndexEntry(index, new_offset, new_size);
                save_index(idx);
            }
        }
    }

    void remove(const std::string& index, int value) {
        auto idx = load_index();

        if (idx.find(index) == idx.end()) return;

        const auto& entry = idx[index];
        auto values = read_values(entry.offset, entry.size);

        auto it = std::lower_bound(values.begin(), values.end(), value);
        if (it != values.end() && *it == value) {
            values.erase(it);

            if (values.empty()) {
                idx.erase(index);
                save_index(idx);
            } else {
                write_values(entry.offset, values);
                size_t new_size = sizeof(size_t) + values.size() * sizeof(int);
                idx[index] = IndexEntry(index, entry.offset, new_size);
                save_index(idx);
            }
        }
    }

    std::vector<int> find(const std::string& index) {
        auto idx = load_index();

        if (idx.find(index) == idx.end()) {
            return std::vector<int>();
        }

        const auto& entry = idx[index];
        return read_values(entry.offset, entry.size);
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