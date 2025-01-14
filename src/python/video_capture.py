import cv2
import numpy as np
from typing import Optional, Tuple, Union
import logging

class VideoCaptureModule:
    """
    Handles video capture from webcam with robust error handling and frame processing.
    
    This module provides a clean interface to OpenCV's video capture functionality,
    with additional features for frame processing and error handling.
    """
    
    def __init__(self, camera_index: int = 0):
        """
        Initialize the video capture module.
        
        Args:
            camera_index (int): Index of the camera to use (default is 0 for primary webcam)
        """
        self.camera_index = camera_index
        self.capture = None
        self.is_running = False
        
        # Configure logging
        logging.basicConfig(level=logging.INFO)
        self.logger = logging.getLogger(__name__)
        
    def start(self) -> bool:
        """
        Start the video capture.
        
        Returns:
            bool: True if capture started successfully, False otherwise
        """
        try:
            # Initialize video capture
            self.capture = cv2.VideoCapture(self.camera_index)
            
            # Verify camera opened successfully
            if not self.capture.isOpened():
                self.logger.error(f"Failed to open camera at index {self.camera_index}")
                return False
            
            # Set common properties for better performance
            self.capture.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
            self.capture.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
            self.capture.set(cv2.CAP_PROP_FPS, 30)
            
            self.is_running = True
            self.logger.info("Video capture started successfully")
            return True
            
        except Exception as e:
            self.logger.error(f"Error starting video capture: {str(e)}")
            return False
    
    def get_frame(self) -> Tuple[bool, Optional[np.ndarray]]:
        """
        Capture and return a single frame from the video feed.
        
        Returns:
            Tuple[bool, Optional[np.ndarray]]: 
                - Success flag
                - Frame data if successful, None otherwise
        """
        if not self.is_running or self.capture is None:
            self.logger.error("Attempted to get frame while capture is not running")
            return False, None
        
        try:
            ret, frame = self.capture.read()
            if not ret:
                self.logger.warning("Failed to capture frame")
                return False, None
                
            return True, frame
            
        except Exception as e:
            self.logger.error(f"Error capturing frame: {str(e)}")
            return False, None
    
    def get_processed_frame(self) -> Tuple[bool, Optional[np.ndarray]]:
        """
        Get a frame with basic preprocessing applied.
        
        This method applies common preprocessing steps that might be useful
        for reaction time detection:
        - Conversion to grayscale
        - Basic noise reduction
        - Edge enhancement
        
        Returns:
            Tuple[bool, Optional[np.ndarray]]:
                - Success flag
                - Processed frame if successful, None otherwise
        """
        success, frame = self.get_frame()
        if not success or frame is None:
            return False, None
            
        try:
            # Convert to grayscale for simpler processing
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            
            # Apply Gaussian blur to reduce noise
            blurred = cv2.GaussianBlur(gray, (5, 5), 0)
            
            # Enhance edges using Laplacian
            edges = cv2.Laplacian(blurred, cv2.CV_64F)
            
            # Convert back to uint8
            processed = np.uint8(np.absolute(edges))
            
            return True, processed
            
        except Exception as e:
            self.logger.error(f"Error processing frame: {str(e)}")
            return False, None
    
    def stop(self) -> None:
        """
        Stop the video capture and release resources.
        """
        if self.capture is not None:
            try:
                self.capture.release()
                self.logger.info("Video capture stopped")
            except Exception as e:
                self.logger.error(f"Error stopping video capture: {str(e)}")
            finally:
                self.capture = None
                self.is_running = False

    def __del__(self):
        """
        Destructor to ensure resources are properly released.
        """
        self.stop()