import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime

class DataAnalyzer:
    def __init__(self, data_file="reaction_times.csv"):
        self.data_file = data_file
        
    def load_data(self):
        return pd.read_csv(self.data_file)
        
    def generate_reports(self):
        try:
            df = self.load_data()
            print("Loaded data columns:", df.columns.tolist())
            daily_avg = df.groupby(pd.to_datetime(df.iloc[:,0]).dt.date)['reaction_time'].mean()
            shape_avg = df.groupby('stimulus_type')['reaction_time'].mean()
            color_avg = df.groupby('stimulus_color')['reaction_time'].mean()
            self.plot_statistics(daily_avg, shape_avg, color_avg)
        except Exception as e:
            print("Analysis error:", e)
        
    def plot_statistics(self, daily_avg, shape_avg, color_avg):
        fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 12))
        
        daily_avg.plot(ax=ax1, title='Daily Average Reaction Time')
        ax1.set_ylabel('Milliseconds')
        
        shape_avg.plot(kind='bar', ax=ax2, title='Average by Shape')
        ax2.set_ylabel('Milliseconds')
        
        color_avg.plot(kind='bar', ax=ax3, title='Average by Color')
        ax3.set_ylabel('Milliseconds')
        
        plt.tight_layout()
        plt.savefig('reaction_analysis.png')
        plt.show()