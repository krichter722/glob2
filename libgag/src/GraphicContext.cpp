/*
  Copyright (C) 2001-2004 Stephane Magnenat & Luc-Olivier de Charrière
  for any question or comment contact us at nct@ysagoon.com or nuage@ysagoon.com

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <GraphicContext.h>
#include <Toolkit.h>
#include <FileManager.h>
#include <SupportFunctions.h>
#include <assert.h>
#include <string>
#include <sstream>
#include <iostream>
#include "SDL_ttf.h"
#include <SDL_image.h>
#include <math.h>
#include <string.h>
#include <valarray>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_OPENGL
#include <GL/gl.h>
#include <GL/glu.h>
#endif

namespace GAGCore
{
	// static local pointer to the actual graphic context
	static GraphicContext *_gc = NULL;
	
	// Color
	Uint32 Color::pack() const
	{
		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		return SDL_MapRGB(_gc->sdlsurface->format, r, g, b) | (a);
		#else
		return SDL_MapRGB(_gc->sdlsurface->format, r, g, b) | (a << 24);
		#endif
	}
	
	void  Color::unpack(const Uint32 packedValue)
	{
		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		SDL_GetRGB(packedValue, _gc->sdlsurface->format, &r, &g, &b);
		a = packedValue & 0xFF;
		#else
		SDL_GetRGB(packedValue, _gc->sdlsurface->format, &r, &g, &b);
		a = packedValue >> 24;
		#endif
	}
	
	void Color::getHSV(float *hue, float *sat, float *lum)
	{
		RGBtoHSV(static_cast<float>(r)/255.0f, static_cast<float>(g)/255.0f, static_cast<float>(b)/255.0f, hue, sat, lum);
	}
	
	void Color::setHSV(float hue, float sat, float lum)
	{
		float fr, fg, fb;
		HSVtoRGB(&fr, &fg, &fb, hue, sat, lum);
		r = static_cast<Uint8>(255.0f*fr);
		g = static_cast<Uint8>(255.0f*fg);
		b = static_cast<Uint8>(255.0f*fb);
	}
	
	// Predefined colors
	Color Color::black = Color(0, 0, 0);
	Color Color::white = Color(255,255,255);
	
	#ifdef HAVE_OPENGL
	// Cache for GL state, call gl only if necessary. GL optimisations
	static struct GLState
	{
		int _doBlend;
		int _doTexture;
		int _doScissor;
		GLint _texture;
		GLenum _sfactor, _dfactor;
		bool isTextureRectangle;
	
		GLState(void)
		{
			_doBlend = -1;
			_doTexture = -1;
			_doScissor = -1;
			_texture = -1;
			_sfactor = 0xffffffff;
			_dfactor = 0xffffffff;
			isTextureRectangle = false;
			
		}
		
		void checkExtensions(void)
		{
			isTextureRectangle = (strstr((char *)glGetString(GL_EXTENSIONS), "GL_NV_texture_rectangle") != NULL);
			if (isTextureRectangle)
				std::cout << "Toolkit : GL_NV_texture_rectangle extension present, optimal texture size will be used" << std::endl;
			else
				std::cout << "Toolkit : GL_NV_texture_rectangle extension not present, power of two texture will be used" << std::endl;
		}
		
		void doBlend(int on)
		{
			if (_doBlend == on)
				return;
		
			if (on)
				glEnable(GL_BLEND);
			else
				glDisable(GL_BLEND);
			_doBlend = on;
		}
		
		void doTexture(int on)
		{
			if (_doTexture == on)
				return;
		
			GLenum cap;
			if (isTextureRectangle)
				cap = GL_TEXTURE_RECTANGLE_NV;
			else
				cap = GL_TEXTURE_2D;
			
			if (on)
				glEnable(cap);
			else
				glDisable(cap);
			_doTexture = on;
		}
		
		void setTexture(int tex)
		{
			if (_texture == tex)
				return;
		
			if (isTextureRectangle)
				glBindTexture(GL_TEXTURE_RECTANGLE_NV, tex);
			else
				glBindTexture(GL_TEXTURE_2D, tex);
			_texture = tex;
		}
		
		void doScissor(int on)
		{
			if (_doScissor == on)
				return;
		
			if (on)
				glEnable(GL_SCISSOR_TEST);
			else
				glDisable(GL_SCISSOR_TEST);
			_doScissor = on;
		}
		
		void blendFunc(GLenum sfactor, GLenum dfactor)
		{
			if ((sfactor == _sfactor) && (dfactor == _dfactor))
				return;
		
			glBlendFunc(sfactor, dfactor);
		
			_sfactor = sfactor;
			_dfactor = dfactor;
		}
	} glState;
	#endif
	
	// Drawable surface
	DrawableSurface::DrawableSurface(const char *imageFileName)
	{
		sdlsurface = NULL;
		if (!loadImage(imageFileName))
			setRes(0, 0);
		allocateTexture();
	}
	
	DrawableSurface::DrawableSurface(const std::string &imageFileName)
	{
		sdlsurface = NULL;
		if (!loadImage(imageFileName))
			setRes(0, 0);
		allocateTexture();
	}
	
	DrawableSurface::DrawableSurface(int w, int h)
	{
		sdlsurface = NULL;
		setRes(w, h);
		allocateTexture();
	}
	
	DrawableSurface::DrawableSurface(const SDL_Surface *sourceSurface)
	{
		assert(sourceSurface);
		// beurk, const cast here becasue SDL API sucks
		sdlsurface = SDL_DisplayFormatAlpha(const_cast<SDL_Surface *>(sourceSurface));
		assert(sdlsurface);
		setClipRect();
		allocateTexture();
		dirty = true;
	}
	
	DrawableSurface *DrawableSurface::clone(void)
	{
		return new DrawableSurface(sdlsurface);
	}
	
	DrawableSurface::~DrawableSurface(void)
	{
		SDL_FreeSurface(sdlsurface);
		freeGPUTexture();
	}
	
	template<typename T>
	T getMinPowerOfTwo(T t)
	{
		T v = 1;
		while (v < t)
			v *= 2;
		return v;
	}
	
	void DrawableSurface::allocateTexture(void)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
		{
			glGenTextures(1, &texture);
			initTextureSize();
		}
		#endif
	}
	
	void DrawableSurface::initTextureSize(void)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
		{
			// only power of two textures are supported
			if (!glState.isTextureRectangle)
			{
				// TODO : if anyone has a better way to do it, please tell :-)
				glState.setTexture(texture);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
				
				int w = getMinPowerOfTwo(sdlsurface->w);
				int h = getMinPowerOfTwo(sdlsurface->h);
				std::valarray<char> zeroBuffer((char)0, w * h * 4);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, &zeroBuffer[0]);
				
				texMultX = 1.0f / static_cast<float>(w);
				texMultY = 1.0f / static_cast<float>(h);
			}
			else
			{
				texMultX = 1.0f;
				texMultY = 1.0f;
			}
		}
		#endif
	}
	
	void DrawableSurface::uploadToTexture(void)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
		{
			glState.setTexture(texture);
			
			if (glState.isTextureRectangle)
			{
				glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA, sdlsurface->w, sdlsurface->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, sdlsurface->pixels);
			}
			else
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sdlsurface->w, sdlsurface->h, GL_BGRA, GL_UNSIGNED_BYTE, sdlsurface->pixels);
			}
		}
		#endif
		dirty = false;
	}
	
	void DrawableSurface::freeGPUTexture(void)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
			glDeleteTextures(1, &texture);
		#endif
	}
	
	void DrawableSurface::setRes(int w, int h)
	{
		if (sdlsurface)
			SDL_FreeSurface(sdlsurface);
		
		Uint32 rmask = _gc->sdlsurface->format->Rmask;
		Uint32 gmask = _gc->sdlsurface->format->Gmask;
		Uint32 bmask = _gc->sdlsurface->format->Bmask;
		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		Uint32 amask = 0x000000ff;
		#else
		Uint32 amask = 0xff000000;
		#endif
		sdlsurface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, rmask, gmask, bmask, amask);
		assert(sdlsurface);
		setClipRect();
		initTextureSize();
		dirty = true;
	}
	
	void DrawableSurface::getClipRect(int *x, int *y, int *w, int *h)
	{
		assert(x);
		assert(y);
		assert(w);
		assert(h);
		
		*x = clipRect.x;
		*y = clipRect.y;
		*w = clipRect.w;
		*h = clipRect.h;
	}
	
	void DrawableSurface::setClipRect(int x, int y, int w, int h)
	{
		assert(sdlsurface);
		
		clipRect.x = static_cast<Sint16>(x);
		clipRect.y = static_cast<Sint16>(y);
		clipRect.w = static_cast<Uint16>(w);
		clipRect.h = static_cast<Uint16>(h);
	
		SDL_SetClipRect(sdlsurface, &clipRect);
	}
	
	void DrawableSurface::setClipRect(void)
	{
		assert(sdlsurface);
		
		clipRect.x = 0;
		clipRect.y = 0;
		clipRect.w = static_cast<Uint16>(sdlsurface->w);
		clipRect.h = static_cast<Uint16>(sdlsurface->h);
	
		SDL_SetClipRect(sdlsurface, &clipRect);
	}
	
	bool DrawableSurface::loadImage(const char *name)
	{
		if (name)
		{
			SDL_RWops *imageStream;
			if ((imageStream = Toolkit::getFileManager()->open(name, "rb")) != NULL)
			{
				SDL_Surface *loadedSurface;
				loadedSurface = IMG_Load_RW(imageStream, 0);
				SDL_RWclose(imageStream);
				if (loadedSurface)
				{
					if (sdlsurface)
						SDL_FreeSurface(sdlsurface);
					sdlsurface = SDL_DisplayFormatAlpha(loadedSurface);
					assert(sdlsurface);
					SDL_FreeSurface(loadedSurface);
					setClipRect();
					dirty = true;
					return true;
				}
			}
		}
		return false;
	}
	
	bool DrawableSurface::loadImage(const std::string &name)
	{
		return loadImage(name.c_str());
	}
	
	void DrawableSurface::shiftHSV(float hue, float sat, float lum)
	{
		Uint32 *mem = (Uint32 *)sdlsurface->pixels;
		for (size_t i = 0; i < static_cast<size_t>(sdlsurface->w * sdlsurface->h); i++)
		{
			// get values
			float h, s, v;
			Color c;
			c.unpack(*mem);
			c.getHSV(&h, &s, &v);
			
			// shift
			h += hue;
			s += sat;
			v += lum;
			
			// wrap and saturate
			if (h >= 360.0f)
				h -= 360.0f;
			if (h < 0.0f)
				h += 360.0f;
			s = std::max(s, 0.0f);
			s = std::min(s, 1.0f);
			v = std::max(v, 0.0f);
			v = std::min(v, 1.0f);
			
			// set values
			c.setHSV(h, s, v);
			*mem = c.pack();
			mem++;
		}
		dirty = true;
	}
	
	void DrawableSurface::drawPixel(int x, int y, Color color)
	{
		// clip
		if ((x<clipRect.x) || (x>=clipRect.x+clipRect.w) || (y<clipRect.y) || (y>=clipRect.y+clipRect.h))
			return;
		
		// draw
		if (color.a == Color::ALPHA_OPAQUE)
		{
			*(((Uint32 *)sdlsurface->pixels) + y*(sdlsurface->pitch>>2) + x) = color.pack();
		}
		else
		{
			Uint32 a = color.a;
			Uint32 na = 255 - a;
			color.a = Color::ALPHA_OPAQUE;
			Uint32 colorValue = color.pack();
			Uint32 colorPreMult0 = (colorValue & 0x00FF00FF) * a;
			Uint32 colorPreMult1 = ((colorValue >> 8) & 0x00FF00FF) * a;
			
			Uint32 *mem = ((Uint32 *)sdlsurface->pixels) + y*(sdlsurface->pitch>>2) + x;
			
			Uint32 surfaceValue = *mem;
			Uint32 surfacePreMult0 = (surfaceValue & 0x00FF00FF) * na;
			Uint32 surfacePreMult1 = ((surfaceValue >> 8) & 0x00FF00FF) * na;
			
			surfacePreMult0 += colorPreMult0;
			surfacePreMult1 += colorPreMult1;
			
			*mem = ((surfacePreMult0 >> 8) & 0x00FF00FF) | (surfacePreMult1 & 0xFF00FF00);
		}
		dirty = true;
	}
	
	void DrawableSurface::drawPixel(float x, float y, Color color)
	{
		drawPixel(static_cast<int>(x), static_cast<int>(y), color);
	}
	
	// compat
	void DrawableSurface::drawPixel(int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawPixel(x, y, Color(r, g, b, a));
	}
	
	void DrawableSurface::drawRect(int x, int y, int w, int h, Color color)
	{
		_drawHorzLine(x, y, w, color);
		_drawHorzLine(x, y+h-1, w, color);
		_drawVertLine(x, y, h, color);
		_drawVertLine(x+w-1, y, h, color);
	}
	
	void DrawableSurface::drawRect(float x, float y, float w, float h, Color color)
	{
		drawRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), color);
	}
	
	// compat
	void DrawableSurface::drawRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawRect(x, y, w, h, Color(r, g, b, a));
	}
	
	void DrawableSurface::drawFilledRect(int x, int y, int w, int h, Color color)
	{
		// clip
		if (x < clipRect.x)
		{
			w -= clipRect.x - x;
			x = clipRect.x;
		}
		if (y < 0)
		{
			h -= clipRect.y - y;
			y = clipRect.y;
		}
		if (x + w >= clipRect.x + clipRect.w)
		{
			w = clipRect.x + clipRect.w - x;
		}
		if (y + h >= clipRect.y + clipRect.h)
		{
			h = clipRect.y + clipRect.h - y;
		}
		if ((w <= 0) || (h <= 0))
			return;
			
		// draw
		if (color.a == Color::ALPHA_OPAQUE)
		{
			Uint32 colorValue = color.pack();
			for (int dy = y; dy < y + h; dy++)
			{
				Uint32 *mem = ((Uint32 *)sdlsurface->pixels) + dy*(sdlsurface->pitch>>2) + x;
				int dw = w;
				do
				{
					*mem++ = colorValue;
				}
				while (--dw);
			}
		}
		else
		{
			Uint32 a = color.a;
			Uint32 na = 255 - a;
			color.a = Color::ALPHA_OPAQUE;
			Uint32 colorValue = color.pack();
			Uint32 colorPreMult0 = (colorValue & 0x00FF00FF) * a;
			Uint32 colorPreMult1 = ((colorValue >> 8) & 0x00FF00FF) * a;
			
			for (int dy = y; dy < y + h; dy++)
			{
				Uint32 *mem = ((Uint32 *)sdlsurface->pixels) + dy*(sdlsurface->pitch>>2) + x;
				int dw = w;
				do
				{
					Uint32 surfaceValue = *mem;
					Uint32 surfacePreMult0 = (surfaceValue & 0x00FF00FF) * na;
					Uint32 surfacePreMult1 = ((surfaceValue >> 8) & 0x00FF00FF) * na;
					surfacePreMult0 += colorPreMult0;
					surfacePreMult1 += colorPreMult1;
					*mem++ = ((surfacePreMult0 >> 8) & 0x00FF00FF) | (surfacePreMult1 & 0xFF00FF00);
				}
				while (--dw);
			}
		}
		dirty = true;
	}
	
	void DrawableSurface::drawFilledRect(float x, float y, float w, float h, Color color)
	{
		drawFilledRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), color);
	}
	
	void DrawableSurface::drawFilledRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawFilledRect(x, y, w, h, Color(r, g, b, a));
	}
	
	void DrawableSurface::_drawVertLine(int x, int y, int l, Color color)
	{
		// clip
		// be sure we have to draw something
		if ((x < clipRect.x) || (x >= clipRect.x + clipRect.w))
			return;
		
		// set l positiv
		if (l < 0)
		{
			y += l;
			l = -l;
		}
	
		// clip on y at top
		if (y < clipRect.y)
		{
			l -= clipRect.y - y;
			y = clipRect.y;
		}
	
		// clip on y at bottom
		if (y + l >= clipRect.y + clipRect.h)
		{
			l = clipRect.y + clipRect.h - y;
		}
	
		// again, be sure we have to draw something
		if (l <= 0)
			return;
			
		// draw
		int increment = sdlsurface->pitch >> 2;
		Uint32 *mem = ((Uint32 *)sdlsurface->pixels) + y*increment + x;
		if (color.a == Color::ALPHA_OPAQUE)
		{
			Uint32 colorValue = color.pack();
			
			do
			{
				*mem = colorValue;
				mem += increment;
			}
			while (--l);
		}
		else
		{
			Uint32 a = color.a;
			Uint32 na = 255 - a;
			color.a = Color::ALPHA_OPAQUE;
			Uint32 colorValue = color.pack();
			Uint32 colorPreMult0 = (colorValue & 0x00FF00FF) * a;
			Uint32 colorPreMult1 = ((colorValue >> 8) & 0x00FF00FF) * a;
			
			do
			{
				Uint32 surfaceValue = *mem;
				Uint32 surfacePreMult0 = (surfaceValue & 0x00FF00FF) * na;
				Uint32 surfacePreMult1 = ((surfaceValue >> 8) & 0x00FF00FF) * na;
				surfacePreMult0 += colorPreMult0;
				surfacePreMult1 += colorPreMult1;
				*mem = ((surfacePreMult0 >> 8) & 0x00FF00FF) | (surfacePreMult1 & 0xFF00FF00);
				mem += increment;
			}
			while (--l);
		}
		dirty = true;
	}
	
	void DrawableSurface::_drawHorzLine(int x, int y, int l, Color color)
	{
		// clip
		// be sure we have to draw something
		if ((y < clipRect.y) || (y >= clipRect.y + clipRect.h))
			return;
		
		// set l positiv
		if (l < 0)
		{
			x += l;
			l = -l;
		}
	
		// clip on x at left
		if (x < clipRect.x)
		{
			l -= clipRect.x - x;
			x = clipRect.x;
		}
	
		// clip on x at right
		if ( x + l >= clipRect.x + clipRect.w)
		{
			l = clipRect.x + clipRect.w - x;
		}
	
		// again, be sure we have to draw something
		if (l <= 0)
			return;
		
		// draw
		Uint32 *mem = ((Uint32 *)sdlsurface->pixels) + y*(sdlsurface->pitch >> 2) + x;
		if (color.a == Color::ALPHA_OPAQUE)
		{
			Uint32 colorValue = color.pack();
			
			do
			{
				*mem++ = colorValue;
			}
			while (--l);
		}
		else
		{
			Uint32 a = color.a;
			Uint32 na = 255 - a;
			color.a = Color::ALPHA_OPAQUE;
			Uint32 colorValue = color.pack();
			Uint32 colorPreMult0 = (colorValue & 0x00FF00FF) * a;
			Uint32 colorPreMult1 = ((colorValue >> 8) & 0x00FF00FF) * a;
			
			do
			{
				Uint32 surfaceValue = *mem;
				Uint32 surfacePreMult0 = (surfaceValue & 0x00FF00FF) * na;
				Uint32 surfacePreMult1 = ((surfaceValue >> 8) & 0x00FF00FF) * na;
				surfacePreMult0 += colorPreMult0;
				surfacePreMult1 += colorPreMult1;
				*mem++ = ((surfacePreMult0 >> 8) & 0x00FF00FF) | (surfacePreMult1 & 0xFF00FF00);
			}
			while (--l);
		}
		dirty = true;
	}
	
	void DrawableSurface::drawLine(int x1, int y1, int x2, int y2, Color color)
	{
		// compute deltas
		int dx = x2 - x1;
		if (dx == 0)
		{
			_drawVertLine(x1, y1, y2-y1, color);
			return;
		}
		int dy = y2 - y1;
		if (dy == 0)
		{
			_drawHorzLine(x1, y1, x2-x1, color);
			return;
		}
		
		// clip
		int test = 1;
		// Y clipping
		if (dy < 0)
		{
			test = -test;
			std::swap(x1, x2);
			std::swap(y1, y2);
			dx = -dx;
			dy = -dy;
		}
		
		// the 2 points are Y-sorted. (y1 <= y2)
		if (y2 < clipRect.y)
			return;
		if (y1 >= clipRect.y + clipRect.h)
			return;
		if (y1 < clipRect.y)
		{
			x1 = x2 - ( (y2 - clipRect.y)*(x2-x1) ) / (y2-y1);
			y1 = clipRect.y;
		}
		if (y1 == y2)
		{
			_drawHorzLine(x1, y1, x2-x1, color);
			return;
		}
		if (y2 >= clipRect.y + clipRect.h)
		{
			x2 = x1 - ( (y1 - (clipRect.y + clipRect.h))*(x1-x2) ) / (y1-y2);
			y2 = (clipRect.y + clipRect.h - 1);
		}
		if (x1 == x2)
		{
			_drawVertLine(x1, y1, y2-y1, color);
			return;
		}
	
		// X clipping
		if (dx < 0)
		{
			test = -test;
			std::swap(x1, x2);
			std::swap(y1, y2);
			dx = -dx;
			dy = -dy;
		}
		// the 2 points are X-sorted. (x1 <= x2)
		if (x2 < clipRect.x)
			return;
		if (x1 >= clipRect.x + clipRect.w)
			return;
		if (x1 < clipRect.x)
		{
			y1 = y2 - ( (x2 - clipRect.x)*(y2-y1) ) / (x2-x1);
			x1 = clipRect.x;
		}
		if (x1 == x2)
		{
			_drawVertLine(x1, y1, y2-y1, color);
			return;
		}
		if (x2 >= clipRect.x + clipRect.w)
		{
			y2 = y1 - ( (x1 - (clipRect.x + clipRect.w))*(y1-y2) ) / (x1-x2);
			x2 = (clipRect.x + clipRect.w - 1);
		}
	
		// last return case
		if (x1 >= (clipRect.x + clipRect.w) || y1 >= (clipRect.y + clipRect.h) || (x2 < clipRect.x) || (y2 < clipRect.y))
			return;
	
		// recompute deltas after clipping
		dx = x2-x1;
		dy = y2-y1;
		
		// setup variable to draw alpha in the right direction
		#define Sgn(x) (x>0 ? (x == 0 ? 0 : 1) : (x==0 ? 0 : -1))
		Sint32 littleincx;
		Sint32 littleincy;
		Sint32 bigincx;
		Sint32 bigincy;
		Sint32 alphadecx;
		Sint32 alphadecy;
		if (abs(dx) > abs(dy))
		{
			littleincx = 1;
			littleincy = 0;
			bigincx = 1;
			bigincy = Sgn(dy);
			alphadecx = 0;
			alphadecy = Sgn(dy);
		}
		else
		{
			// we swap x and y meaning
			test = -test;
			std::swap(dx, dy);
			littleincx = 0;
			littleincy = 1;
			bigincx = Sgn(dx);
			bigincy = 1;
			alphadecx = 1;
			alphadecy = 0;
		}
	
		if (dx < 0)
		{
			dx = -dx;
			littleincx = 0;
			littleincy = -littleincy;
			bigincx = -bigincx;
			bigincy = -bigincy;
			alphadecy = -alphadecy;
		}
	
		// compute initial position
		int px, py;
		px = x1;
		py = y1;
	
		// variable initialisation for bresenham algo
		if (dx == 0)
			return;
		if (dy == 0)
			return;
		const int FIXED = 8;
		const int I = 255; // number of degree of alpha
		const int Ibits = 8;
		int m = (abs(dy) << (Ibits+FIXED)) / abs(dx);
		int w = (I << FIXED) - m;
		int e = 1 << (FIXED-1);
	
		// first point
		color.a = I - (e >> FIXED);
		drawPixel(px, py, color);
	
		// main loop
		int x = dx+1;
		if (x <= 0)
			return;
		while (--x)
		{
			if (e < w)
			{
				px+=littleincx;
				py+=littleincy;
				e+= m;
			}
			else
			{
				px+=bigincx;
				py+=bigincy;
				e-= w;
			}
			color.a = I - (e >> FIXED);
			drawPixel(px, py, color);
			color.a = e >> FIXED;
			drawPixel(px + alphadecx, py + alphadecy, color);
		}
	}
	
	void DrawableSurface::drawLine(float x1, float y1, float x2, float y2, Color color)
	{
		drawRect(static_cast<int>(x1), static_cast<int>(y1), static_cast<int>(x2), static_cast<int>(y2), color);
	}
	
	// compat
	void DrawableSurface::drawVertLine(int x, int y, int l, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		 _drawVertLine(x, y, l, Color(r, g, b, a));
	}
	// compat
	void DrawableSurface::drawHorzLine(int x, int y, int l, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		_drawHorzLine(x, y, l, Color(r, g, b, a));
	}
	// compat
	void DrawableSurface::drawLine(int x1, int y1, int x2, int y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawLine(x1, y1, x2, y2, Color(r, g, b, a));
	}
	
	void DrawableSurface::drawCircle(int x, int y, int radius, Color color)
	{
		// clip
		if ((x<clipRect.x) || (x>=clipRect.x+clipRect.w) || (y<clipRect.y) || (y>=clipRect.y+clipRect.h))
			return;
		
		// draw
		int dx, dy, d;
		int rdx, rdy;
		int i;
		color.a >>= 2;
		for (i=0; i<3; i++)
		{
			dx = 0;
			dy = (radius<<1) + i;
			d = 0;
		
			do
			{
				rdx = (dx>>1);
				rdy = (dy>>1);
				drawPixel(x+rdx, y+rdy, color);
				drawPixel(x+rdx, y-rdy, color);
				drawPixel(x-rdx, y+rdy, color);
				drawPixel(x-rdx, y-rdy, color);
				drawPixel(x+rdy, y+rdx, color);
				drawPixel(x+rdy, y-rdx, color);
				drawPixel(x-rdy, y+rdx, color);
				drawPixel(x-rdy, y-rdx, color);
				dx++;
				if (d >= 0)
				{
					dy--;
					d += ((dx-dy)<<1)+2;
				}
				else
				{
					d += (dx<<1) +1;
				}
			}
			while (dx <= dy);
		}
	}
	
	void DrawableSurface::drawCircle(float x, float y, float radius, Color color)
	{
		drawCircle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(radius), color);
	}
	
	// compat
	void DrawableSurface::drawCircle(int x, int y, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawCircle(x, y, radius, Color(r, g, b, a));
	}
	
	void DrawableSurface::drawSurface(int x, int y, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void DrawableSurface::drawSurface(float x, float y, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void DrawableSurface::drawSurface(int x, int y, int w, int h, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, w, h, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void DrawableSurface::drawSurface(float x, float y, float w, float h, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, w, h, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void DrawableSurface::drawSurface(int x, int y, DrawableSurface *surface, int sx, int sy, int sw, int sh, Uint8 alpha)
	{
		if (alpha == Color::ALPHA_OPAQUE)
		{
			// well, we *hope* SDL is faster than a handmade code
			SDL_Rect sr, dr;
			sr.x = static_cast<Sint16>(sx);
			sr.y = static_cast<Sint16>(sy);
			sr.w = static_cast<Uint16>(sw);
			sr.h = static_cast<Uint16>(sh);
			dr.x = static_cast<Sint16>(x);
			dr.y = static_cast<Sint16>(y);
			dr.w = static_cast<Uint16>(sw);
			dr.h = static_cast<Uint16>(sh);
			SDL_BlitSurface(surface->sdlsurface, &sr, sdlsurface, &dr);
		}
		else
		{
			// check we assume the source rect is within the source surface
			assert((sx >= 0) && (sx < surface->getW()));
			assert((sy >= 0) && (sy < surface->getH()));
			assert((sw > 0) && (sx + sw <= surface->getW()));
			assert((sh > 0) && (sy + sh <= surface->getH()));
			
			// clip
			if (x < clipRect.x)
			{
				int diff = clipRect.x - x;
				sw -= diff;
				sx += diff;
				x = clipRect.x;
			}
			if (y < 0)
			{
				int diff = clipRect.y - y;
				sh -= diff;
				sy += diff;
				y = clipRect.y;
			}
			if (x + sw >= clipRect.x + clipRect.w)
			{
				sw = clipRect.x + clipRect.w - x;
			}
			if (y + sh >= clipRect.y + clipRect.h)
			{
				sh = clipRect.y + clipRect.h - y;
			}
			if ((sw <= 0) || (sh <= 0))
				return;
			
			// draw
			#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			Uint32 alphaShift = 0;
			#else
			Uint32 alphaShift = 24;
			#endif
			for (int dy = 0; dy < sh; dy++)
			{
				Uint32 *memSrc = ((Uint32 *)surface->sdlsurface->pixels) + (sy + dy)*(surface->sdlsurface->pitch>>2) + sx;
				Uint32 *memDest = ((Uint32 *)sdlsurface->pixels) + (y + dy)*(sdlsurface->pitch>>2) + x;
				int dw = sw;
				do
				{
					Uint32 srcValue = *memSrc++;
					Uint32 srcAlpha = (((srcValue >> alphaShift) & 0xFF) * alpha) >> 8;
					Uint32 destAlpha = 255 - srcAlpha;
					Uint32 srcPreMult0 =  (srcValue & 0x00FF00FF) * srcAlpha;
					Uint32 srcPreMult1 = ((srcValue >> 8) & 0x00FF00FF) * srcAlpha;
					
					Uint32 destValue = *memDest;
					Uint32 destPreMult0 =  (destValue & 0x00FF00FF) * destAlpha;
					Uint32 destPreMult1 = ((destValue >> 8) & 0x00FF00FF) * destAlpha;
					
					destPreMult0 += srcPreMult0;
					destPreMult1 += srcPreMult1;
					
					*memDest++ = ((destPreMult0 >> 8) & 0x00FF00FF) | (destPreMult1 & 0xFF00FF00);
				}
				while (--dw);
			}
		}
		dirty = true;
	}
	
	void DrawableSurface::drawSurface(float x, float y, DrawableSurface *surface, int sx, int sy, int sw, int sh, Uint8 alpha)
	{
		drawSurface(static_cast<int>(x), static_cast<int>(y), surface, sx, sy, sw, sh, alpha);
	}
	
	void DrawableSurface::drawSurface(int x, int y, int w, int h, DrawableSurface *surface, int sx, int sy, int sw, int sh,  Uint8 alpha)
	{
		// TODO : Implement
	}
	
	void DrawableSurface::drawSurface(float x, float y, float w, float h, DrawableSurface *surface, int sx, int sy, int sw, int sh, Uint8 alpha)
	{
		drawSurface(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), surface, sx, sy, sw, sh, alpha);
	}
	
	void DrawableSurface::drawSprite(int x, int y, Sprite *sprite, unsigned index,  Uint8 alpha)
	{
		// check bounds
		assert(sprite);
		if (!sprite->checkBound(index))
			return;
		
		// draw background
		if (sprite->images[index])
			drawSurface(x, y, sprite->images[index], alpha);
			
		// draw rotation
		if (sprite->rotated[index])
			drawSurface(x, y, sprite->getRotatedSurface(index), alpha);
	}
	
	void DrawableSurface::drawSprite(float x, float y, Sprite *sprite, unsigned index,  Uint8 alpha)
	{
		// check bounds
		assert(sprite);
		if (!sprite->checkBound(index))
			return;
		
		// draw background
		if (sprite->images[index])
			drawSurface(x, y, sprite->images[index], alpha);
			
		// draw rotation
		if (sprite->rotated[index])
			drawSurface(x, y, sprite->getRotatedSurface(index), alpha);
	}
	
	void DrawableSurface::drawSprite(int x, int y, int w, int h, Sprite *sprite, unsigned index, Uint8 alpha)
	{
		// check bounds
		assert(sprite);
		if (!sprite->checkBound(index))
			return;
		
		// draw background
		if (sprite->images[index])
			drawSurface(x, y, w, h, sprite->images[index], alpha);
			
		// draw rotation
		if (sprite->rotated[index])
			drawSurface(x, y, w, h, sprite->getRotatedSurface(index), alpha);
	}
	
	void DrawableSurface::drawSprite(float x, float y, float w, float h, Sprite *sprite, unsigned index, Uint8 alpha)
	{
		// check bounds
		assert(sprite);
		if (!sprite->checkBound(index))
			return;
		
		// draw background
		if (sprite->images[index])
			drawSurface(x, y, w, h, sprite->images[index], alpha);
			
		// draw rotation
		if (sprite->rotated[index])
			drawSurface(x, y, w, h, sprite->getRotatedSurface(index), alpha);
	}
	
	void DrawableSurface::drawString(int x, int y, Font *font, const char *msg, int w, Uint8 alpha)
	{
		std::string output(msg);
		
		// usefull macro to replace some char (like newline) with \0 in string
		#define FILTER_OUT_CHAR(s, c) { char *_c; if ( (_c=(strchr(s, c)))!=NULL) *_c=0; }
		
		FILTER_OUT_CHAR(output.c_str(), '\n');
		FILTER_OUT_CHAR(output.c_str(), '\r');
		
		#undef FILTER_OUT_CHAR
		
		// TODO : rewrite this with clean C++
		
		font->drawString(this, x, y, w, output.c_str(), alpha);
	}
	
	void DrawableSurface::drawString(float x, float y, Font *font, const char *msg, float w, Uint8 alpha)
	{
		std::string output(msg);
		
		// usefull macro to replace some char (like newline) with \0 in string
		#define FILTER_OUT_CHAR(s, c) { char *_c; if ( (_c=(strchr(s, c)))!=NULL) *_c=0; }
		
		FILTER_OUT_CHAR(output.c_str(), '\n');
		FILTER_OUT_CHAR(output.c_str(), '\r');
		
		#undef FILTER_OUT_CHAR
		
		// TODO : rewrite this with clean C++
		
		font->drawString(this, x, y, w, output.c_str(), alpha);
	}
	
	void DrawableSurface::drawString(int x, int y, Font *font, const std::string &msg, int w, Uint8 alpha)
	{
		drawString(x, y, font, msg.c_str(), w, alpha);
	}
	
	void DrawableSurface::drawString(float x, float y, Font *font, const std::string &msg, float w, Uint8 alpha)
	{
		drawString(x, y, font, msg.c_str(), w, alpha);
	}
	
	// compat
	void DrawableSurface::drawString(int x, int y, Font *font, int i)
	{
		std::stringstream str;
		str << i;
		this->drawString(x, y, font, str.str());
	}
	
	// here begin the Graphic Context part
	
	void GraphicContext::setClipRect(int x, int y, int w, int h)
	{
		DrawableSurface::setClipRect(x, y, w, h);
		glState.doScissor(1);
		glScissor(clipRect.x, getH() - clipRect.y - clipRect.h, clipRect.w, clipRect.h);
	}
	
	void GraphicContext::setClipRect(void)
	{
		DrawableSurface::setClipRect();
		glState.doScissor(0);
	}
	
	// drawing, reimplementation for GL
	
	void GraphicContext::drawPixel(int x, int y, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
			GraphicContext::drawPixel(static_cast<float>(x), static_cast<float>(y), color);
		else
		#endif
			DrawableSurface::drawPixel(x, y, color);
	}
	
	void GraphicContext::drawPixel(float x, float y, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
		{
			// state change
			glState.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glState.doBlend(1);
			glState.doTexture(0);
	
			// draw
			glBegin(GL_POINT);
			if (color.a < 255)
				glColor4ub(color.r, color.g, color.b, color.a);
			else
				glColor3ub(color.r, color.g, color.b);
			glVertex2f(x, y);
			glEnd();
		}
		else
		#endif
			DrawableSurface::drawPixel(static_cast<int>(x), static_cast<int>(y), color);
	}
	
	
	void GraphicContext::drawRect(int x, int y, int w, int h, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
			GraphicContext::drawRect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), color);
		else
		#endif
			DrawableSurface::drawRect(x, y, w, h, color);
	}
	
	void GraphicContext::drawRect(float x, float y, float w, float h, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
		{
			// state change
			glState.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glState.doBlend(1);
			glState.doTexture(0);
	
			// draw
			glBegin(GL_LINES);
			if (color.a < 255)
				glColor4ub(color.r, color.g, color.b, color.a);
			else
				glColor3ub(color.r, color.g, color.b);
			glVertex2f(x, y);     glVertex2f(x+w, y);
			glVertex2f(x+w, y);   glVertex2f(x+w, y+h);
			glVertex2f(x+w, y+h); glVertex2f(x, y+h);
			glVertex2f(x, y+h);   glVertex2f(x, y);
			glEnd();
		}
		else
		#endif
			DrawableSurface::drawRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), color);
	}
	
	
	void GraphicContext::drawFilledRect(int x, int y, int w, int h, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
			GraphicContext::drawFilledRect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), color);
		else
		#endif
			DrawableSurface::drawFilledRect(x, y, w, h, color);
	}
	
	void GraphicContext::drawFilledRect(float x, float y, float w, float h, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
		{
			// state change
			if (color.a < 255)
				glState.doBlend(1);
			else
				glState.doBlend(0);
			glState.doTexture(0);
		
			// draw
			glBegin(GL_QUADS);
			if (color.a < 255)
				glColor4ub(color.r, color.g, color.b, color.a);
			else
				glColor3ub(color.r, color.g, color.b);
			glVertex2f(x, y);
			glVertex2f(x+w, y);
			glVertex2f(x+w, y+h);
			glVertex2f(x, y+h);
			glEnd();
		}
		else
		#endif
			DrawableSurface::drawFilledRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), color);
	}
	
	
	void GraphicContext::drawLine(int x1, int y1, int x2, int y2, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
			GraphicContext::drawLine(static_cast<float>(x1), static_cast<float>(y1), static_cast<float>(x2), static_cast<float>(y2), color);
		else
		#endif
			DrawableSurface::drawLine(x1, y1, x2, y2, color);
	}
	
	void GraphicContext::drawLine(float x1, float y1, float x2, float y2, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
		{
			// state change
			glState.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glState.doBlend(1);
			glState.doTexture(0);
			
			// draw
			glBegin(GL_LINES);
			if (color.a < 255)
				glColor4ub(color.r, color.g, color.b, color.a);
			else
				glColor3ub(color.r, color.g, color.b);
			glVertex2f(x1, y1);
			glVertex2f(x2, y2);
			glEnd();
		}
		else
		#endif
			DrawableSurface::drawLine(static_cast<int>(x1), static_cast<int>(y1), static_cast<int>(x2), static_cast<int>(y2), color);
	}
	
	
	void GraphicContext::drawCircle(int x, int y, int radius, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
			drawCircle(static_cast<float>(x), static_cast<float>(y), static_cast<float>(radius), color);
		else
		#endif
			DrawableSurface::drawCircle(x, y, radius, color);
	}
	
	void GraphicContext::drawCircle(float x, float y, float radius, Color color)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
		{
			glState.doBlend(1);
			glState.doTexture(0);
			glLineWidth(2);
		
			double tot = radius;
			double fx = x;
			double fy = y;
			double fray = radius;
		
			glBegin(GL_LINES);
			if (color.a < 255)
				glColor4ub(color.r, color.g, color.b, color.a);
			else
				glColor3ub(color.r, color.g, color.b);
			for (int i=0; i<tot; i++)
			{
				double angle0 = (2*M_PI*(double)i)/((double)tot);
				double angle1 = (2*M_PI*(double)(i+1))/((double)tot);
				glVertex2d(fx+fray*sin(angle0), fy+fray*cos(angle0));
				glVertex2d(fx+fray*sin(angle1), fy+fray*cos(angle1));
			}
			glEnd();
			glLineWidth(1);
		}
		else
		#endif
			DrawableSurface::drawCircle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(radius), color);
	}
	
	void GraphicContext::drawSurface(int x, int y, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void GraphicContext::drawSurface(float x, float y, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void GraphicContext::drawSurface(int x, int y, int w, int h, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, w, h, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void GraphicContext::drawSurface(float x, float y, float w, float h, DrawableSurface *surface, Uint8 alpha)
	{
		drawSurface(x, y, w, h, surface, 0, 0, surface->getW(), surface->getH(), alpha);
	}
	
	void GraphicContext::drawSurface(int x, int y, DrawableSurface *surface, int sx, int sy, int sw, int sh, Uint8 alpha)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
			drawSurface(x, y, sw, sh, surface, sx, sy, sw, sh, alpha);
		else
		#endif
			DrawableSurface::drawSurface(x, y, surface, sx, sy, sw, sh, alpha);
	}
	
	void GraphicContext::drawSurface(float x, float y, DrawableSurface *surface, int sx, int sy, int sw, int sh, Uint8 alpha)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
			drawSurface(x, y, static_cast<float>(sw), static_cast<float>(sh), surface, sx, sy, sw, sh, alpha);
		else
		#endif
			DrawableSurface::drawSurface(static_cast<int>(x), static_cast<int>(y), surface, sx, sy, sw, sh, alpha);
	}
	
	void GraphicContext::drawSurface(int x, int y, int w, int h, DrawableSurface *surface, int sx, int sy, int sw, int sh,  Uint8 alpha)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
			GraphicContext::drawSurface(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), surface, sx, sy, sw, sh, alpha);
		else
		#endif
			DrawableSurface::drawSurface(x, y, w, h, surface, sx, sy, sw, sh, alpha);
	}
	
	void GraphicContext::drawSurface(float x, float y, float w, float h, DrawableSurface *surface, int sx, int sy, int sw, int sh, Uint8 alpha)
	{
		#ifdef HAVE_OPENGL
		if (_gc->optionFlags & GraphicContext::USEGPU)
		{
			// upload
			if (surface->dirty)
				surface->uploadToTexture();
			
			// state change
			glState.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glState.doBlend(1);
			glState.doTexture(1);
			glColor4ub(255, 255, 255, alpha);
			
			// draw
			glState.setTexture(surface->texture);
			glBegin(GL_QUADS);
			glTexCoord2f(static_cast<float>(sx) * surface->texMultX, static_cast<float>(sy) * surface->texMultY);
			glVertex2f(x, y);
			glTexCoord2f(static_cast<float>(sx + sw) * surface->texMultX, static_cast<float>(sy) * surface->texMultY);
			glVertex2f(x+w, y);
			glTexCoord2f(static_cast<float>(sx + sw) * surface->texMultX, static_cast<float>(sy + sh) * surface->texMultY);
			glVertex2f(x+w, y+h);
			glTexCoord2f(static_cast<float>(sx) * surface->texMultX, static_cast<float>(sy + sh) * surface->texMultY);
			glVertex2f(x, y+h);
			glEnd();
		}
		else
		#endif
			DrawableSurface::drawSurface(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h), surface, sx, sy, sw, sh, alpha);
	}
	
	// compat... this is there because it sems gc is not able to do function overloading with several levels of inheritance
	void GraphicContext::drawPixel(int x, int y, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawPixel(x, y, Color(r, g, b, a));
	}
	
	void GraphicContext::drawRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawRect(x, y, w, h, Color(r, g, b, a));
	}
	
	void GraphicContext::drawFilledRect(int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawFilledRect(x, y, w, h, Color(r, g, b, a));
	}
	
	void GraphicContext::drawLine(int x1, int y1, int x2, int y2, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawLine(x1, y1, x2, y2, Color(r, g, b, a));
	}
	
	void GraphicContext::drawVertLine(int x, int y, int l, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
			drawLine(x, y, x, y+l, Color(r, g, b, a));
		else
		#endif
			 _drawVertLine(x, y, l, Color(r, g, b, a));
	}
	
	void GraphicContext::drawHorzLine(int x, int y, int l, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		#ifdef HAVE_OPENGL
		if (optionFlags & GraphicContext::USEGPU)
			drawLine(x, y, x+l, y, Color(r, g, b, a));
		else
		#endif
			_drawHorzLine(x, y, l, Color(r, g, b, a));
	}
	
	void GraphicContext::drawCircle(int x, int y, int radius, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
	{
		drawCircle(x, y, radius, Color(r, g, b, a));
	}
	
	void GraphicContext::setMinRes(int w, int h)
	{
		minW = w;
		minH = h;
	}
	
	void GraphicContext::beginVideoModeListing(void)
	{
		modes = SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_HWSURFACE);
	}
	
	bool GraphicContext::getNextVideoMode(int *w, int *h)
	{
		if (modes && (modes != (SDL_Rect **)-1))
		{
			while (*modes)
			{
				int nw = (*modes)->w;
				int nh = (*modes)->h;
				modes++;
				
				if ( ((minW == 0) || (nw >= minW))
					&& ((minH == 0) || (nh >= minH)))
				{
					*w = nw;
					*h = nh;
					return true;
				}
			}
		}
		return false;
	}
	
	GraphicContext::GraphicContext(int w, int h, Uint32 flags)
	{
		// some assert on the universe's structure
		assert(sizeof(Color) == 4);
		
		minW = minH = 0;
		sdlsurface = NULL;
		optionFlags = DEFAULT;
	
		// Load the SDL library
		if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER)<0 )
		{
			fprintf(stderr, "Toolkit : Initialisation Error : %s\n", SDL_GetError());
			exit(1);
		}
		else
		{
			fprintf(stderr, "Toolkit : Initialized : Graphic Context created\n");
		}
	
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
		SDL_EnableUNICODE(1);
		
		TTF_Init();
		
		setRes(w, h, flags);
	}
	
	GraphicContext::~GraphicContext(void)
	{
		TTF_Quit();
		SDL_Quit();
		sdlsurface = NULL;
		
		fprintf(stderr, "Toolkit : Graphic Context destroyed\n");
	}
	
	bool GraphicContext::setRes(int w, int h, Uint32 flags)
	{
		// check dimension
		if (minW && (w < minW))
		{
			fprintf(stderr, "Toolkit : Screen width %d is too small, set to min %d\n", w, minW);
			w = minW;
		}
		if (minH && (h < minH))
		{
			fprintf(stderr, "Toolkit : Screen height %d is too small, set to min %d\n", h, minH);
			h = minH;
		}
		
		// set flags
		optionFlags = flags;
		Uint32 sdlFlags = 0;
		if (flags & FULLSCREEN)
			sdlFlags |= SDL_FULLSCREEN;
		if (flags & FULLSCREEN)
			sdlFlags |= SDL_RESIZABLE;
		#ifdef HAVE_OPENGL
		if (flags & USEGPU)
		{
			SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
			sdlFlags |= SDL_OPENGL;
		}
		#else
		// remove GL from options
		optionFlags &= ~USEGPU;
		#endif
		
		// create surface
		sdlsurface = SDL_SetVideoMode(w, h, 32, sdlFlags);
		_gc = this;
		
		// check surface
		if (!sdlsurface)
		{
			fprintf(stderr, "Toolkit : can't set screen to %dx%d at 32 bpp\n", w, h);
			fprintf(stderr, "Toolkit : %s\n", SDL_GetError());
			return false;
		}
		else
		{
			setClipRect();
			if (flags & CUSTOMCURSOR)
			{
				// disable system cursor
				SDL_ShowCursor(SDL_DISABLE);
				// load custom cursors
				cursorManager.load();
			}
			else
				SDL_ShowCursor(SDL_ENABLE);
			
			if (flags & FULLSCREEN)
				fprintf(stderr, "Toolkit : Screen set to %dx%d at 32 bpp in fullscreen\n", w, h);
			else
				fprintf(stderr, "Toolkit : Screen set to %dx%d at 32 bpp in window\n", w, h);
			
			#ifdef HAVE_OPENGL
			if (optionFlags & USEGPU)
			{
				glState.checkExtensions();
				gluOrtho2D(0, w, h, 0);
				glEnable(GL_LINE_SMOOTH);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glState.doTexture(1);
				glState.blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			#endif
			
			return true;
		}
	}
	
	void GraphicContext::nextFrame(void)
	{
		if (sdlsurface)
		{
			if (optionFlags & CUSTOMCURSOR)
			{
				int mx, my;
				unsigned b = SDL_GetMouseState(&mx, &my);
				cursorManager.nextTypeFromMouse(this, mx, my, b != 0);
				setClipRect();
				cursorManager.draw(this, mx, my);
			}
			
			#ifdef HAVE_OPENGL
			if (optionFlags & GraphicContext::USEGPU)
				SDL_GL_SwapBuffers();
			else
			#endif
				SDL_Flip(sdlsurface);
		}
	}
	
	void GraphicContext::printScreen(const char *filename)
	{
		if (sdlsurface)
			SDL_SaveBMP(sdlsurface, filename);
	}
	
	// Font stuff
	
	int Font::getStringWidth(const int i)
	{
		std::ostringstream temp;
		temp << i;
		return getStringWidth(temp.str().c_str());
	}
	
	int Font::getStringWidth(const char *string, int len)
	{
		std::string temp;
		temp.append(string, len);
		return getStringWidth(temp.c_str());
	}
	
	int Font::getStringHeight(const char *string, int len)
	{
		std::string temp;
		temp.append(string, len);
		return getStringHeight(temp.c_str());
	}
	
	int Font::getStringHeight(const int i)
	{
		std::ostringstream temp;
		temp << i;
		return getStringHeight(temp.str().c_str());
	}
}
