#pragma once
#include "d3dx9/d3dx9math.h"

struct CameraData {
  UINT cameraDistance;
  UINT cameraXSpeed;
  UINT cameraYSpeed;
  UINT cameraDeadZonePercent;
  bool customCameraEnable;
};

class Camera {
public:
  void setCameraData(const CameraData& cameraData);
  void update();
  void redirectCamera(D3DMATRIX* viewMat);
  struct RequiredGameData {
    UINT_PTR mhfoHDhandle;
    UINT areaID;
    UINT timeDef;
    UINT time;
    int questID;
    int questState;
    D3DXVECTOR3 target;
    D3DXVECTOR3 targetWithCollision;
    D3DXVECTOR3 eyePos;
  };
private:



  RequiredGameData readGameMemory();
  void syncCameraToGameCamera(const RequiredGameData& data);
  friend class DirectInput8Hook;
protected:
  CameraData m_cameraData;
  D3DXMATRIX m_viewMatNew;
  D3DXVECTOR3 m_camPos;
  D3DXVECTOR3 m_target;
  D3DXVECTOR3 m_targetWithCollision;
  int previousTime = 0;
  bool m_blockCustomCamera = false;
  float m_camYaw = 90.0f;
  float m_camPitch;
  float m_fovY = 0.0f;
};