/// @file rt_analyze.cc
/// @brief CLI tool for analyzing trace data: computes latency statistics,
///        histograms, and generates reports.
///
/// Usage:
///   rt-analyze [OPTIONS]
///
/// Options:
///   --input <file>      Input trace file (or "simulate" for synthetic data)
///   --samples <count>   Number of synthetic samples (default: 50000)
///   --buckets <n>       Histogram buckets (default: 20)
///   --csv <file>        Export samples to CSV
///   --help              Show this help message

#include "rtdiag/analyzer.h"
#include "rtdiag/types.h"
#include "src/utils/timestamp.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

static void PrintUsage(const char* prog) {
    std::cout << "RT Diagnostic Toolchain - Latency Analyzer" << std::endl;
    std::cout << std::endl;
    std::cout << "Usage: " << prog << " [OPTIONS]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --input <file>      Trace file to analyze" << std::endl;
    std::cout << "  --samples <count>   Synthetic sample count (default: 50000)" << std::endl;
    std::cout << "  --buckets <n>       Histogram buckets (default: 20)" << std::endl;
    std::cout << "  --csv <file>        Export to CSV" << std::endl;
    std::cout << "  --help              Show this message" << std::endl;
}

/// Parse trace dump format: timestamp_ns [cpu] pid p=priority type comm details
static bool ParseTraceLine(const std::string& line, uint64_t& timestamp_ns, int32_t& pid) {
    if (line.empty() || line[0] == '#') return false;

    std::istringstream iss(line);
    iss >> timestamp_ns;
    if (iss.fail()) return false;

    // Skip [cpu]
    std::string token;
    iss >> token;  // [cpu]

    iss >> pid;
    return !iss.fail();
}

int main(int argc, char* argv[]) {
    std::string input_file;
    size_t num_samples = 50000;
    size_t num_buckets = 20;
    std::string csv_file;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input_file = argv[++i];
        } else if (strcmp(argv[i], "--samples") == 0 && i + 1 < argc) {
            num_samples = static_cast<size_t>(std::atol(argv[++i]));
        } else if (strcmp(argv[i], "--buckets") == 0 && i + 1 < argc) {
            num_buckets = static_cast<size_t>(std::atol(argv[++i]));
        } else if (strcmp(argv[i], "--csv") == 0 && i + 1 < argc) {
            csv_file = argv[++i];
        } else {
            std::cerr << "Unknown option: " << argv[i] << std::endl;
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "╔══════════════════════════════════════════════╗" << std::endl;
    std::cout << "║        RT Diagnostic Latency Analyzer        ║" << std::endl;
    std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;

    rtdiag::LatencyAnalyzer analyzer;

    if (!input_file.empty() && input_file != "simulate") {
        // Load trace file and compute inter-event latencies
        std::cout << "[INFO] Loading trace file: " << input_file << std::endl;

        std::ifstream infile(input_file);
        if (!infile.is_open()) {
            std::cerr << "[ERROR] Cannot open: " << input_file << std::endl;
            return 1;
        }

        std::vector<uint64_t> timestamps;
        std::string line;
        while (std::getline(infile, line)) {
            uint64_t ts;
            int32_t pid;
            if (ParseTraceLine(line, ts, pid)) {
                timestamps.push_back(ts);
            }
        }

        std::cout << "[INFO] Loaded " << timestamps.size() << " events" << std::endl;

        // Compute latencies as inter-event intervals
        for (size_t i = 1; i < timestamps.size(); ++i) {
            uint64_t latency = timestamps[i] - timestamps[i - 1];
            analyzer.AddSample(latency);
        }

        std::cout << "[INFO] Computed " << analyzer.Count()
                  << " latency samples" << std::endl;
    } else {
        // Generate synthetic latency data
        std::cout << "[INFO] Generating " << num_samples
                  << " synthetic latency samples" << std::endl;

        std::mt19937 rng(42);
        // Bimodal distribution to simulate realistic RT latency patterns
        std::normal_distribution<double> fast_path(2000.0, 500.0);   // 2µs ± 0.5µs
        std::normal_distribution<double> slow_path(15000.0, 3000.0); // 15µs ± 3µs
        std::uniform_real_distribution<double> selector(0.0, 1.0);

        for (size_t i = 0; i < num_samples; ++i) {
            double latency;
            if (selector(rng) < 0.9) {
                latency = std::max(100.0, fast_path(rng));
            } else {
                latency = std::max(100.0, slow_path(rng));
            }
            analyzer.AddSample(static_cast<uint64_t>(latency));
        }
    }

    std::cout << std::endl;

    // Print the full report
    std::cout << analyzer.RenderReport() << std::endl;

    // Export CSV if requested
    if (!csv_file.empty()) {
        if (analyzer.ExportCSV(csv_file)) {
            std::cout << "[INFO] CSV exported to: " << csv_file << std::endl;
        } else {
            std::cerr << "[ERROR] Failed to export CSV" << std::endl;
        }
    }

    return 0;
}
