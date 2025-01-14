#include "timing.hpp"
#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(cpp_timing, m) {
    py::class_<HighPrecisionTimer>(m, "HighPrecisionTimer")
        .def(py::init<>())
        .def("start", &HighPrecisionTimer::start)
        .def("stop", &HighPrecisionTimer::stop);
}