# Real-Time Reaction:
Webcam-based Application that measures user reaction time through motion detection. The system combines Python's ease of use with C++'s performance capabilities to provide accurate reaction time measurements.
# Requirements
- Python 3.9
- OpenCV
- NumPy
- Webcam
## System Architecture 
This application is built using a hybrid approach:
- Core timing logic implemented in C++ for microsecond precision
- Python frontend for webcam handling and user interface
- OpenCV for efficient motion detection and image processing
## Prerequisites

### Essential Software
- Python 3.9 or later
- Visual Studio 2019 or later with C++ Desktop Development workload
   - Make sure to install "Desktop development with C++" workload
   - Include "Windows 10 SDK" and "MSVC v142" components
- Functioning webcam

## Installation

python -m venv venv
venv\Scripts\activate
or
source venv/bin/activate



## Install required Python packages:
pip install -r requirements.txt

## Build C++ components:
python setup.py build_ext --inplace

## Running the Application
Start the reaction time tester:
python test_full_system.py
# Controls

'+' key: Increase motion detection sensitivity

'-' key: Decrease motion detection sensitivity

'q' key: Quit application

# What to Expect

The application opens two windows:

Main window showing your webcam feed with stimulus overlays

Motion detection window displaying movement analysis

Random shapes will appear at random intervals

Move quickly when you see a shape appear

Your reaction time will be measured and displayed

Final statistics show at the end of your session

