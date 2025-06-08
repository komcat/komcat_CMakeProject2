#!/usr/bin/env python3
"""
Training Script for Scanner Peak Prediction
Loads JSON scan files from a folder and trains a neural network model

Usage:
    python train_scanner_model.py --data_dir /path/to/scan/json/files
    python train_scanner_model.py --data_dir ./scan_data --epochs 200 --architecture medium
"""

import argparse
import numpy as np
import pandas as pd
import json
from pathlib import Path
import matplotlib.pyplot as plt
import seaborn as sns
# --- FIX STARTS HERE ---
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_squared_error, r2_score, mean_absolute_error
# --- FIX ENDS HERE ---
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import joblib
from typing import Dict, List, Tuple, Optional
import warnings
warnings.filterwarnings('ignore')

# The rest of your script would follow here...
# (You might also have a definition for `training_script.py` in your earlier conversation, make sure this file matches that one)

class ScannerPeakPredictor:
    """Neural Network model to predict peak locations from initial scan measurements"""
    
    def __init__(self, model_name: str = "scanner_peak_predictor"):
        self.model_name = model_name
        self.model = None
        self.feature_scaler = StandardScaler()
        self.target_scaler = StandardScaler()
        
        # Feature names for the 47 input features
        self.feature_names = [
            # Starting position (6 features)
            'start_u', 'start_v', 'start_w', 'start_x', 'start_y', 'start_z',
            # Starting measurement value (1 feature)
            'start_value',
            # First 10 measurements - values (10 features)
            'meas_1_value', 'meas_2_value', 'meas_3_value', 'meas_4_value', 'meas_5_value',
            'meas_6_value', 'meas_7_value', 'meas_8_value', 'meas_9_value', 'meas_10_value',
            # First 10 measurements - gradients (10 features)
            'meas_1_gradient', 'meas_2_gradient', 'meas_3_gradient', 'meas_4_gradient', 'meas_5_gradient',
            'meas_6_gradient', 'meas_7_gradient', 'meas_8_gradient', 'meas_9_gradient', 'meas_10_gradient',
            # First 10 measurements - improvements (10 features)
            'meas_1_improvement', 'meas_2_improvement', 'meas_3_improvement', 'meas_4_improvement', 'meas_5_improvement',
            'meas_6_improvement', 'meas_7_improvement', 'meas_8_improvement', 'meas_9_improvement', 'meas_10_improvement',
            # Movement pattern features - encoded axis+direction (10 features)
            'move_1_encoded', 'move_2_encoded', 'move_3_encoded', 'move_4_encoded', 'move_5_encoded',
            'move_6_encoded', 'move_7_encoded', 'move_8_encoded', 'move_9_encoded', 'move_10_encoded'
        ]
        
        # Target names for the 9 output predictions
        self.target_names = [
            # Peak position deltas (6 targets)
            'peak_delta_u', 'peak_delta_v', 'peak_delta_w', 
            'peak_delta_x', 'peak_delta_y', 'peak_delta_z',
            # Peak value prediction (1 target)
            'peak_value',
            # Efficiency metrics (2 targets)
            'improvement_factor',
            'measurements_to_peak'
        ]
    
    def load_scan_data(self, data_dir: str) -> List[Dict]:
        """Load all JSON scan files from directory"""
        data_dir = Path(data_dir)
        
        if not data_dir.exists():
            raise FileNotFoundError(f"Data directory not found: {data_dir}")
        
        # Look for JSON files matching the scan pattern
        json_patterns = ["ScanResults_*.json", "scan_*.json", "*.json"]
        scan_files = []
        
        for pattern in json_patterns:
            found_files = list(data_dir.glob(pattern))
            if found_files:
                scan_files.extend(found_files)
                break
        
        if not scan_files:
            raise FileNotFoundError(f"No JSON scan files found in {data_dir}")
        
        scans = []
        for file_path in sorted(scan_files):
            try:
                print(f"Loading: {file_path.name}")
                with open(file_path, 'r') as f:
                    scan_data = json.load(f)
                    scans.append(scan_data)
            except Exception as e:
                print(f"Error loading {file_path}: {e}")
                continue
        
        print(f"Successfully loaded {len(scans)} scan files")
        return scans
    
    def encode_axis_direction(self, axis: str, direction: str) -> float:
        """Encode axis and direction as a single numerical value"""
        axis_map = {'X': 1, 'Y': 2, 'Z': 3}
        direction_map = {'Positive': 0.1, 'Negative': -0.1}
        return axis_map.get(axis, 3) + direction_map.get(direction, 0.1)
    
    def extract_features(self, scan_data: Dict) -> Tuple[np.ndarray, np.ndarray]:
        """Extract features from scan data for training"""
        baseline = scan_data['baseline']
        peak = scan_data['peak']
        measurements = scan_data['measurements']
        
        # Starting position features (6)
        features = [
            baseline['position']['u'],
            baseline['position']['v'], 
            baseline['position']['w'],
            baseline['position']['x'],
            baseline['position']['y'],
            baseline['position']['z'],
            baseline['value']  # Starting measurement value
        ]
        
        # Get first 10 measurements - pad with baseline if fewer measurements
        first_10 = measurements[:10]
        while len(first_10) < 10:
            if first_10:
                # Pad with last measurement
                last_measurement = first_10[-1].copy()
                first_10.append(last_measurement)
            else:
                # Create dummy measurement from baseline
                dummy_measurement = {
                    'value': baseline['value'],
                    'gradient': 0.0,
                    'relativeImprovement': 0.0,
                    'axis': 'Z',
                    'direction': 'Positive'
                }
                first_10.append(dummy_measurement)
        
        # Measurement values (10 features)
        for i in range(10):
            features.append(first_10[i].get('value', baseline['value']))
        
        # Measurement gradients (10 features)
        for i in range(10):
            features.append(first_10[i].get('gradient', 0.0))
        
        # Measurement improvements (10 features)  
        for i in range(10):
            features.append(first_10[i].get('relativeImprovement', 0.0))
        
        # Movement patterns encoded (10 features)
        for i in range(10):
            axis = first_10[i].get('axis', 'Z')
            direction = first_10[i].get('direction', 'Positive')
            features.append(self.encode_axis_direction(axis, direction))
        
        # Target values (9 outputs)
        targets = [
            # Peak position deltas (6)
            peak['position']['u'] - baseline['position']['u'],
            peak['position']['v'] - baseline['position']['v'],
            peak['position']['w'] - baseline['position']['w'], 
            peak['position']['x'] - baseline['position']['x'],
            peak['position']['y'] - baseline['position']['y'],
            peak['position']['z'] - baseline['position']['z'],
            # Peak value (1)
            peak['value'],
            # Improvement factor (1)
            peak['value'] / baseline['value'] if baseline['value'] != 0 else 1.0,
            # Measurements to peak (1)
            next((i+1 for i, m in enumerate(measurements) if m.get('isPeak', False)), len(measurements))
        ]
        
        return np.array(features), np.array(targets)
    
    def prepare_dataset(self, scans: List[Dict]) -> Tuple[np.ndarray, np.ndarray]:
        """Prepare training dataset from scan data"""
        all_features = []
        all_targets = []
        
        print("\nProcessing scan data...")
        for i, scan in enumerate(scans):
            try:
                features, targets = self.extract_features(scan)
                all_features.append(features)
                all_targets.append(targets)
                
                scan_id = scan.get('scanId', f'scan_{i+1}')
                print(f"  {scan_id}: baseline={targets[6]:.2e}, peak={targets[6]:.2e}, improvement={targets[7]:.1f}x")
                
            except Exception as e:
                scan_id = scan.get('scanId', f'scan_{i+1}')
                print(f"  Error processing {scan_id}: {e}")
                continue
        
        if not all_features:
            raise ValueError("No valid scan data found!")
        
        X = np.array(all_features)
        y = np.array(all_targets)
        
        print(f"\nDataset prepared:")
        print(f"  Features shape: {X.shape} (samples x features)")
        print(f"  Targets shape: {y.shape} (samples x targets)")
        print(f"  Feature count: {len(self.feature_names)}")
        print(f"  Target count: {len(self.target_names)}")
        
        return X, y
    
    def build_model(self, input_dim: int, output_dim: int, architecture: str = "medium") -> keras.Model:
        """Build neural network model"""
        
        architectures = {
            "simple": [64, 32],
            "medium": [128, 64, 32], 
            "complex": [256, 128, 64, 32],
            "deep": [512, 256, 128, 64, 32]
        }
        
        layer_sizes = architectures.get(architecture, architectures["medium"])
        
        model = keras.Sequential([
            layers.Input(shape=(input_dim,), name='input_features'),
            layers.BatchNormalization(name='input_normalization'),
        ])
        
        # Add hidden layers with dropout and batch normalization
        for i, size in enumerate(layer_sizes):
            model.add(layers.Dense(size, activation='relu', name=f'hidden_{i+1}'))
            model.add(layers.BatchNormalization(name=f'batch_norm_{i+1}'))
            if i == 0:
                model.add(layers.Dropout(0.4, name=f'dropout_{i+1}'))  # Higher dropout for first layer
            else:
                model.add(layers.Dropout(0.2, name=f'dropout_{i+1}'))
        
        # Output layer
        model.add(layers.Dense(output_dim, activation='linear', name='output_predictions'))
        
        # Compile model with adaptive learning rate
        optimizer = keras.optimizers.Adam(learning_rate=0.001)
        model.compile(
            optimizer=optimizer,
            loss='mse',
            metrics=['mae', 'mse']
        )
        
        return model
    
    def train(self, scans: List[Dict], 
              validation_split: float = 0.2,
              epochs: int = 200,
              batch_size: int = 4,
              architecture: str = "medium",
              early_stopping_patience: int = 25,
              learning_rate: float = 0.001,
              verbose: int = 1) -> Dict:
        """Train the neural network model"""
        
        print(f"\n{'='*60}")
        print("TRAINING SCANNER PEAK PREDICTION MODEL")
        print(f"{'='*60}")
        
        # Prepare dataset
        X, y = self.prepare_dataset(scans)
        
        if len(X) < 3:
            print("Warning: Very small dataset! Consider gathering more scan data.")
        
        # Scale features and targets
        print("\nNormalizing data...")
        X_scaled = self.feature_scaler.fit_transform(X)
        y_scaled = self.target_scaler.fit_transform(y)
        
        # Split data
        if len(X) > 2:
            X_train, X_val, y_train, y_val = train_test_split(
                X_scaled, y_scaled, test_size=validation_split, random_state=42
            )
        else:
            # For very small datasets, use all data for training
            X_train, X_val, y_train, y_val = X_scaled, X_scaled, y_scaled, y_scaled
            print("Warning: Using same data for training and validation due to small dataset size")
        
        print(f"Train samples: {len(X_train)}, Validation samples: {len(X_val)}")
        
        # Build model
        self.model = self.build_model(X.shape[1], y.shape[1], architecture)
        
        if verbose:
            print(f"\nModel Architecture ({architecture}):")
            self.model.summary()
        
        # Callbacks
        callbacks = [
            keras.callbacks.EarlyStopping(
                monitor='val_loss', 
                patience=early_stopping_patience,
                restore_best_weights=True,
                verbose=1
            ),
            keras.callbacks.ReduceLROnPlateau(
                monitor='val_loss',
                factor=0.5,
                patience=max(10, early_stopping_patience // 2),
                min_lr=1e-6,
                verbose=1
            )
        ]
        
        # Train model
        print(f"\nStarting training for {epochs} epochs...")
        history = self.model.fit(
            X_train, y_train,
            validation_data=(X_val, y_val),
            epochs=epochs,
            batch_size=batch_size,
            callbacks=callbacks,
            verbose=verbose
        )
        
        # Evaluate model
        print("\nEvaluating model performance...")
        train_metrics = self.model.evaluate(X_train, y_train, verbose=0)
        val_metrics = self.model.evaluate(X_val, y_val, verbose=0)
        
        # Calculate R² scores
        y_train_pred = self.model.predict(X_train, verbose=0)
        y_val_pred = self.model.predict(X_val, verbose=0)
        
        train_r2 = r2_score(y_train, y_train_pred)
        val_r2 = r2_score(y_val, y_val_pred)
        
        # Create results summary
        results = {
            'history': history,
            'train_loss': train_metrics[0],
            'val_loss': val_metrics[0],
            'train_mae': train_metrics[1],
            'val_mae': val_metrics[1],
            'train_r2': train_r2,
            'val_r2': val_r2,
            'n_samples': len(X),
            'n_features': X.shape[1],
            'n_targets': y.shape[1],
            'architecture': architecture,
            'epochs_trained': len(history.history['loss'])
        }
        
        print(f"\n{'='*40}")
        print("TRAINING RESULTS")
        print(f"{'='*40}")
        print(f"Final Train Loss: {train_metrics[0]:.6f}")
        print(f"Final Val Loss: {val_metrics[0]:.6f}")
        print(f"Train MAE: {train_metrics[1]:.6f}")
        print(f"Val MAE: {val_metrics[1]:.6f}")
        print(f"Train R²: {train_r2:.4f}")
        print(f"Val R²: {val_r2:.4f}")
        print(f"Epochs completed: {results['epochs_trained']}/{epochs}")
        
        return results
    
    def predict(self, 
                start_position: Dict[str, float],
                start_value: float,
                first_10_measurements: List[Dict]) -> Dict:
        """Predict peak location and value from initial scan data"""
        
        if self.model is None:
            raise ValueError("Model not trained. Call train() first or load a trained model.")
        
        # Prepare features (same as training)
        features = [
            start_position['u'], start_position['v'], start_position['w'],
            start_position['x'], start_position['y'], start_position['z'],
            start_value
        ]
        
        # Pad measurements to 10 if needed
        measurements = first_10_measurements.copy()
        while len(measurements) < 10:
            if measurements:
                measurements.append(measurements[-1].copy())
            else:
                measurements.append({
                    'value': start_value,
                    'gradient': 0.0,
                    'relativeImprovement': 0.0,
                    'axis': 'Z',
                    'direction': 'Positive'
                })
        
        # Add measurement features
        for i in range(10):
            features.append(measurements[i].get('value', start_value))
        for i in range(10):
            features.append(measurements[i].get('gradient', 0.0))
        for i in range(10):
            features.append(measurements[i].get('relativeImprovement', 0.0))
        for i in range(10):
            axis = measurements[i].get('axis', 'Z')
            direction = measurements[i].get('direction', 'Positive')
            features.append(self.encode_axis_direction(axis, direction))
        
        # Scale and predict
        X = np.array([features])
        X_scaled = self.feature_scaler.transform(X)
        y_pred_scaled = self.model.predict(X_scaled, verbose=0)
        y_pred = self.target_scaler.inverse_transform(y_pred_scaled)[0]
        
        # Format results
        predicted_peak = {
            'predicted_position': {
                'u': start_position['u'] + y_pred[0],
                'v': start_position['v'] + y_pred[1], 
                'w': start_position['w'] + y_pred[2],
                'x': start_position['x'] + y_pred[3],
                'y': start_position['y'] + y_pred[4],
                'z': start_position['z'] + y_pred[5]
            },
            'predicted_value': y_pred[6],
            'improvement_factor': y_pred[7],
            'estimated_measurements_to_peak': max(1, int(round(y_pred[8]))),
            'movement_deltas': {
                'delta_u': y_pred[0], 'delta_v': y_pred[1], 'delta_w': y_pred[2],
                'delta_x': y_pred[3], 'delta_y': y_pred[4], 'delta_z': y_pred[5]
            },
            'confidence_metrics': {
                'distance_3d': np.sqrt(y_pred[3]**2 + y_pred[4]**2 + y_pred[5]**2),
                'expected_improvement': y_pred[7],
                'efficiency_estimate': y_pred[7] / max(1, y_pred[8])  # improvement per measurement
            }
        }
        
        return predicted_peak
    
    def save_model(self, filepath: str):
        """Save trained model and scalers"""
        if self.model is None:
            raise ValueError("No model to save. Train first.")
        
        filepath = Path(filepath)
        filepath.parent.mkdir(parents=True, exist_ok=True)
        
        # Save model
        self.model.save(f"{filepath}_model.keras")
        
        # Save scalers
        joblib.dump(self.feature_scaler, f"{filepath}_feature_scaler.pkl")
        joblib.dump(self.target_scaler, f"{filepath}_target_scaler.pkl")
        
        # Save metadata
        metadata = {
            'feature_names': self.feature_names,
            'target_names': self.target_names,
            'model_name': self.model_name,
            'n_features': len(self.feature_names),
            'n_targets': len(self.target_names)
        }
        
        with open(f"{filepath}_metadata.json", 'w') as f:
            json.dump(metadata, f, indent=2)
        
        print(f"Model saved to {filepath}_* files")
    
    def load_model(self, filepath: str):
        """Load trained model and scalers"""
        filepath = Path(filepath)
        
        # Load model
        self.model = keras.models.load_model(f"{filepath}_model.keras")
        
        # Load scalers
        self.feature_scaler = joblib.load(f"{filepath}_feature_scaler.pkl")
        self.target_scaler = joblib.load(f"{filepath}_target_scaler.pkl")
        
        # Load metadata
        with open(f"{filepath}_metadata.json", 'r') as f:
            metadata = json.load(f)
        
        self.feature_names = metadata['feature_names']
        self.target_names = metadata['target_names']
        self.model_name = metadata['model_name']
        
        print(f"Model loaded from {filepath}_* files")
    
    def plot_training_history(self, history):
        """Plot training history"""
        fig, axes = plt.subplots(2, 2, figsize=(15, 10))
        
        # Loss
        axes[0,0].plot(history.history['loss'], label='Training Loss', linewidth=2)
        axes[0,0].plot(history.history['val_loss'], label='Validation Loss', linewidth=2)
        axes[0,0].set_title('Model Loss', fontsize=14, fontweight='bold')
        axes[0,0].set_xlabel('Epoch')
        axes[0,0].set_ylabel('Loss')
        axes[0,0].legend()
        axes[0,0].grid(True, alpha=0.3)
        
        # MAE
        axes[0,1].plot(history.history['mae'], label='Training MAE', linewidth=2)
        axes[0,1].plot(history.history['val_mae'], label='Validation MAE', linewidth=2)
        axes[0,1].set_title('Mean Absolute Error', fontsize=14, fontweight='bold')
        axes[0,1].set_xlabel('Epoch')
        axes[0,1].set_ylabel('MAE')
        axes[0,1].legend()
        axes[0,1].grid(True, alpha=0.3)
        
        # Learning rate (if available)
        if 'lr' in history.history:
            axes[1,0].plot(history.history['lr'], linewidth=2, color='red')
            axes[1,0].set_title('Learning Rate', fontsize=14, fontweight='bold')
            axes[1,0].set_xlabel('Epoch')
            axes[1,0].set_ylabel('Learning Rate')
            axes[1,0].set_yscale('log')
            axes[1,0].grid(True, alpha=0.3)
        else:
            axes[1,0].text(0.5, 0.5, 'Learning Rate\nNot Recorded', 
                          ha='center', va='center', transform=axes[1,0].transAxes)
        
        # Loss difference
        loss_diff = np.array(history.history['val_loss']) - np.array(history.history['loss'])
        axes[1,1].plot(loss_diff, linewidth=2, color='purple')
        axes[1,1].axhline(y=0, color='black', linestyle='--', alpha=0.5)
        axes[1,1].set_title('Validation - Training Loss', fontsize=14, fontweight='bold')
        axes[1,1].set_xlabel('Epoch')
        axes[1,1].set_ylabel('Loss Difference')
        axes[1,1].grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(f'{self.model_name}_training_history.png', dpi=300, bbox_inches='tight')
        plt.show()


def main():
    """Main training script with command line arguments"""
    parser = argparse.ArgumentParser(description='Train Scanner Peak Prediction Model')
    
    parser.add_argument('--data_dir', type=str, required=True,
                       help='Directory containing JSON scan files')
    parser.add_argument('--output_dir', type=str, default='./models',
                       help='Directory to save trained model (default: ./models)')
    parser.add_argument('--model_name', type=str, default='scanner_peak_predictor',
                       help='Name for the trained model')
    
    # Training parameters
    parser.add_argument('--epochs', type=int, default=200,
                       help='Number of training epochs (default: 200)')
    parser.add_argument('--batch_size', type=int, default=4,
                       help='Batch size for training (default: 4)')
    parser.add_argument('--learning_rate', type=float, default=0.001,
                       help='Learning rate (default: 0.001)')
    parser.add_argument('--architecture', type=str, default='medium',
                       choices=['simple', 'medium', 'complex', 'deep'],
                       help='Model architecture (default: medium)')
    parser.add_argument('--validation_split', type=float, default=0.2,
                       help='Validation split ratio (default: 0.2)')
    parser.add_argument('--early_stopping_patience', type=int, default=25,
                       help='Early stopping patience (default: 25)')
    
    # Options
    parser.add_argument('--plot', action='store_true',
                       help='Plot training history')
    parser.add_argument('--verbose', type=int, default=1,
                       help='Verbosity level (0, 1, or 2)')
    
    args = parser.parse_args()
    
    try:
        # Initialize predictor
        predictor = ScannerPeakPredictor(args.model_name)
        
        # Load scan data
        print(f"Loading scan data from: {args.data_dir}")
        scans = predictor.load_scan_data(args.data_dir)
        
        if len(scans) == 0:
            print("Error: No scan data loaded!")
            return
        
        # Train model
        results = predictor.train(
            scans=scans,
            epochs=args.epochs,
            batch_size=args.batch_size,
            architecture=args.architecture,
            validation_split=args.validation_split,
            early_stopping_patience=args.early_stopping_patience,
            learning_rate=args.learning_rate,
            verbose=args.verbose
        )
        
        # Save model
        output_path = Path(args.output_dir) / args.model_name
        predictor.save_model(str(output_path))
        
        # Plot training history if requested
        if args.plot:
            predictor.plot_training_history(results['history'])
        
        # Print usage example
        print(f"\n{'='*60}")
        print("MODEL TRAINING COMPLETE!")
        print(f"{'='*60}")
        print(f"Model saved to: {output_path}_*")
        print(f"Training samples: {results['n_samples']}")
        print(f"Final validation R²: {results['val_r2']:.4f}")
        print(f"Final validation MAE: {results['val_mae']:.6f}")
        
        print(f"\n{'='*40}")
        print("USAGE EXAMPLE:")
        print(f"{'='*40}")
        print("""
# Load the trained model
from train_scanner_model import ScannerPeakPredictor

predictor = ScannerPeakPredictor()
predictor.load_model('""" + str(output_path) + """')

# Make a prediction
start_pos = {
    'u': -2.5, 'v': 0.0002, 'w': -0.0003,
    'x': -5.8, 'y': -4.52, 'z': -0.86
}

start_val = 0.00002

measurements = [
    {'value': 0.000014, 'gradient': 0.0, 'relativeImprovement': 0.0, 'axis': 'Z', 'direction': 'Positive'},
    {'value': 0.000021, 'gradient': -0.002, 'relativeImprovement': 0.5, 'axis': 'Z', 'direction': 'Negative'},
    # ... add up to 10 measurements
]

prediction = predictor.predict(start_pos, start_val, measurements)
print("Predicted peak:", prediction['predicted_position'])
print("Expected value:", prediction['predicted_value'])
print("Improvement factor:", prediction['improvement_factor'])
""")
        
    except Exception as e:
        print(f"Error during training: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    main()