#include "Sprite.hxx"

#include "ResourcesManager.hxx"
#include "WindowManager.hxx"
#include "basics/Camera.hxx"
#include "basics/isoMath.hxx"
#include "map/MapLayers.hxx"
#include "common/enums.hxx"
#include "LOG.hxx"
#include "Exception.hxx"

#ifdef MICROPROFILE_ENABLED
#include "microprofile.h"
#endif

Sprite::Sprite(Point _isoCoordinates) : isoCoordinates(_isoCoordinates)
{
  m_screenCoordinates = convertIsoToScreenCoordinates(_isoCoordinates);
  m_SpriteData.resize(LAYERS_COUNT); // resize the spritedata vector to the amount of layers we have.
}

void Sprite::render() const
{
#ifdef MICROPROFILE_ENABLED
  MICROPROFILE_SCOPEI("Map", "Sprite render", MP_RED);
#endif
  for (auto currentLayer : allLayersOrdered)
  {
    if (MapLayers::isLayerActive(currentLayer) && m_SpriteData[currentLayer].texture)
    {
      if (highlightSprite)
      {
        if (SDL_SetTextureColorMod(m_SpriteData[currentLayer].texture, highlightColor.r, highlightColor.g, highlightColor.b))
        {
          //TODO indicates failure - an approperiate LOG message should be printed.
        }
      }

      if (transparentSprite)
      {
        if (SDL_SetTextureAlphaMod(m_SpriteData[currentLayer].texture, alpha))
        {
          //TODO indicates failure - an approperiate LOG message should be printed.
        }
      }

      if (m_SpriteData[currentLayer].clipRect.w != 0)
      {
        SDL_RenderCopy(WindowManager::instance().getRenderer(), m_SpriteData[currentLayer].texture,
                       &m_SpriteData[currentLayer].clipRect, &m_SpriteData[currentLayer].destRect);
      }
      else
      {
        SDL_RenderCopy(WindowManager::instance().getRenderer(), m_SpriteData[currentLayer].texture, nullptr,
                       &m_SpriteData[currentLayer].destRect);
      }

      if (highlightSprite)
      {
        if (SDL_SetTextureColorMod(m_SpriteData[currentLayer].texture, 255, 255, 255))
        {
          //TODO indicates failure - an approperiate LOG message should be printed.
        }
      }

      if (transparentSprite)
      {
        if (SDL_SetTextureAlphaMod(m_SpriteData[currentLayer].texture, 255))
        {
          //TODO indicates failure - an approperiate LOG message should be printed.
        }
      }
    }
  }
}

void Sprite::refresh(const Layer &layer)
{
  std::vector<Layer> layersToGoOver;
  if (layer != Layer::NONE)
  {
    // in case this is not the default value (which is NONE), we need to update only 1 layer.
    layersToGoOver.push_back(layer);
  }
  else
  {
    layersToGoOver.insert(layersToGoOver.begin(), std::begin(allLayersOrdered), std::end(allLayersOrdered));
  }

  if (m_currentZoomLevel != Camera::zoomLevel || m_needsRefresh)
  {
    for (auto currentLayer : layersToGoOver)
    {
      if (m_SpriteData[currentLayer].texture)
      {
        m_currentZoomLevel = Camera::zoomLevel;
        int spriteSheetHeight = 0;
        SDL_QueryTexture(m_SpriteData[currentLayer].texture, nullptr, nullptr, nullptr, &spriteSheetHeight);
        // we need to offset the cliprect.y coodinate, because we've moved the "originpoint" for drawing the sprite to the screen on the bottom.
        // the sprites need to start at the bottom, so the cliprect must too.
        m_SpriteData[currentLayer].clipRect.y = spriteSheetHeight - m_SpriteData[currentLayer].clipRect.h;

        if (m_SpriteData[currentLayer].clipRect.w != 0)
        {
          m_SpriteData[currentLayer].destRect.w =
              static_cast<int>(std::round(static_cast<double>(m_SpriteData[currentLayer].clipRect.w) * m_currentZoomLevel));
          m_SpriteData[currentLayer].destRect.h =
              static_cast<int>(std::round(static_cast<double>(m_SpriteData[currentLayer].clipRect.h) * m_currentZoomLevel));
        }
        else
        {
          SDL_QueryTexture(m_SpriteData[currentLayer].texture, nullptr, nullptr, &m_SpriteData[currentLayer].destRect.w,
                           &m_SpriteData[currentLayer].destRect.h);

          m_SpriteData[currentLayer].destRect.w =
              static_cast<int>(std::round(static_cast<double>(m_SpriteData[currentLayer].clipRect.w) * m_currentZoomLevel));
          m_SpriteData[currentLayer].destRect.h =
              static_cast<int>(std::round(static_cast<double>(m_SpriteData[currentLayer].clipRect.h) * m_currentZoomLevel));
        }
      }
    }
  }

  // convert this tiles isometric coordinates to screen coordinates (with camera offset / zoomlevel taken into account.
  m_screenCoordinates = convertIsoToScreenCoordinates(isoCoordinates);

  for (auto &it : m_SpriteData)
  {
    // render the sprite in the middle of its bounding box so bigger than 1x1 sprites will render correctly
    it.destRect.x = m_screenCoordinates.x - (it.destRect.w / 2);
    // change y coordinates with sprites height taken into account to render the sprite at its base and not at its top.
    it.destRect.y = m_screenCoordinates.y - it.destRect.h;
  }

  m_needsRefresh = false;
}

void Sprite::setTexture(SDL_Texture *texture, Layer layer)
{
  if (!texture)
    throw UIError(TRACE_INFO "Called Sprite::setTexture() with a non valid texture");
  m_SpriteData[layer].texture = texture;
  m_needsRefresh = true;
  refresh(layer);
}

void Sprite::setClipRect(SDL_Rect clipRect, const Layer layer) { m_SpriteData[layer].clipRect = clipRect; }
void Sprite::setDestRect(SDL_Rect destRect, Layer layer) { m_SpriteData[layer].destRect = destRect; }

SDL_Rect Sprite::getActiveClipRect()
{
  if (MapLayers::isLayerActive(Layer::BUILDINGS) && m_SpriteData[Layer::BUILDINGS].clipRect.w != 0 &&
      m_SpriteData[Layer::BUILDINGS].clipRect.h != 0)
  {
    return m_SpriteData[Layer::BUILDINGS].clipRect;
  }
  else if (MapLayers::isLayerActive(Layer::TERRAIN))
  {
    return m_SpriteData[Layer::TERRAIN].clipRect;
  }
  return {0, 0, 0, 0};
}
bool Sprite::isLayerUsed(Layer layer)
{
  return (MapLayers::isLayerActive(layer) && m_SpriteData[layer].clipRect.w != 0 && m_SpriteData[layer].clipRect.h != 0);
}

SDL_Rect Sprite::getActiveDestRect()
{
  if (MapLayers::isLayerActive(Layer::BUILDINGS) && m_SpriteData[Layer::BUILDINGS].destRect.w != 0 &&
      m_SpriteData[Layer::BUILDINGS].destRect.h != 0)
  {
    return m_SpriteData[Layer::BUILDINGS].destRect;
  }
  else if (MapLayers::isLayerActive(Layer::TERRAIN))
  {
    return m_SpriteData[Layer::TERRAIN].destRect;
  }
  return {0, 0, 0, 0};
}

void Sprite::clearSprite(Layer layer)
{
  m_SpriteData[layer].clipRect = {0, 0, 0, 0};
  m_SpriteData[layer].destRect = {0, 0, 0, 0};
  m_SpriteData[layer].texture = nullptr;
}
