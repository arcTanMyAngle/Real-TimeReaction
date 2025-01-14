import pytest
from src.python.timing import TimingModule
import time

def test_timing_module_basic():
    timing = TimingModule()
    
    # Test basic measurement
    timing.start_measurement()
    time.sleep(0.1)  # Sleep for 100ms
    reaction_time = timing.stop_measurement()
    
    # Check if measurement is reasonable (between 90ms and 110ms)
    assert 90 <= reaction_time <= 110, f"Expected ~100ms, got {reaction_time}ms"
    
def test_timing_module_statistics():
    timing = TimingModule()
    
    # Take multiple measurements
    for _ in range(3):
        timing.start_measurement()
        time.sleep(0.1)
        timing.stop_measurement()
    
    stats = timing.get_statistics()
    assert stats["count"] == 3
    assert 90 <= stats["average"] <= 110