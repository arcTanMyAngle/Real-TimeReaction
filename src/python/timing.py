import time
from typing import Optional
from cpp_timing import HighPrecisionTimer # type: ignore

class TimingModule:
    """Handles precise timing measurements for reaction time testing."""
    
    def __init__(self):
        self.timer = HighPrecisionTimer()
        self.latest_measurement: Optional[float] = None
        self.measurements = []
    
    def start_measurement(self):
        """Start a new reaction time measurement."""
        try:
            self.timer.start()
        except RuntimeError as e:
            raise RuntimeError(f"Failed to start timer: {str(e)}")
    
    def stop_measurement(self) -> float:
        """
        Stop the current measurement and return the reaction time in milliseconds.
        
        Returns:
            float: Reaction time in milliseconds
        """
        try:
            reaction_time = self.timer.stop()
            self.latest_measurement = reaction_time
            self.measurements.append(reaction_time)
            return reaction_time
        except RuntimeError as e:
            raise RuntimeError(f"Failed to stop timer: {str(e)}")
    
    def get_statistics(self):
        """
        Calculate statistics from all measurements.
        
        Returns:
            dict: Statistical summary of measurements
        """
        if not self.measurements:
            return {
                "average": None,
                "min": None,
                "max": None,
                "count": 0
            }
            
        return {
            "average": sum(self.measurements) / len(self.measurements),
            "min": min(self.measurements),
            "max": max(self.measurements),
            "count": len(self.measurements)
        }