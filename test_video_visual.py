from src.python.video_capture import VideoCaptureModule
import cv2
import time

def main():
    """
    Visual test of the VideoCaptureModule showing both raw and processed frames.
    """
    # Initialize video capture
    capture = VideoCaptureModule()
    
    if not capture.start():
        print("Failed to start video capture!")
        return
    
    try:
        while True:
            # Get both raw and processed frames
            success1, raw_frame = capture.get_frame()
            success2, processed_frame = capture.get_processed_frame()
            
            if success1 and success2:
                # Display both frames
                cv2.imshow('Raw Feed', raw_frame)
                cv2.imshow('Processed Feed', processed_frame)
                
                # Break loop on 'q' press
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            else:
                print("Failed to capture frames!")
                break
                
            # Cap frame rate
            time.sleep(1/30)
            
    finally:
        capture.stop()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()