import json
from pathlib import Path
from typing import Dict, List, Any, Optional

# Make sure this file (predict_scanner_peak.py) is in the same directory as training_script.py
from training_script import ScannerPeakPredictor

def load_and_predict_from_json(json_file_path: Path, predictor: ScannerPeakPredictor) -> Optional[Dict]:
    """
    Loads data from a JSON file, makes a prediction, and compares it to the actual peak.
    Returns a dictionary of metrics for summarization or None if processing fails.
    """
    print(f"\n--- Processing: {json_file_path.name} ---")
    metrics = {
        'filename': json_file_path.name,
        'success': False,
        'predicted_position': None,
        'predicted_value': None,
        'actual_position': None,
        'actual_value': None,
        'x_diff': None,
        'y_diff': None,
        'z_diff': None,
        'value_relative_error': None,
        'improvement_factor': None,
        'estimated_measurements_to_peak': None
    }

    try:
        with open(json_file_path, 'r') as f:
            data = json.load(f)

        start_pos = data['baseline']['position']
        start_val = data['baseline']['value']

        measurements_raw = data.get('measurements', [])
        if not isinstance(measurements_raw, list):
            print(f"Warning: 'measurements' is not a list in {json_file_path.name}. Skipping.")
            return metrics

        first_10_measurements_for_prediction = []
        for i, m in enumerate(measurements_raw):
            if i >= 10:
                break
            prepared_m = {
                'value': m.get('value', start_val),
                'gradient': m.get('gradient', 0.0),
                'relativeImprovement': m.get('relativeImprovement', 0.0),
                'axis': m.get('axis', 'Z'),
                'direction': m.get('direction', 'Positive')
            }
            first_10_measurements_for_prediction.append(prepared_m)

        if not first_10_measurements_for_prediction:
            print(f"No valid measurements found or extracted from {json_file_path.name} to make a prediction.")
            return metrics

        try:
            prediction = predictor.predict(
                start_position=start_pos,
                start_value=start_val,
                first_10_measurements=first_10_measurements_for_prediction
            )
            metrics['predicted_position'] = prediction['predicted_position']
            metrics['predicted_value'] = prediction['predicted_value']
            metrics['improvement_factor'] = prediction['improvement_factor']
            metrics['estimated_measurements_to_peak'] = prediction['estimated_measurements_to_peak']
            metrics['success'] = True

        except Exception as e:
            print(f"Error during prediction for {json_file_path.name}: {e}")
            return metrics

        print("\n--- Prediction Results ---")
        print(f"Predicted peak position: {prediction['predicted_position']}")
        print(f"Predicted peak value: {prediction['predicted_value']:.8f}")
        print(f"Improvement factor: {prediction['improvement_factor']:.4f}")
        print(f"Estimated measurements to peak: {prediction['estimated_measurements_to_peak']}")

        actual_peak_pos = None
        actual_peak_value = None
        for m in measurements_raw:
            if m.get('isPeak') == True:
                actual_peak_pos = m.get('position')
                actual_peak_value = m.get('value')
                break

        if actual_peak_pos and actual_peak_value is not None:
            metrics['actual_position'] = actual_peak_pos
            metrics['actual_value'] = actual_peak_value

            print("\n--- Actual Peak Data ---")
            print(f"Actual peak position: {actual_peak_pos}")
            print(f"Actual peak value: {actual_peak_value:.8f}")

            if all(k in actual_peak_pos and k in prediction['predicted_position'] for k in ['x', 'y', 'z']):
                metrics['x_diff'] = abs(actual_peak_pos['x'] - prediction['predicted_position']['x'])
                metrics['y_diff'] = abs(actual_peak_pos['y'] - prediction['predicted_position']['y'])
                metrics['z_diff'] = abs(actual_peak_pos['z'] - prediction['predicted_position']['z'])
                print(f"Absolute difference in X position: {metrics['x_diff']:.6f}")
                print(f"Absolute difference in Y position: {metrics['y_diff']:.6f}")
                print(f"Absolute difference in Z position: {metrics['z_diff']:.6f}")

            if actual_peak_value != 0:
                metrics['value_relative_error'] = abs((actual_peak_value - prediction['predicted_value']) / actual_peak_value) * 100
                print(f"Relative error in peak value: {metrics['value_relative_error']:.2f}%")
        else:
            print("No 'isPeak': true found in measurements for comparison.")

    except json.JSONDecodeError as e:
        print(f"Error decoding JSON from {json_file_path.name}: {e}")
    except KeyError as e:
        print(f"Missing expected key in JSON from {json_file_path.name}: {e}. Please check JSON structure.")
    except Exception as e:
        print(f"An unexpected error occurred processing {json_file_path.name}: {e}")
        import traceback
        traceback.print_exc()

    return metrics


if __name__ == "__main__":
    MODEL_PATH = Path('models/scanner_peak_predictor')
    # Update this path to where your JSON files are located.
    # Using forward slashes or raw strings for path for cross-platform compatibility
    ACTUAL_RESULTS_DIR = Path('C:/Users/komgr/source/repos/komcat/CMakeProject2/training_model/actual_results')

    if not ACTUAL_RESULTS_DIR.exists():
        print(f"Error: Actual results directory '{ACTUAL_RESULTS_DIR}' does not exist.")
        exit()

    # --- CHANGE IS HERE: Dynamically find all JSON files ---
    JSON_FILES = sorted(list(ACTUAL_RESULTS_DIR.glob('*.json'))) # Find all .json files
    if not JSON_FILES:
        print(f"No JSON files found in '{ACTUAL_RESULTS_DIR}'. Please ensure files are present.")
        exit()
    # --- END CHANGE ---

    # Ensure the model exists before trying to load
    # Check for at least one of the expected saved files
    if not (MODEL_PATH.parent / f"{MODEL_PATH.name}_model.keras").exists():
        print(f"Error: Model files not found in '{MODEL_PATH.parent}'.")
        print(f"Expected files like '{MODEL_PATH.name}_model.keras'.")
        print("Please ensure you have run your training script successfully to save the model.")
        exit()

    predictor = ScannerPeakPredictor()
    print(f"Loading model from base path: {MODEL_PATH}")
    try:
        predictor.load_model(str(MODEL_PATH))
        print("Model loaded successfully.")
    except Exception as e:
        print(f"Error loading model: {e}")
        print("Please ensure the model files are present at the specified path and are correctly saved by the training script.")
        exit()

    all_results = []
    for json_file in JSON_FILES:
        result = load_and_predict_from_json(json_file, predictor)
        if result:
            all_results.append(result)

    # --- Summarize Results ---
    print(f"\n{'='*60}")
    print("SUMMARY OF PREDICTION RESULTS")
    print(f"{'='*60}")

    total_files = len(all_results)
    successful_predictions = sum(1 for r in all_results if r['success'] and r['actual_value'] is not None)
    files_with_predictions_but_no_actual = sum(1 for r in all_results if r['success'] and r['actual_value'] is None)
    failed_processing = total_files - successful_predictions - files_with_predictions_but_no_actual

    total_x_diff = 0
    total_y_diff = 0
    total_z_diff = 0
    total_value_relative_error = 0
    count_for_averages = 0

    for r in all_results:
        if r['success'] and r['actual_value'] is not None:
            if r['x_diff'] is not None: total_x_diff += r['x_diff']
            if r['y_diff'] is not None: total_y_diff += r['y_diff']
            if r['z_diff'] is not None: total_z_diff += r['z_diff']
            if r['value_relative_error'] is not None: total_value_relative_error += r['value_relative_error']
            count_for_averages += 1

    print(f"Total files processed: {total_files}")
    print(f"Successfully predicted and compared to actual peak: {successful_predictions}")
    print(f"Successfully predicted but no actual peak found for comparison: {files_with_predictions_but_no_actual}")
    print(f"Failed to process or predict: {failed_processing}")

    if count_for_averages > 0:
        avg_x_diff = total_x_diff / count_for_averages
        avg_y_diff = total_y_diff / count_for_averages
        avg_z_diff = total_z_diff / count_for_averages
        avg_value_relative_error = total_value_relative_error / count_for_averages
        print(f"\nAverage Absolute Differences (for files with actual peaks):")
        print(f"  Avg X Diff: {avg_x_diff:.6f}")
        print(f"  Avg Y Diff: {avg_y_diff:.6f}")
        print(f"  Avg Z Diff: {avg_z_diff:.6f}")
        print(f"  Avg Value Relative Error: {avg_value_relative_error:.2f}%")
    else:
        print("\nNo files with actual peak data were processed for average error calculation.")

    print(f"\n{'='*60}")
    print("DETAILED RESULTS (Successes only)")
    print(f"{'='*60}")
    for r in all_results:
        if r['success']:
            status = "Compared" if r['actual_value'] is not None else "Predicted Only"
            x_diff_str = f"X_Diff: {r['x_diff']:.6f}" if r['x_diff'] is not None else "N/A"
            y_diff_str = f"Y_Diff: {r['y_diff']:.6f}" if r['y_diff'] is not None else "N/A"
            z_diff_str = f"Z_Diff: {r['z_diff']:.6f}" if r['z_diff'] is not None else "N/A"
            val_err_str = f"Val_Err: {r['value_relative_error']:.2f}%" if r['value_relative_error'] is not None else "N/A"
            print(f"File: {r['filename']} - Status: {status}")
            print(f"  Predicted Value: {r['predicted_value']:.8f}, Actual Value: {r['actual_value'] if r['actual_value'] is not None else 'N/A':.8f}")
            print(f"  Predicted Pos: {r['predicted_position']}, Actual Pos: {r['actual_position']}")
            print(f"  Metrics: {x_diff_str}, {y_diff_str}, {z_diff_str}, {val_err_str}")
            print(f"  Improvement Factor: {r['improvement_factor']:.4f}, Est. Measurements: {r['estimated_measurements_to_peak']}")
            print("-" * 30)