from src.python.timing import TimingModule
import time

def test_basic_timing():
    print("Testing basic timing functionality...")
    timing = TimingModule()
    
    print("Starting 1-second measurement...")
    timing.start_measurement()
    time.sleep(1)  # Sleep for exactly 1 second
    result = timing.stop_measurement()
    
    print(f"Measured time: {result:.2f} milliseconds")
    print(f"Expected time: 1000.00 milliseconds")
    print(f"Difference: {abs(result - 1000):.2f} milliseconds")
    
    return abs(result - 1000) < 50  # Check if within 50ms of expected

if __name__ == "__main__":
    success = test_basic_timing()
    print("\nTest result:", "PASSED" if success else "FAILED")