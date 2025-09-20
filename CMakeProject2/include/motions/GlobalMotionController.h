#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>

// Forward declarations
class PIControllerManager;
class ACSControllerManager;

struct Matrix3x3 {
  float m[3][3];
  Matrix3x3();
  void SetIdentity();
  void Transform(float& x, float& y, float& z) const;
  Matrix3x3 GetInverse() const;
};

struct GlobalPosition {
  float x, y, z;
  std::string deviceId;
  GlobalPosition(float x = 0, float y = 0, float z = 0, const std::string& device = "");
};

struct GlobalLimits {
  float minX = -1000, maxX = 1000;
  float minY = -1000, maxY = 1000;
  float minZ = -100, maxZ = 100;
};

class GlobalMotionController {
public:
  GlobalMotionController();
  ~GlobalMotionController();

  // Set controllers (not owned by this class)
  void SetPIController(PIControllerManager* pi) { m_piController = pi; }
  void SetACSController(ACSControllerManager* acs) { m_acsController = acs; }

  PIControllerManager* GetPIController() { return m_piController; }
  ACSControllerManager* GetACSController() { return m_acsController; }

  // Transformation matrix management
  bool LoadTransformationMatrices(const std::string& jsonFile);
  void SetTransformationMatrix(const std::string& deviceId, const Matrix3x3& matrix);

  // Global coordinate movement
  bool MoveToGlobal(const std::string& deviceId, float globalX, float globalY, float globalZ, float velocity = 10.0f);
  bool MoveRelativeGlobal(const std::string& deviceId, float deltaX, float deltaY, float deltaZ, float velocity = 10.0f);
  bool JogGlobal(const std::string& deviceId, int globalAxis, float distance, float velocity = 10.0f);

  // Position queries
  GlobalPosition GetGlobalPosition(const std::string& deviceId);

  // Coordinate transformations
  void GlobalToDevice(const std::string& deviceId, float globalX, float globalY, float globalZ,
    float& deviceX, float& deviceY, float& deviceZ) const;
  void DeviceToGlobal(const std::string& deviceId, float deviceX, float deviceY, float deviceZ,
    float& globalX, float& globalY, float& globalZ) const;

  // Safety
  void SetGlobalLimits(const GlobalLimits& limits) { m_globalLimits = limits; }
  bool IsWithinGlobalLimits(float globalX, float globalY, float globalZ) const;
  void EmergencyStopGlobal();

  // Device management
  std::vector<std::string> GetAvailableDevices() const;
  bool IsDeviceAvailable(const std::string& deviceId) const;

  // Callbacks
  using StatusCallback = std::function<void(const std::string&)>;
  void SetStatusCallback(StatusCallback callback) { m_statusCallback = callback; }

  // Device status methods
  bool IsDeviceMoving(const std::string& deviceId) const;
  bool IsAnyDeviceMoving() const;

  // Asynchronous movement methods
  bool MoveToGlobalAsync(const std::string& deviceId, float globalX, float globalY, float globalZ, float velocity = 10.0f);
  bool JogGlobalAsync(const std::string& deviceId, int globalAxis, float distance, float velocity = 10.0f);

  // Movement status
  bool IsAnyMovementPending() const { return m_movementPending; }

private:
  PIControllerManager* m_piController = nullptr;
  ACSControllerManager* m_acsController = nullptr;

  std::unordered_map<std::string, Matrix3x3> m_transformMatrices;
  GlobalLimits m_globalLimits;
  StatusCallback m_statusCallback;
  mutable std::mutex m_mutex;

  void InitializeDefaultMatrices();
  bool IsPIDevice(const std::string& deviceId) const;
  bool IsACSDevice(const std::string& deviceId) const;
  void LogStatus(const std::string& message) const;


  std::atomic<bool> m_movementPending{ false };
  std::thread m_moveThread;

  void ExecuteMovementThread(const std::string& deviceId, float deviceX, float deviceY, float deviceZ, float velocity);
};