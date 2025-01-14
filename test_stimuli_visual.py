from src.python.video_capture import VideoCaptureModule
from src.python.stimuli_display import StimuliDisplayModule
import cv2
import time

def main():
    """
    Visual test combining video capture and stimuli display.
    Shows how the reaction time test will look to users.
    """
    # Initialize modules
    capture = VideoCaptureModule()
    stimuli = StimuliDisplayModule()
    
    if not capture.start():
        print("Failed to start video capture!")
        return
        
    print("Starting visual test...")
    print("Press 'q' to quit")
    print("Watch for random shapes to appear - they simulate the reaction time test")
    
    try:
        while True:
            # Get frame from camera
            success, frame = capture.get_frame()
            if not success:
                print("Failed to capture frame!")
                break
            
            # Check if we should show a new stimulus
            if stimuli.should_show_stimulus():
                stimuli.activate_random_stimulus()
            
            # Overlay any active stimulus
            frame = stimuli.overlay_stimulus(frame)
            
            # Display frame
            cv2.imshow('Reaction Time Test', frame)
            
            # Get processed frame for motion detection
            success, processed = capture.get_processed_frame()
            if success:
                cv2.imshow('Motion Detection', processed)
            
            # Break loop on 'q' press
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
                
            # Deactivate stimulus after 1 second
            if (stimuli.is_stimulus_active and 
                stimuli.get_current_stimulus_duration() > 1000):
                stimuli.deactivate_stimulus()
            
            # Cap frame rate
            time.sleep(1/30)
            
    finally:
        capture.stop()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()