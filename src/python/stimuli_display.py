import cv2
import numpy as np
import random
import time
from typing import Tuple, Optional, List, Dict
import logging

class StimuliDisplayModule:
    """
    Handles the display of visual stimuli for reaction time testing.
    
    This module manages the presentation of visual cues, including:
    - Random timing of stimuli appearance
    - Different types of visual cues (shapes, colors, patterns)
    - Integration with the video feed display
    """
    
    def __init__(self, window_size: Tuple[int, int] = (640, 480)):
        """
        Initialize the stimuli display module.
        
        Args:
            window_size (Tuple[int, int]): Width and height of display window
        """
        self.window_width, self.window_height = window_size
        self.is_stimulus_active = False
        self.stimulus_start_time = 0.0
        self.current_stimulus = None
        self.last_stimulus_time = 0.0
        
        # Configure logging
        logging.basicConfig(level=logging.INFO)
        self.logger = logging.getLogger(__name__)
        
        # Stimulus configuration
        self.stimulus_types = {
            'circle': self._draw_circle,
            'square': self._draw_square,
            'cross': self._draw_cross
        }
        
        # Colors in BGR format
        self.colors = {
            'red': (0, 0, 255),
            'green': (0, 255, 0),
            'blue': (255, 0, 0),
            'yellow': (0, 255, 255)
        }

    def _draw_circle(self, frame: np.ndarray, color: Tuple[int, int, int]) -> np.ndarray:
        """Draw a circle stimulus on the frame."""
        center = (self.window_width // 2, self.window_height // 2)
        radius = min(self.window_width, self.window_height) // 4
        cv2.circle(frame, center, radius, color, -1)
        return frame

    def _draw_square(self, frame: np.ndarray, color: Tuple[int, int, int]) -> np.ndarray:
        """Draw a square stimulus on the frame."""
        side = min(self.window_width, self.window_height) // 3
        start_point = (
            self.window_width // 2 - side // 2,
            self.window_height // 2 - side // 2
        )
        end_point = (
            start_point[0] + side,
            start_point[1] + side
        )
        cv2.rectangle(frame, start_point, end_point, color, -1)
        return frame

    def _draw_cross(self, frame: np.ndarray, color: Tuple[int, int, int]) -> np.ndarray:
        """Draw a cross stimulus on the frame."""
        thickness = 20
        length = min(self.window_width, self.window_height) // 3
        
        # Vertical line
        start_point = (
            self.window_width // 2,
            self.window_height // 2 - length // 2
        )
        end_point = (
            self.window_width // 2,
            self.window_height // 2 + length // 2
        )
        cv2.line(frame, start_point, end_point, color, thickness)
        
        # Horizontal line
        start_point = (
            self.window_width // 2 - length // 2,
            self.window_height // 2
        )
        end_point = (
            self.window_width // 2 + length // 2,
            self.window_height // 2
        )
        cv2.line(frame, start_point, end_point, color, thickness)
        return frame

    def should_show_stimulus(self, min_delay: float = 2.0, max_delay: float = 5.0) -> bool:
        """
        Determine if it's time to show a new stimulus based on random timing.
        
        Args:
            min_delay (float): Minimum delay between stimuli in seconds
            max_delay (float): Maximum delay between stimuli in seconds
            
        Returns:
            bool: True if a new stimulus should be shown
        """
        current_time = time.time()
        
        # If no stimulus is active and enough time has passed
        if (not self.is_stimulus_active and 
            current_time - self.last_stimulus_time > 
            random.uniform(min_delay, max_delay)):
            return True
            
        return False

    def activate_random_stimulus(self) -> None:
        """Activate a random stimulus type and color."""
        self.current_stimulus = {
            'type': random.choice(list(self.stimulus_types.keys())),
            'color': random.choice(list(self.colors.keys()))
        }
        self.is_stimulus_active = True
        self.stimulus_start_time = time.time()
        self.logger.info(f"Activated {self.current_stimulus['type']} stimulus in {self.current_stimulus['color']}")

    def deactivate_stimulus(self) -> None:
        """Deactivate the current stimulus."""
        if self.is_stimulus_active:
            self.is_stimulus_active = False
            self.last_stimulus_time = time.time()
            self.current_stimulus = None

    def overlay_stimulus(self, frame: np.ndarray) -> np.ndarray:
        """
        Overlay the current stimulus on the provided frame if active.
        
        Args:
            frame (np.ndarray): Input frame to overlay stimulus on
            
        Returns:
            np.ndarray: Frame with stimulus overlay if active
        """
        if not self.is_stimulus_active or self.current_stimulus is None:
            return frame
            
        overlay = frame.copy()
        stimulus_func = self.stimulus_types[self.current_stimulus['type']]
        color = self.colors[self.current_stimulus['color']]
        
        # Draw the stimulus on the overlay
        overlay = stimulus_func(overlay, color)
        
        # Blend with original frame
        alpha = 0.5
        frame = cv2.addWeighted(overlay, alpha, frame, 1 - alpha, 0)
        
        return frame

    def get_current_stimulus_duration(self) -> Optional[float]:
        """
        Get the duration of the current stimulus if active.
        
        Returns:
            Optional[float]: Duration in milliseconds if stimulus is active, None otherwise
        """
        if not self.is_stimulus_active:
            return None
            
        return (time.time() - self.stimulus_start_time) * 1000