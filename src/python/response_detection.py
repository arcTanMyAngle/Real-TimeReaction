import cv2
import numpy as np
from typing import Optional, Tuple, Dict
import logging
import time

class ResponseDetectionModule:
    def __init__(self, 
                 movement_threshold: float = 1000,
                 frame_buffer_size: int = 3):
        self.movement_threshold = movement_threshold
        self.frame_buffer_size = frame_buffer_size
        self.previous_frames = []
        logging.basicConfig(level=logging.INFO)
        self.logger = logging.getLogger(__name__)
        self.waiting_for_response = False
        self.response_start_time = 0.0
        self.last_movement_timestamp = 0.0
        
    def detect_movement(self, current_frame: np.ndarray) -> Tuple[bool, np.ndarray]:
        gray = cv2.cvtColor(current_frame, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        
        self.previous_frames.append(blurred)
        if len(self.previous_frames) > self.frame_buffer_size:
            self.previous_frames.pop(0)
            
        if len(self.previous_frames) < 2:
            return False, current_frame.copy()
            
        frame_diff = cv2.absdiff(self.previous_frames[-2], self.previous_frames[-1])
        _, thresh = cv2.threshold(frame_diff, 25, 255, cv2.THRESH_BINARY)
        
        kernel = np.ones((5,5), np.uint8)
        dilated = cv2.dilate(thresh, kernel, iterations=2)
        
        contours, _ = cv2.findContours(dilated, cv2.RETR_EXTERNAL, 
                                     cv2.CHAIN_APPROX_SIMPLE)
        
        total_movement_area = sum(cv2.contourArea(c) for c in contours)
        
        motion_vis = current_frame.copy()
        cv2.drawContours(motion_vis, contours, -1, (0, 255, 0), 2)
        cv2.putText(motion_vis, f"Movement: {total_movement_area:.0f}", 
                   (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        cv2.putText(motion_vis, f"Threshold: {self.movement_threshold:.0f}",
                   (10, 70), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        
        movement_detected = total_movement_area > self.movement_threshold
        if movement_detected:
            self.last_movement_timestamp = time.time()
            
        return movement_detected, motion_vis
        
    def start_response_window(self) -> None:
        self.waiting_for_response = True
        self.response_start_time = time.time()
        self.logger.info("Started waiting for response")
        
    def stop_response_window(self) -> Optional[float]:
        if not self.waiting_for_response:
            return None
            
        self.waiting_for_response = False
        
        if self.last_movement_timestamp >= self.response_start_time:
            response_time = (self.last_movement_timestamp - self.response_start_time) * 1000
            self.logger.info(f"Response detected in {response_time:.1f} ms")
            return response_time
        
        self.logger.info("No response detected")
        return None
        
    def get_response_visualization(self, frame: np.ndarray) -> np.ndarray:
        vis_frame = frame.copy()
        
        if self.waiting_for_response:
            elapsed_time = (time.time() - self.response_start_time) * 1000
            cv2.putText(vis_frame, f"Reaction Time: {elapsed_time:.0f} ms",
                       (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
        
        return vis_frame