from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "cpp_timing",
        ["src/cpp/timing/timing.cpp"],
        include_dirs=[pybind11.get_include()],
        language='c++'
    ),
]

setup(
    name="cpp_timing",
    ext_modules=ext_modules,
)