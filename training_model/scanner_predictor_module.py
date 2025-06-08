# scanner_predictor_module.py

import numpy as np
import pandas as pd
import json # You might not need json here if only training script uses it for file loading
from pathlib import Path
# import matplotlib.pyplot as plt # Probably not needed for just prediction
# import seaborn as sns # Probably not needed for just prediction
from sklearn.preprocessing import StandardScaler
from tensorflow import keras
from tensorflow.keras import layers
import joblib
# --- CHANGE IS HERE ---
from typing import Dict, List, Tuple, Optional, Any
# --- END CHANGE ---

class ScannerPeakPredictor:
    def __init__(self):
        self.model = None
        self.scaler_X = None
        self.scaler_y = None
        self.feature_columns = None # Store the order of features used during training
        self.target_columns = None # Store the order of targets used during training

    def load_model(self, model_path: str):
        """Loads the trained model and scalers from the specified path."""
        try:
            self.model = keras.models.load_model(Path(model_path) / 'model.h5')
            self.scaler_X = joblib.load(Path(model_path) / 'scaler_X.pkl')
            self.scaler_y = joblib.load(Path(model_path) / 'scaler_y.pkl')
            self.feature_columns = joblib.load(Path(model_path) / 'feature_columns.pkl')
            self.target_columns = joblib.load(Path(model_path) / 'target_columns.pkl')
            print(f"Model and scalers loaded successfully from {model_path}")
        except Exception as e:
            raise IOError(f"Error loading model components from {model_path}: {e}")

    def predict(self, start_pos: Dict[str, float], start_val: float, measurements: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Makes a prediction for the peak position and value.
        """
        # Ensure the order of axes for consistency (u, v, w, x, y, z)
        axis_order = ['u', 'v', 'w', 'x', 'y', 'z']

        # 1. Prepare input features for prediction
        input_data = []

        # Add start_pos and start_val
        for axis in axis_order:
            input_data.append(start_pos.get(axis, 0.0)) # Use 0.0 if axis not present
        input_data.append(start_val)

        # Add measurements (first 10, pad if less than 10)
        # Each measurement contributes 5 features: value, gradient, relativeImprovement, axis (one-hot), direction (one-hot)
        # You might need to adjust this based on how your training script encoded categorical features (axis, direction)
        # For simplicity here, let's assume direct numerical values or some basic encoding.
        # A more robust solution would be to use the same preprocessing as in training.

        # If you used one-hot encoding for axis and direction during training,
        # you need to recreate that here.
        # Example (simplified, assuming fixed axis order and direction mapping):
        num_measurements_to_use = 10
        for i in range(num_measurements_to_use):
            if i < len(measurements):
                m = measurements[i]
                input_data.append(m.get('value', 0.0))
                input_data.append(m.get('gradient', 0.0))
                input_data.append(m.get('relativeImprovement', 0.0))
                # Simplified categorical encoding (replace with actual one-hot from training)
                input_data.append(1.0 if m.get('axis') == 'X' else 0.0) # Example: One-hot for X
                input_data.append(1.0 if m.get('axis') == 'Y' else 0.0) # Example: One-hot for Y
                input_data.append(1.0 if m.get('axis') == 'Z' else 0.0) # Example: One-hot for Z
                input_data.append(1.0 if m.get('direction') == 'Positive' else 0.0) # Example: One-hot for Positive
                input_data.append(1.0 if m.get('direction') == 'Negative' else 0.0) # Example: One-hot for Negative
            else:
                # Pad with zeros if fewer than 10 measurements
                input_data.extend([0.0] * 8) # 5 numerical + 3 axis OHE + 2 direction OHE (adjust count based on your actual encoding)

        # Convert to numpy array and reshape for the model (batch size 1)
        X_pred = np.array(input_data).reshape(1, -1)

        # Ensure X_pred has the same number of features as feature_columns
        if self.feature_columns and X_pred.shape[1] != len(self.feature_columns):
             raise ValueError(f"Input features mismatch. Expected {len(self.feature_columns)} but got {X_pred.shape[1]}. Check your preprocessing in `predict` method.")


        # Scale the input data using the loaded scaler
        X_pred_scaled = self.scaler_X.transform(X_pred)

        # Make the prediction
        y_pred_scaled = self.model.predict(X_pred_scaled)

        # Inverse transform the prediction to get actual peak position and value
        y_pred = self.scaler_y.inverse_transform(y_pred_scaled)[0] # Take first (and only) sample

        # Map predictions back to the target columns (e.g., u, v, w, x, y, z, value)
        predicted_position = {}
        predicted_value = 0.0
        improvement_factor = 1.0 # This would typically be calculated from start_val and predicted_value

        if self.target_columns:
            # Assuming target_columns holds the order: 'u', 'v', 'w', 'x', 'y', 'z', 'value'
            for i, col in enumerate(self.target_columns):
                if col in axis_order: # It's a position coordinate
                    predicted_position[col] = y_pred[i]
                elif col == 'value': # It's the predicted peak value
                    predicted_value = y_pred[i]

            if start_val != 0:
                improvement_factor = predicted_value / start_val
        else:
            # Fallback if target_columns is not loaded (less ideal, assumes fixed output order)
            # This needs to match the exact output order of your model
            predicted_position = {
                'u': y_pred[0], 'v': y_pred[1], 'w': y_pred[2],
                'x': y_pred[3], 'y': y_pred[4], 'z': y_pred[5]
            }
            predicted_value = y_pred[6] # Assuming value is the 7th output
            if start_val != 0:
                improvement_factor = predicted_value / start_val


        return {
            'predicted_position': predicted_position,
            'predicted_value': predicted_value,
            'improvement_factor': improvement_factor
        }