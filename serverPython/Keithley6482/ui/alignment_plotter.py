"""Alignment-specific plotter with log scale support."""

from ui.dual_channel_plotter import DualChannelPlotter
import pyqtgraph as pg
import numpy as np


class AlignmentPlotter(DualChannelPlotter):
    """Enhanced plotter for alignment applications."""
    
    def __init__(self, max_points: int = 500):
        super().__init__(max_points)
        self._setup_log_scale_option()
    
    def _setup_log_scale_option(self):
        """Add option for log scale display (useful for wide dynamic range)."""
        # Add log scale option to plots
        for plot in [self.combined_plot, self.ch1_plot_individual, self.ch2_plot_individual]:
            plot.setLogMode(x=False, y=False)  # Can be toggled
            plot.showGrid(x=True, y=True, alpha=0.3)
    
    def update_ch1_plot(self, data: float):
        """Update channel 1 with handling for wide range."""
        # Store absolute value for log scale compatibility
        self.ch1_data.append(abs(data) if data != 0 else 1e-12)
        super().update_ch1_plot(data)
    
    def update_ch2_plot(self, data: float):
        """Update channel 2 with handling for wide range."""
        self.ch2_data.append(abs(data) if data != 0 else 1e-12)
        super().update_ch2_plot(data)