import json
from pathlib import Path
import shutil # Import the shutil module for file operations
from typing import Dict, List, Any

def filter_and_group_json_data(data_folder: Path) -> List[Path]:
    """
    Filters JSON files based on start power and peak value criteria,
    and returns a list of paths to matching files.
    """
    matching_files = []
    
    # Define thresholds
    START_POWER_THRESHOLD = 100 * 1e-6  # 100 micro
    PEAK_VALUE_THRESHOLD = 3 * 1e-3    # 3 milli

    print(f"Scanning for JSON files in: {data_folder}")

    if not data_folder.exists():
        print(f"Error: Data folder '{data_folder}' does not exist.")
        return []
    
    json_files = sorted(list(data_folder.glob('*.json')))
    if not json_files:
        print(f"No JSON files found in '{data_folder}'.")
        return []

    for file_path in json_files:
        try:
            with open(file_path, 'r') as f:
                data = json.load(f)

            start_power = data['baseline']['value']
            
            peak_value = None
            measurements = data.get('measurements', [])
            for m in measurements:
                if m.get('isPeak') == True:
                    peak_value = m.get('value')
                    break # Assuming only one peak per scan

            if peak_value is None:
                print(f"Skipping {file_path.name}: No peak found (isPeak: true missing).")
                continue

            # Apply filtering conditions
            if start_power < START_POWER_THRESHOLD and peak_value > PEAK_VALUE_THRESHOLD:
                print(f"Match found for {file_path.name}:")
                print(f"  Start Power: {start_power:.8f} (Threshold: < {START_POWER_THRESHOLD:.8f})")
                print(f"  Peak Value:  {peak_value:.8f} (Threshold: > {PEAK_VALUE_THRESHOLD:.8f})")
                matching_files.append(file_path)
            else:
                print(f"No match for {file_path.name}:")
                print(f"  Start Power: {start_power:.8f}")
                print(f"  Peak Value:  {peak_value:.8f}")


        except json.JSONDecodeError as e:
            print(f"Error decoding JSON from {file_path.name}: {e}")
        except KeyError as e:
            print(f"Missing expected key '{e}' in JSON from {file_path.name}.")
        except Exception as e:
            print(f"An unexpected error occurred processing {file_path.name}: {e}")

    return matching_files

if __name__ == "__main__":
    # Define the path to your JSON collection folder
    JSON_COLLECTION_FOLDER = Path('C:/Users/komgr/Downloads/scanning/scanning')
    # Define the subfolder where matching files will be copied
    MAIN_DATA_SUBFOLDER = JSON_COLLECTION_FOLDER / 'maindata'

    print(f"\n{'='*60}")
    print("STARTING JSON DATA FILTERING AND COPYING")
    print(f"{'='*60}")

    filtered_json_paths = filter_and_group_json_data(JSON_COLLECTION_FOLDER)

    print(f"\n{'='*60}")
    print("FILTERING SUMMARY")
    print(f"{'='*60}")
    if filtered_json_paths:
        print(f"Found {len(filtered_json_paths)} JSON files matching the criteria.")
        
        # Create the 'maindata' subfolder if it doesn't exist
        try:
            MAIN_DATA_SUBFOLDER.mkdir(parents=True, exist_ok=True)
            print(f"Created/Ensured directory: {MAIN_DATA_SUBFOLDER}")
        except Exception as e:
            print(f"Error creating directory {MAIN_DATA_SUBFOLDER}: {e}")
            exit()

        print("\nCopying matching files to /maindata subfolder:")
        for path in filtered_json_paths:
            destination_path = MAIN_DATA_SUBFOLDER / path.name
            try:
                shutil.copy2(path, destination_path) # copy2 preserves metadata
                print(f"- Copied: {path.name} to {destination_path}")
            except Exception as e:
                print(f"  Error copying {path.name}: {e}")
    else:
        print("No JSON files matched the specified criteria. No files copied.")