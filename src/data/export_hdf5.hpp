#pragma once

#include "data/store.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace mads_dash::data {

/// `datasets` counts HDF5 datasets written (t and value each count);
/// `samples` counts exported samples (a matrix counts as one).
struct ExportStats {
  std::size_t datasets = 0, samples = 0;
};

/// Writes the samples with `t0 <= t <= t1` of the store to an HDF5 file
/// (overwritten). Empty `keypaths` means all. Keypaths without samples in
/// range are skipped. Throws std::runtime_error on I/O failure.
ExportStats export_hdf5(const Store &store, const std::filesystem::path &file,
                        double t0, double t1,
                        const std::vector<std::string> &keypaths = {});

} // namespace mads_dash::data
