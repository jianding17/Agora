// Copyright (c) 2018-2020, Rice University
// RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license

/**
 * @file utils.cpp
 * @brief Utility functions for file and text processing.
 */

#include "utils.h"

size_t cpu_layout[MAX_CORE_NUM];
bool cpu_layout_initialized = false;

void PrintBitmask(const struct bitmask* bm) {
  for (size_t i = 0; i < bm->size; ++i) {
    std::printf("%d", numa_bitmask_isbitset(bm, i));
  }
}

void SetCpuLayoutOnNumaNodes(bool verbose) {
  if (cpu_layout_initialized == false) {
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    // numa_set_localalloc();

    bitmask* bm = numa_bitmask_alloc(num_cores);
    int cpu_id = 0;
    for (int i = 0; i <= numa_max_node(); ++i) {
      numa_node_to_cpus(i, bm);
      if (verbose) {
        std::printf("NUMA node %d ", i);
        PrintBitmask(bm);
        std::printf(" CPUs: ");
      }
      for (size_t j = 0; j < bm->size; j++) {
        if (numa_bitmask_isbitset(bm, j) != 0) {
          if (verbose) {
            std::printf("%zu ", j);
          }
          cpu_layout[cpu_id] = j;
          cpu_id++;
        }
      }
      if (verbose) {
        std::printf("\n");
      }
    }

    numa_bitmask_free(bm);
    cpu_layout_initialized = true;
  }
}

size_t GetPhysicalCoreId(size_t core_id) {
  if (cpu_layout_initialized) {
    return cpu_layout[core_id];
  }
  return core_id;
}

int PinToCore(int core_id) {
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if ((core_id < 0) || (core_id >= num_cores)) {
    return -1;
  }

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

void PinToCoreWithOffset(ThreadType thread_type, int core_offset, int thread_id,
                         bool verbose) {
  if (kEnableThreadPinning == true) {
    int actual_core_id = core_offset + thread_id;
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    /* Reserve core 0 for kernel threads */
    if (actual_core_id >= num_cores) {
      actual_core_id = (actual_core_id % num_cores) + 1;
    }

    size_t physical_core_id =
        cpu_layout_initialized ? cpu_layout[actual_core_id] : actual_core_id;

    if (PinToCore(physical_core_id) != 0) {
      std::fprintf(
          stderr,
          "%s thread %d: failed to pin to core %zu. Exiting. "
          "This can happen if the machine has insufficient cores. "
          "Set kEnableThreadPinning to false to run Agora to run despite "
          "this - performance will be low.\n",
          ThreadTypeStr(thread_type).c_str(), thread_id, physical_core_id);
      throw std::runtime_error("Utils: failed to pin to core");
    } else {
      if (verbose == true) {
        std::printf("%s thread %d: pinned to core %zu, requested core %d \n",
                    ThreadTypeStr(thread_type).c_str(), thread_id,
                    physical_core_id, core_offset + thread_id);
      }
    }  // EnableThreadPinning == true
  }
}

std::vector<size_t> Utils::StrToChannels(const std::string& channel) {
  std::vector<size_t> channels;
  if (channel == "A") {
    channels = {0};
  } else if (channel == "B") {
    channels = {1};
  } else {
    channels = {0, 1};
  }
  return (channels);
}

std::vector<std::complex<int16_t>> Utils::DoubleToCint16(
    std::vector<std::vector<double>> in) {
  int len = in[0].size();
  std::vector<std::complex<int16_t>> out(len, 0);
  for (int i = 0; i < len; i++) {
    out[i] = std::complex<int16_t>((int16_t)(in[0][i] * 32768),
                                   (int16_t)(in[1][i] * 32768));
  }
  return out;
}

std::vector<std::complex<float>> Utils::DoubleToCfloat(
    std::vector<std::vector<double>> in) {
  int len = in[0].size();
  std::vector<std::complex<float>> out(len, 0);
  for (int i = 0; i < len; i++) {
    out[i] = std::complex<float>(in[0][i], in[1][i]);
  }
  return out;
}

std::vector<std::complex<float>> Utils::Uint32tocfloat(
    std::vector<uint32_t> in, const std::string& order) {
  int len = in.size();
  std::vector<std::complex<float>> out(len, 0);
  for (size_t i = 0; i < in.size(); i++) {
    auto arr_hi_int = static_cast<int16_t>(in[i] >> 16);
    auto arr_lo_int = static_cast<int16_t>(in[i] & 0x0FFFF);

    float arr_hi = (float)arr_hi_int / 32768.0;
    float arr_lo = (float)arr_lo_int / 32768.0;

    if (order == "IQ") {
      std::complex<float> csamp(arr_hi, arr_lo);
      out[i] = csamp;
    } else if (order == "QI") {
      std::complex<float> csamp(arr_lo, arr_hi);
      out[i] = csamp;
    }
  }
  return out;
}

std::vector<uint32_t> Utils::Cint16ToUint32(
    std::vector<std::complex<int16_t>> in, bool conj,
    const std::string& order) {
  std::vector<uint32_t> out(in.size(), 0);
  for (size_t i = 0; i < in.size(); i++) {
    auto re = static_cast<uint16_t>(in[i].real());
    auto im = static_cast<uint16_t>(conj ? -in[i].imag() : in[i].imag());
    if (order == "IQ") {
      out[i] = (uint32_t)re << 16 | im;
    } else if (order == "QI") {
      out[i] = (uint32_t)im << 16 | re;
    }
  }
  return out;
}

std::vector<uint32_t> Utils::Cfloat32ToUint32(
    std::vector<std::complex<float>> in, bool conj, const std::string& order) {
  std::vector<uint32_t> out(in.size(), 0);
  for (size_t i = 0; i < in.size(); i++) {
    auto re =
        static_cast<uint16_t>(static_cast<int16_t>(in[i].real() * 32768.0));
    auto im = static_cast<uint16_t>(
        static_cast<int16_t>((conj ? -in[i].imag() : in[i].imag()) * 32768));
    if (order == "IQ") {
      out[i] = (uint32_t)re << 16 | im;
    } else if (order == "QI") {
      out[i] = (uint32_t)im << 16 | re;
    }
  }
  return out;
}

// Returns index locations of sym for each frame in frames
std::vector<std::vector<size_t>> Utils::LoadSymbols(
    std::vector<std::string> const& frames, char sym) {
  std::vector<std::vector<size_t>> symbol_index_vector;
  size_t num_frames = frames.size();
  symbol_index_vector.resize(num_frames);

  for (size_t f = 0; f < num_frames; f++) {
    std::string frame = frames.at(f);
    for (size_t g = 0; g < frame.length(); g++) {
      if (frame.at(g) == sym) {
        symbol_index_vector.at(f).push_back(g);
      }
    }
  }
  return symbol_index_vector;
}

void Utils::LoadDevices(std::string filename, std::vector<std::string>& data) {
  std::string line;
  std::string cur_directory = TOSTRING(PROJECT_DIRECTORY);
  filename = cur_directory + "/" + filename;
  std::ifstream myfile(filename, std::ifstream::in);
  if (myfile.is_open()) {
    while (getline(myfile, line)) {
      // line.erase( std::remove (line.begin(), line.end(), ' '),
      // line.end());
      if (line.at(0) == '#') {
        continue;
      }
      data.push_back(line);
      std::cout << line << '\n';
    }
    myfile.close();
  }

  else {
    std::printf("Unable to open device file %s\n", filename.c_str());
  }
}

void Utils::LoadData(const char* filename,
                     std::vector<std::complex<int16_t>>& data, int samples) {
  FILE* fp = std::fopen(filename, "r");
  data.resize(samples);
  float real;
  float imag;
  for (int i = 0; i < samples; i++) {
    int ret = fscanf(fp, "%f %f", &real, &imag);
    if (ret < 0) {
      break;
    }
    data[i] =
        std::complex<int16_t>(int16_t(real * 32768), int16_t(imag * 32768));
  }
  std::fclose(fp);
}

void Utils::LoadData(const char* filename, std::vector<unsigned>& data,
                     int samples) {
  FILE* fp = std::fopen(filename, "r");
  data.resize(samples);
  for (int i = 0; i < samples; i++) {
    int ret = fscanf(fp, "%u", &data[i]);
    if (ret < 0) {
      break;
    }
  }
  std::fclose(fp);
}

void Utils::LoadTddConfig(const std::string& filename, std::string& jconfig) {
  std::string line;
  std::ifstream config_file(filename);
  if (config_file.is_open()) {
    while (std::getline(config_file, line)) {
      jconfig += line;
    }
    config_file.close();
  }

  else {
    std::printf("Unable to open config file %s\n", filename.c_str());
  }
}

std::vector<std::string> Utils::Split(const std::string& s, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream token_stream(s);
  while (std::getline(token_stream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

void Utils::PrintVector(std::vector<std::complex<int16_t>>& data) {
  for (auto& i : data) {
    std::cout << real(i) << " " << imag(i) << std::endl;
  }
}

void Utils::WriteBinaryFile(const std::string& name, size_t elem_size,
                            size_t buffer_size, void* buff) {
  FILE* f_handle = std::fopen(name.c_str(), "wb");
  std::fwrite(buff, elem_size, buffer_size, f_handle);
  std::fclose(f_handle);
}

void Utils::PrintMat(arma::cx_fmat c, const std::string& ss) {
  std::stringstream so;
  so << ss << " = [";
  for (size_t i = 0; i < c.n_cols; i++) {
    so << "[";
    for (size_t j = 0; j < c.n_rows; j++) {
      so << std::fixed << std::setw(5) << std::setprecision(3)
         << c.at(j, i).real() << "+" << c.at(j, i).imag() << "i ";
    }
    so << "];\n";
  }
  so << "];\n";
  so << std::endl;
  std::cout << so.str();
}

void Utils::PrintVec(arma::cx_fvec c, const std::string& ss) {
  std::stringstream so;
  so << ss << " = [";
  for (size_t j = 0; j < c.size(); j++) {
    so << std::fixed << std::setw(5) << std::setprecision(3) << c.at(j).real()
       << "+" << c.at(j).imag() << "i ";
  }
  so << "];\n";
  so << std::endl;
  std::cout << so.str();
}
