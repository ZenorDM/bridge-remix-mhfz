#include "textures_manager.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <thread>


Texture::Texture(std::string& texturePath, UINT mipLevels, UINT usage, D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey) :
  path(texturePath), mipLevels(mipLevels), usage(usage), pool(pool), filter(filter), mipFilter(mipFilter), colorKey(colorKey)
{
  state = TextureState::None; 
}

void TexturesManager::pushToLoad(std::string& texturePath,UINT mipLevels, UINT usage, D3DPOOL pool, DWORD filter, DWORD mipFilter, D3DCOLOR colorKey, uint32_t textureHdl) {
  unsigned int nthreads = std::min(std::thread::hardware_concurrency(), TextureLoadThreadCount);


  size_t minThread = 0xFFffFFff;
  uint32_t selectedThread = 0;
  for (uint32_t threadID = 0; threadID < nthreads; ++threadID) {
    if (minThread > m_Textures[threadID].size()) {
      minThread = m_Textures[threadID].size();
      selectedThread = threadID;
    }

  }

  m_Textures[selectedThread].emplace(textureHdl, Texture { texturePath, mipLevels, usage,pool, filter, mipFilter, colorKey });
}

void TexturesManager::update(IDirect3DDevice9* device, uint32_t threadID) {
  for (auto& [hdl, texture] : m_Textures[threadID]) {
    if (texture.state == TextureState::Loaded) {
      texture.mapTexture(device);
    }
  }
}

// Happen on loading thread
void TexturesManager::updateLoadingThread(uint32_t threadID) {
  std::vector<UINT> toRemove;
  for (auto& [hdl, texture] : m_Textures[threadID]) {
    if (texture.state == TextureState::None) {
      texture.loadFile();
    }
    // If texture have been mapped we can free loading data
    if (texture.state == TextureState::Ready) {
      texture.freeData();
    }
    if (texture.releseRegister) {
      if (texture.state >= TextureState::Ready) {
        texture.texture->Release();
        texture.texture = nullptr;
      }
      if (texture.state >= TextureState::Loaded && texture.state < TextureState::Unloaded) {
        texture.freeData();
      }
      toRemove.push_back(hdl);
    }
  }
  for (UINT hdl : toRemove) {
    m_Textures[threadID].erase(hdl);
  }
}

IDirect3DTexture9* TexturesManager::setTexture(IDirect3DDevice9* device, UINT hdl) {

  unsigned int nthreads = std::min(std::thread::hardware_concurrency(), TextureLoadThreadCount);
  for (uint32_t threadID = 0; threadID < nthreads; ++threadID) {
    auto it = m_Textures[threadID].find(hdl);
    if (it != m_Textures[threadID].end()) {
      Texture& texture = it->second;
      if (it->second.state == TextureState::Unloaded) {
        return texture.texture;
      }
    }
  }
  return nullptr;
}

void TexturesManager::destroyTexture(UINT hdl) {
  unsigned int nthreads = std::min(std::thread::hardware_concurrency(), TextureLoadThreadCount);
  for (uint32_t threadID = 0; threadID < nthreads; ++threadID) {
    auto it = m_Textures[threadID].find(hdl);
    if (it != m_Textures[threadID].end()) {
      Texture& tex = it->second;
      tex.releseRegister = true;

    }
  }
}

void Texture::mapTexture(IDirect3DDevice9* device) {
 
  HRESULT result = device->CreateTexture(width, height, mipLevels, usage, D3DFMT_A8B8G8R8, pool, &texture, nullptr);
  if (SUCCEEDED(result)) {
    IDirect3DSurface9* surface;
    texture->GetSurfaceLevel(0, &surface);

    D3DLOCKED_RECT rect;
    HRESULT hr = surface->LockRect(&rect, 0, 0);
    if (SUCCEEDED(hr)) {

      memcpy((uint8_t*) rect.pBits, data, sizeof(unsigned char) * width * height * 4);
      hr = surface->UnlockRect();
    }
    surface->Release();
    state = TextureState::Ready;
  }
}

void Texture::loadFile() {
  int channels_in_file;
  data = stbi_load(path.c_str(), &width, &height, &channels_in_file, STBI_rgb_alpha);

  if (data == nullptr) {
    return;
  }

  state = TextureState::Loaded;
}

void Texture::freeData() { 
  stbi_image_free(data);
  data = nullptr;
  state = TextureState::Unloaded;
}
