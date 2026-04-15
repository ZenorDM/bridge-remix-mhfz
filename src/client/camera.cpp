#include "camera.h"
#include "di_hook.h"
#include "utility"
#include <array>
#include <chrono>

std::array<UINT, 5> areaToFilter = {
  173,
  174,
  175,
  257,
  265
};

namespace {
  static bool patched = false;
  static auto lastTime = std::chrono::high_resolution_clock::now();
  void patchDisableCulling(Camera::RequiredGameData& data) {
    if (!patched) {
      patched = true;
      uintptr_t offset = 0xCCF0;

      BYTE* addr = (BYTE*) ((uintptr_t) data.mhfoHDhandle + offset);

      BYTE patch[] = { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 };

      DWORD oldProtect;

      if (VirtualProtect(addr, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        memcpy(addr, patch, sizeof(patch));
        VirtualProtect(addr, sizeof(patch), oldProtect, &oldProtect);
      }
    }
  }
}

Camera::RequiredGameData Camera::readGameMemory() {

  static float lastTragetColiisionY = 0;

  RequiredGameData data;
  data.mhfoHDhandle = (UINT_PTR) GetModuleHandleW(L"mhfo-hd.dll");
  patchDisableCulling(data);

  data.areaID = *((UINT*) (data.mhfoHDhandle + 0xDC6BF48));
  data.timeDef = *((UINT*) (data.mhfoHDhandle + 0x2AFA820));
  data.time = *((UINT*) (data.mhfoHDhandle + 0xE7FE170));
  data.questID = *((int*) ( data.mhfoHDhandle + 0xEBEE53C));
  data.questState = *((int*) ( data.mhfoHDhandle + 0xED52892));

  float targetXCollide = *((float*) (data.mhfoHDhandle + 0xE7FFFCC));
  float targetYCollide = *((float*) (data.mhfoHDhandle + 0xE7FFFD0));
  float targetZCollide = *((float*) (data.mhfoHDhandle + 0xE7FFFD4));

  float targetX = *((float*) (data.mhfoHDhandle + 0x1C4A130));
  float targetY = *((float*) (data.mhfoHDhandle + 0x1C4A134));
  float targetZ = *((float*) (data.mhfoHDhandle + 0x1C4A138));

  float eyePosX = *((float*) (data.mhfoHDhandle + 0x1C49D50));
  float eyePosY = *((float*) (data.mhfoHDhandle + 0x1C49D50 + sizeof(float)));
  float eyePosZ = *((float*) (data.mhfoHDhandle + 0x1C49D50 + sizeof(float) * 2));

  data.targetWithCollision = { targetXCollide ,targetYCollide ,targetZCollide };
  data.target = { targetX ,targetY ,targetZ };
  data.eyePos = { eyePosX ,eyePosY ,eyePosZ };

  D3DXVECTOR3 targetToTargetCollide = { 0, targetY - targetYCollide,0 };
  float targetDistance = D3DXVec3Length(&targetToTargetCollide);

  if (targetDistance > 200.0f) {
    data.target = data.targetWithCollision;
  }

  if (abs(lastTragetColiisionY - data.targetWithCollision.y) > 2 && abs(lastTragetColiisionY - data.targetWithCollision.y) < 10 && data.questID != 0) {

    data.target.y += data.targetWithCollision.y - lastTragetColiisionY;
  }

  lastTragetColiisionY = data.targetWithCollision.y;
  return data;
}

void Camera::setCameraData(const CameraData& cameraData) {
  m_cameraData = cameraData;
}

void Camera::update() {
  RequiredGameData data = readGameMemory();
  auto currentTime = std::chrono::high_resolution_clock::now();
  float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
  lastTime = currentTime;
  DI::setCamera(this);

  bool endQuestCinematic = false;
  // wait until cam resync
  if (data.questState == 1) {
    D3DXVECTOR3 eyeToCan = { data.targetWithCollision.x - data.target.x,data.targetWithCollision.y - data.target.y,data.targetWithCollision.z - data.target.z };
    float targetDistance = D3DXVec3Length(&eyeToCan);

    endQuestCinematic = targetDistance > 20.0f;
  }

  if (endQuestCinematic == false && m_cameraData.customCameraEnable && std::find(areaToFilter.begin(), areaToFilter.end(), data.areaID) == areaToFilter.end()) {
    syncCameraToGameCamera(data);
    m_camYaw += DI::getRightStickXAxis(m_cameraData.cameraDeadZonePercent) * m_cameraData.cameraXSpeed * deltaTime * 30.f;
    m_camPitch -= DI::getRightStickYAxis(m_cameraData.cameraDeadZonePercent) * m_cameraData.cameraYSpeed * deltaTime * 30.0f;

    m_camPitch = std::min(std::max(m_camPitch, -60.0f), 80.0f);

    auto radians = [](float angle) {
      return angle * (3.1415 / 180);
      };


    float factor = std::min(std::max(m_camPitch + 90.0f, 0.0f), 45.0f) / 45.0f;
    float distance = std::max(m_cameraData.cameraDistance, 1u) * std::max(factor, 0.2f);

    m_camPos.x = cos(radians(m_camYaw)) * cos(radians(m_camPitch)) * distance + data.target.x;
    m_camPos.y = sin(radians(m_camPitch)) * distance + data.target.y;
    m_camPos.z = sin(radians(m_camYaw)) * cos(radians(m_camPitch)) * distance + data.target.z;

    m_camPos.y = std::max(data.target.y - 130.0f, m_camPos.y);

    m_target = data.target;
    m_targetWithCollision = data.targetWithCollision;
    D3DXVECTOR3 up(0, 1, 0);
    D3DXMatrixLookAtRH(&m_viewMatNew, &m_camPos, &m_target, &up);

    auto updateCamPosition = [&](uint32_t entryAddr) {
      float* x = ((float*) (data.mhfoHDhandle + entryAddr));
      float* y = ((float*) (data.mhfoHDhandle + entryAddr + sizeof(float)));
      float* z = ((float*) (data.mhfoHDhandle + entryAddr + sizeof(float) * 2));

      *x = m_camPos.x;
      *y = m_camPos.y;
      *z = m_camPos.z;
      };

    updateCamPosition(0x1C49E50);
    updateCamPosition(0x1C4A1B0);

    updateCamPosition(0x1C49D50);
    updateCamPosition(0x1C49DD0);
    updateCamPosition(0x1C4A100);
    updateCamPosition(0x1C4A118);
    updateCamPosition(0x1C4A124);
    //ERR updateCamPosition(0x1E86344);
    updateCamPosition(0xE2A32E0);
    updateCamPosition(0xE7FFF8C);
    updateCamPosition(0xE7FFFC0);
    updateCamPosition(0xECDEB68);


    D3DXVECTOR3 camDirNormalize;
    D3DXVec3Normalize(&camDirNormalize, &data.target);
    //mhfo - hd.dll + B5030A "EB 17 - jmp mhfo - hd.dll + B50323""
    *(BYTE*) ((PBYTE) (data.mhfoHDhandle + 0xB5030A)) = 0xEB;
    /*auto updateCamDir = [&](uint32_t entryAddr) {
      float* x = ((float*) (data.mhfoHDhandle + entryAddr));
      float* y = ((float*) (data.mhfoHDhandle + entryAddr + sizeof(float)));
      float* z = ((float*) (data.mhfoHDhandle + entryAddr + sizeof(float) * 2));

      *x = camDirNormalize.x;
      *y = camDirNormalize.y;
      *z = camDirNormalize.z;
      };

   updateCamDir(0xDC6B730);
    updateCamDir(0xDCE5B28);
    updateCamDir(0x1B7FF38);
    updateCamDir(0x1B80064);
    updateCamDir(0xE2A3278);
    updateCamDir(0xE2A32B8);
    updateCamDir(0xE7FFF60);//oo
    updateCamDir(0xE7FFF40);//oo
    //updateCamDir(0xE87EFB0);
    updateCamDir(0xED0683C);*/

    m_blockCustomCamera = false;
  }
  else{
    m_blockCustomCamera = true;
  }
}

void Camera::redirectCamera(D3DMATRIX* viewMat) {
  if (m_blockCustomCamera == false) {
    *viewMat = m_viewMatNew;
  }
}

void Camera::syncCameraToGameCamera(const Camera::RequiredGameData& data) {
  static bool changeAreaRegistered = false;
  auto degree = [](float angle) {
    return angle * (180.0 / 3.1415);
    };

  if (data.areaID == 0 || data.areaID >= 470) {
    changeAreaRegistered = true;
  }
  else
  {
      if (changeAreaRegistered) {
        D3DXVECTOR2 V { data.targetWithCollision.x - data.eyePos.x , data.targetWithCollision.z - data.eyePos.z };

        D3DXVECTOR2 camDirNormalize;
        D3DXVec2Normalize(&camDirNormalize, &V);
        D3DXVECTOR2 f(0.0f, -1.0f);
        m_camYaw = atan2(-camDirNormalize.y, -camDirNormalize.x);
        m_camYaw = degree(m_camYaw);
        m_camPitch = 0;
        changeAreaRegistered = false;
      }
    }
}
