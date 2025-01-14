import pytest
import numpy as np
from src.python.video_capture import VideoCaptureModule

def test_video_capture_initialization():
    """Test basic initialization of the VideoCaptureModule"""
    capture = VideoCaptureModule()
    assert not capture.is_running
    assert capture.camera_index == 0

def test_video_capture_start_stop():
    """Test starting and stopping the video capture"""
    capture = VideoCaptureModule()
    
    # Test starting
    success = capture.start()
    if not success:
        pytest.skip("No camera available - skipping video capture tests")
    
    assert capture.is_running
    
    # Test stopping
    capture.stop()
    assert not capture.is_running

def test_frame_capture():
    """Test frame capture functionality"""
    capture = VideoCaptureModule()
    success = capture.start()
    
    if not success:
        pytest.skip("No camera available - skipping frame capture tests")
    
    # Test regular frame capture
    success, frame = capture.get_frame()
    assert success
    assert isinstance(frame, np.ndarray)
    assert frame.shape[2] == 3  # Should be BGR format
    
    # Test processed frame capture
    success, processed_frame = capture.get_processed_frame()
    assert success
    assert isinstance(processed_frame, np.ndarray)
    assert len(processed_frame.shape) == 2  # Should be grayscale
    
    capture.stop()

def test_error_handling():
    """Test error handling in various scenarios"""
    capture = VideoCaptureModule(camera_index=999)  # Invalid camera index
    
    # Should fail gracefully
    success = capture.start()
    assert not success
    
    # Should handle frame capture when not started
    success, frame = capture.get_frame()
    assert not success
    assert frame is None