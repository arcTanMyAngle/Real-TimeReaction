from src.python.video_capture import VideoCaptureModule
from src.python.stimuli_display import StimuliDisplayModule
from src.python.response_detection import ResponseDetectionModule
import cv2
import time

def main():
    capture = VideoCaptureModule()
    stimuli = StimuliDisplayModule()
    response = ResponseDetectionModule()
    
    if not capture.start():
        print("Failed to start video capture!")
        return
        
    print("\nStarting Reaction Time Test System")
    print("==================================")
    print("1. Random shapes will appear on screen")
    print("2. Move quickly when you see a shape")
    print("3. Your reaction time will be measured")
    print("4. Press 'q' to quit")
    print("5. Use '+' to increase and '-' to decrease motion sensitivity\n")
    
    reaction_times = []
    
    try:
        while True:
            success, frame = capture.get_frame()
            if not success:
                print("Failed to capture frame!")
                break
                
            movement_detected, motion_frame = response.detect_movement(frame)
            
            if stimuli.is_stimulus_active:
                if stimuli.get_current_stimulus_duration() > 2000:
                    reaction_time = response.stop_response_window()
                    if reaction_time is not None:
                        reaction_times.append(reaction_time)
                    stimuli.deactivate_stimulus()
                    
            elif stimuli.should_show_stimulus(min_delay=2.0, max_delay=4.0):
                stimuli.activate_random_stimulus()
                response.start_response_window()
            
            if movement_detected and response.waiting_for_response:
                reaction_time = response.stop_response_window()
                if reaction_time is not None:
                    reaction_times.append(reaction_time)
                    print(f"Reaction time: {reaction_time:.1f} ms")
                stimuli.deactivate_stimulus()
            
            display_frame = stimuli.overlay_stimulus(frame)
            display_frame = response.get_response_visualization(display_frame)
            
            cv2.imshow('Reaction Time Test', display_frame)
            cv2.imshow('Motion Detection', motion_frame)
            
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            elif key == ord('+'):
                response.movement_threshold *= 0.8
                print(f"Sensitivity increased - Threshold: {response.movement_threshold:.0f}")
            elif key == ord('-'):
                response.movement_threshold *= 1.2
                print(f"Sensitivity decreased - Threshold: {response.movement_threshold:.0f}")
            
            time.sleep(1/30)
            
    finally:
        if reaction_times:
            print("\nTest Results")
            print("============")
            print(f"Number of trials: {len(reaction_times)}")
            print(f"Average reaction time: {sum(reaction_times)/len(reaction_times):.1f} ms")
            print(f"Fastest reaction: {min(reaction_times):.1f} ms")
            print(f"Slowest reaction: {max(reaction_times):.1f} ms")
        
        capture.stop()
        cv2.destroyAllWindows()

if __name__ == "__main__":
    main()