#pragma once
#include <string>
#include <unordered_map>
#include "d3d9/d3d9.h"
#include <optional>

enum class TextureState {
  None = 0,
  Loaded,
  Ready, //Mapped
  Unloaded
};

#define TextureLoadThreadCount 8u
// If we use rtx remix we don't load in place hd texture
#define RTX_REMIX 1

struct Texture {
#if !RTX_REMIX
  Texture(std::string& texturePath, UINT mipLevels, UINT usage, D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey);
  void mapTexture(IDirect3DDevice9* device);

  void loadFile();
  void freeData();

  std::string path;
  unsigned char* data;

  IDirect3DTexture9* texture = nullptr;

  int width;
  int height;

  UINT mipLevels;
  UINT usage;
  D3DPOOL pool;
  DWORD filter;
  DWORD mipFilter;
  D3DCOLOR colorKey;
  bool isEmmodel = false;
  TextureState state = TextureState::None;
  bool releaseRegister = false;
#endif
};

class TexturesManager {
public:
#if !RTX_REMIX
  void pushToLoad(std::string& texturePath,UINT mipLevels, UINT usage, D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey, uint32_t textureHdl);
  void update(IDirect3DDevice9* device, uint32_t threadID);
  void updateLoadingThread(uint32_t threadID);
  IDirect3DTexture9* setTexture(IDirect3DDevice9* device, UINT hdl);
  void destroyTexture(UINT hdl);
  bool isExecution() const {
    return  !m_executionFinished;
  }
  void closeExecution() {
    m_executionFinished = true;
  }
#endif endif
private:
#if !RTX_REMIX
  bool m_executionFinished = false;

  std::unordered_map<uint32_t, Texture> m_textures[TextureLoadThreadCount];
#endif
};