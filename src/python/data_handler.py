import os
from datetime import datetime

class DataHandler:
    def __init__(self):
        self.data_file = "reaction_times.csv"
        if not os.path.exists(self.data_file):
            with open(self.data_file, "w") as f:
                f.write("timestamp,reaction_time,stimulus_type,stimulus_color\n")
                
    def save_reaction(self, timestamp, reaction_time, stimulus_type, stimulus_color):
        with open(self.data_file, "a") as f:
            f.write(f"{timestamp},{reaction_time},{stimulus_type},{stimulus_color}\n")