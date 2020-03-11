/**
 * @file option_file_parser.cpp
 * @author Herwig Letofsky
 * @brief this app parses an option file for the keys supplied, which are
 * specified as command line arguments. The parsed file needs to fullfill the
 * following criteras.
 *  - One Key-Value pair per line
 *  - Key and Value are seperated by an equal sign with no spaces
 *    e.g: "<key>=<value>"
 *
 * @version 0.1
 * @date 2020-02-17
 *
 * @copyright Copyright (c) 2020
 *
 */

#include <getopt.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <vector>
#include <regex>

#define APP_NAME "[OptionFileParser] "

enum class ModifyKeysMode : uint8_t { read = 0x00, write, remove, undefined };

void print_help() {
  std::cerr << "usage: command [-h] [-v] -f <file_to_parse> "
               "[-s <key>=<value>... | -r <key>... | -d <key>...]\n"
            << "options:\n"
            << "  -h                   print usage information and exit\n"
            << "  -v                   show more detailed output\n"
            << "  -f file_to_parse     path to file which should be parsed\n"
            << "                       file format has to be <key>=<value>\n"
            << "  -w <key>=<value>     set <key> to <value>\n"
            << "  -r <key>             read value of <key>\n"
            << "  -d <key>             delete key-value pair\n";
}

inline std::string ltrim(const std::string& s) {
	return std::regex_replace(s, std::regex("^\\s+"), std::string(""));
}

inline std::string rtrim(const std::string& s) {
	return std::regex_replace(s, std::regex("\\s+$"), std::string(""));
}

inline std::string trim(const std::string& s) {
	return ltrim(rtrim(s));
}


int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_help();
    return 0;
  }

  int opt = -1;
  char file_to_parse_name[256] = {0};
  bool verboseEnabled = false;
  ModifyKeysMode mode = ModifyKeysMode::undefined;

  std::list<std::string> keysToReadOrDelete;
  std::list<std::pair<std::string, std::string>> keysToWrite;

  while ((opt = getopt(argc, argv, "hvf:wrd")) != -1) {
    switch (opt) {
      case 'h':
        print_help();
        return 0;
      case 'v':
        verboseEnabled = true;
        break;
      case 'f':
        snprintf(file_to_parse_name, 256, "%s", optarg);
        break;
      case 'w': {
        if (mode != ModifyKeysMode::undefined &&
            mode != ModifyKeysMode::write) {
          std::cerr << "READ, WRITE and DELETE can not be used together"
                    << std::endl;
          print_help();
          return -1;
        }
        mode = ModifyKeysMode::write;
        break;
      }
      case 'r':
        if (mode != ModifyKeysMode::undefined && mode != ModifyKeysMode::read) {
          std::cerr << "READ, WRITE and DELETE can not be used together"
                    << std::endl;
          print_help();
          return -1;
        }
        mode = ModifyKeysMode::read;
        break;
      case 'd':
        if (mode != ModifyKeysMode::undefined &&
            mode != ModifyKeysMode::remove) {
          std::cerr << "READ, WRITE and DELETE can not be used together"
                    << std::endl;
          print_help();
          return -1;
        }
        mode = ModifyKeysMode::remove;
        break;
      default:
        break;
    }
  }

  if (strlen(file_to_parse_name) == 0) {
    std::cerr << APP_NAME "Please specify a file path" << std::endl;
    print_help();
    return -1;
  }

  if (mode == ModifyKeysMode::undefined) {
    std::cerr << APP_NAME "Please specify a mode: READ, WRITE or DELETE" << std::endl;
    print_help();
    return -1;
  }

  // read the command line arguments
  for (int i = optind; i < argc; ++i) {
    std::string arg(argv[i]);
    std::size_t eq_pos = arg.find_first_of("=", 0);

    if (mode == ModifyKeysMode::read || mode == ModifyKeysMode::remove) {
      if (eq_pos == std::string::npos) {
        keysToReadOrDelete.emplace_back(trim(arg));
      } else {
        std::cerr << "Wrong format of options - Expected <key> | Got '" << arg
                  << "'" << std::endl;
        print_help();
        return -1;
      }
    } else if (mode == ModifyKeysMode::write) {
      if (eq_pos != std::string::npos && eq_pos > 0 &&
          eq_pos < arg.size() - 1) {
        keysToWrite.emplace_back(trim(arg.substr(0, eq_pos)),
                                 trim(arg.substr(eq_pos + 1, arg.size())));
      } else {
        std::cerr << "Wrong format to set key - Expected <key>=<value> | Got '"
                  << arg << "'" << std::endl;
        print_help();
        return -1;
      }
    }
  }

  keysToWrite.unique([](std::pair<std::string, std::string>& a,
                        std::pair<std::string, std::string>& b) {
    return (a.first == b.first);
  });

  if (keysToReadOrDelete.empty() && keysToWrite.empty() &&
      keysToReadOrDelete.empty()) {
    std::cerr << APP_NAME << "Specify at least one key to READ, WRITE or DELETE"
              << std::endl;
    print_help();
    return -1;
  }

  if (verboseEnabled) {
    std::cerr << APP_NAME "File to parse: " << file_to_parse_name << std::endl;
    if (!keysToWrite.empty()) {
      std::cerr << APP_NAME "Keys to set: [";
      for (auto& kv : keysToWrite) {
        std::cerr << kv.first << ": " << kv.second << ", ";
      }
      std::cerr << "]" << std::endl;
    }
    if (!keysToReadOrDelete.empty()) {
      std::cerr << APP_NAME "Keys to read/delete: [";
      for (auto& key : keysToReadOrDelete) {
        std::cerr << key << ", ";
      }
      std::cerr << "]" << std::endl;
    }
  }

  // work on the file
  std::ifstream input_file(file_to_parse_name);
  std::string input_line;
  std::list<std::string> all_file_lines;

  auto lineHasKey = [](std::string& line, std::string& key) {
    std::size_t eq_pos = line.find_first_of("=", 0);
    if (eq_pos != std::string::npos && eq_pos > 0 && eq_pos < line.size() - 1) {
      if (trim(line.substr(0, eq_pos)) == key) {
        return eq_pos;
      }
    }
    return std::string::npos;
  };

  if (!input_file.is_open()) {
    std::cerr << APP_NAME "Failed to open file: '" << file_to_parse_name << "'"
              << std::endl;
  } else {
    if (mode == ModifyKeysMode::read) {
      std::cerr << APP_NAME "Mode: READ" << std::endl;
      std::map<std::string, std::string> keyValueMap;
      while (std::getline(input_file, input_line)) {
        if (input_line[0] == '#') continue;  // ignore commented lines
        for (auto& key : keysToReadOrDelete) {
          std::size_t eq_pos = lineHasKey(input_line, key);
          if (eq_pos != std::string::npos) {
            keyValueMap.emplace(
                key, trim(input_line.substr(eq_pos + 1, input_line.size())));
          }
        }
      }

      for (auto& key : keysToReadOrDelete) {
        if (verboseEnabled) std::cerr << key << "=";
        std::cout << keyValueMap[key] << std::endl;
      }

      input_file.close();
    } else if (mode == ModifyKeysMode::write ||
               mode == ModifyKeysMode::remove) {
      while (std::getline(input_file, input_line)) {
        all_file_lines.emplace_back(std::move(input_line));
      }
      input_file.close();

      // open file early, to avoid unnecessary computations
      std::ofstream output_file(file_to_parse_name);
      if (!output_file.is_open()) {
        std::cerr << APP_NAME "Failed to open output file: "
                  << file_to_parse_name << std::endl;
        return -1;
      }

      if (mode == ModifyKeysMode::write) {
        std::cerr << APP_NAME "Mode: WRITE" << std::endl;
        // replace values of existing keys
        for (auto& line : all_file_lines) {
          for (auto it = keysToWrite.begin(); it != keysToWrite.end();) {
            if (lineHasKey(line, it->first) != std::string::npos) {
              line = it->first + "=" + it->second;
              it = keysToWrite.erase(it);
            } else {
              ++it;
            }
          }
        }
        // add all new key-value pairs
        for (auto& keyValuePair : keysToWrite) {
          all_file_lines.emplace_back(keyValuePair.first + "=" +
                                      keyValuePair.second);
        }
      } else if (mode == ModifyKeysMode::remove) {
        std::cerr << APP_NAME "Mode: DELETE" << std::endl;

        // only us unique values to reduce loop time
        keysToReadOrDelete.unique(
            [](std::string& a, std::string& b) { return (a == b); });
        // remove all occurrences of the key-value pair
        for (auto it = keysToReadOrDelete.begin();
             it != keysToReadOrDelete.end(); ++it) {
          for (auto line = all_file_lines.begin();
               line != all_file_lines.end();) {
            if (lineHasKey((*line), (*it)) != std::string::npos) {
              line = all_file_lines.erase(line);
            } else {
              ++line;
            }
          }
        }
      }
      // write out the file
      for (auto& line : all_file_lines) {
        output_file << line << std::endl;
      }
      output_file.close();
    }
  }
  return 0;
}
