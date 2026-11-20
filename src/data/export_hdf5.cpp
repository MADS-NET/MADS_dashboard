#include "data/export_hdf5.hpp"

#include <highfive/highfive.hpp>

#include <algorithm>
#include <stdexcept>

namespace mads_dash::data {

namespace {

bool in_range(double t, double t0, double t1) { return t >= t0 && t <= t1; }

/// Writes one keypath; returns false when nothing falls in range.
bool write_series(HighFive::File &f, const std::string &path, const Series &s,
                  double t0, double t1, ExportStats &st) {
  std::vector<double> t;
  const char *kind = nullptr;
  auto make_group = [&] {
    auto g = f.createGroup(path);
    const std::string k = kind;
    g.createAttribute<std::string>("kind", HighFive::DataSpace::From(k))
        .write(k);
    return g;
  };

  switch (s.kind) {
  case Kind::Number:
  case Kind::Bool: {
    std::vector<double> v;
    for (std::size_t i = 0; i < s.t.size() && i < s.v.size(); ++i)
      if (in_range(s.t[i], t0, t1)) {
        t.push_back(s.t[i]);
        v.push_back(s.v[i]);
      }
    if (t.empty())
      return false;
    kind = s.kind == Kind::Bool ? "bool" : "number";
    auto g = make_group();
    g.createDataSet("t", t);
    g.createDataSet("value", v);
    st.datasets += 2;
    break;
  }
  case Kind::String: {
    std::vector<std::string> v;
    for (const auto &[et, ev] : s.events)
      if (in_range(et, t0, t1)) {
        t.push_back(et);
        v.push_back(ev);
      }
    if (t.empty())
      return false;
    kind = "string";
    auto g = make_group();
    g.createDataSet("t", t);
    g.createDataSet("value", v);
    st.datasets += 2;
    break;
  }
  case Kind::Array: {
    std::vector<std::vector<double>> v;
    for (const auto &[at, av] : s.snaps) {
      if (!in_range(at, t0, t1))
        continue;
      if (v.empty() && av.empty())
        continue;
      if (!v.empty() && av.size() != v.front().size())
        continue;
      t.push_back(at);
      v.push_back(av);
    }
    if (t.empty())
      return false;
    kind = "array";
    auto g = make_group();
    g.createDataSet("t", t);
    g.createDataSet("value", v);
    st.datasets += 2;
    break;
  }
  case Kind::Matrix: {
    if (!in_range(s.last_t, t0, t1) || !s.last.is_array() || s.last.empty())
      return false;
    std::vector<std::vector<double>> rows;
    for (const auto &r : s.last) {
      if (!r.is_array() || r.empty() ||
          (!rows.empty() && r.size() != rows.front().size()))
        return false;
      rows.push_back(r.get<std::vector<double>>());
    }
    kind = "matrix";
    auto g = make_group();
    g.createDataSet("value", rows);
    g.createAttribute<double>("t", HighFive::DataSpace::Scalar())
        .write(s.last_t);
    ++st.datasets;
    t.push_back(s.last_t);
    break;
  }
  default:
    return false;
  }
  st.samples += t.size();
  return true;
}

} // namespace

ExportStats export_hdf5(const Store &store, const std::filesystem::path &file,
                        double t0, double t1,
                        const std::vector<std::string> &keypaths) {
  ExportStats st;
  try {
    HighFive::File f(file.string(), HighFive::File::Truncate);
    f.createAttribute<double>("t0", HighFive::DataSpace::Scalar()).write(t0);
    f.createAttribute<double>("t1", HighFive::DataSpace::Scalar()).write(t1);
    const std::string by = "mads-dashboard";
    f.createAttribute<std::string>("created_by", HighFive::DataSpace::From(by))
        .write(by);
    for (const auto &[path, s] : store.series()) {
      if (!keypaths.empty() &&
          std::find(keypaths.begin(), keypaths.end(), path) == keypaths.end())
        continue;
      write_series(f, path, s, t0, t1, st);
    }
  } catch (const HighFive::Exception &e) {
    throw std::runtime_error("HDF5 export to '" + file.string() +
                             "' failed: " + e.what());
  }
  return st;
}

} // namespace mads_dash::data
