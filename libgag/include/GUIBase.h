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

#ifndef __GUIBASE_H
#define __GUIBASE_H

#include "GAGSys.h"
#include <vector>
#include <set>

namespace GAGCore
{
	class GraphicContext;
	class DrawableSurface;
}

namespace GAGGUI
{
	// transform an ucs 16 unicode char to an utf8 one
	void UCS16toUTF8(Uint16 ucs16, char utf8[4]);
	
	// return the number of char to go to the next utf8 one in the string
	unsigned getNextUTF8Char(unsigned char c);
	
	// return pos of the next UTF8 char in text
	unsigned getNextUTF8Char(const char *text, unsigned pos);
	
	// return pos of the previous UTF8 char in text
	unsigned getPrevUTF8Char(const char *text, unsigned pos);
	
	//! Widget reacts to SDL_Event and produce Action
	enum Action
	{
		SCREEN_CREATED,	// after first draw
		SCREEN_DESTROYED,	// after endExectue
		SCREEN_RESIZED,
	
		BUTTON_GOT_MOUSEOVER,
		BUTTON_LOST_MOUSEOVER,
		BUTTON_PRESSED,
		BUTTON_RELEASED,
		BUTTON_SHORTCUT,
		BUTTON_STATE_CHANGED,
	
		TEXT_CURSOR_MOVED,
		TEXT_MODIFIED,
		TEXT_ACTIVATED,
		TEXT_VALIDATED,
		TEXT_CANCELED,
		TEXT_SET,
	
		LIST_ELEMENT_SELECTED,
	
		NUMBER_ELEMENT_SELECTED,
		RATIO_CHANGED,
		VALUE_CHANGED,
	};
	
	class Screen;
	
	//! A widget is a GUI block element
	class Widget
	{
	public:
			//! if the widget is visible it receive paint event, timer event and SDL event. Otherwise it receive no events.
		bool visible;
	
	public:
		//! Widget constructor
		Widget();
		//! Widget destructor
		virtual ~Widget();
	
		//! Method called for each timer's tick
		virtual void onTimer(Uint32 tick) { }
		//! Method called for each SDL_Event
		virtual void onSDLEvent(SDL_Event *event) { }
		//! Drawing methode of the widget
		virtual void paint(void) = 0;
		//! Init the widget, called before the first paint
		virtual void init(void) { }
	
	protected:
		friend class Screen;
		//! Return true if (px,py) is in the rect (x,y)-(x+w,y+h)
		bool isPtInRect(int px, int py, int x, int y, int w, int h) { if ((px>x) && (py>y) && (px<x+w) && (py<y+h)) return true; else return false; }
		//! Screen that contains the widget
		Screen *parent;
	};
	
	#define ALIGN_LEFT 0
	#define ALIGN_RIGHT 1
	#define ALIGN_FILL 2
	#define ALIGN_SCREEN_CENTERED 3
	#define ALIGN_TOP 0
	#define ALIGN_BOTTOM 1
	
	//! The parent for all standards widgets like Button, texts, etc...
	class RectangularWidget:public Widget
	{
	protected:
		Sint32 x;
		Sint32 y;
		Sint32 w;
		Sint32 h;
		Uint32 hAlignFlag;
		Uint32 vAlignFlag;
	
	public:
		//! RectangularWidget constructor, set all values to 0
		RectangularWidget();
		//! RectangularWidget destructor
		virtual ~RectangularWidget() { }
	
		//! Show the widget, put in queue for end of step
		virtual void show(void);
	
		//! Hide the widget, put in queue for end of step
		virtual void hide(void);
	
		//! Show or hide the widget, call show or hide depending on visibe
		virtual void setVisible(bool visible);
	
	protected:
		//! Compute the actual position from the layout informations
		virtual void getScreenPos(int *sx, int *sy, int *sw, int *sh);
	};
	
	//! This class provides highlight support through mouse motion detection
	class HighlightableWidget:public RectangularWidget
	{
	public:
		bool highlighted;
		float prevHighlightValue;
		float nextHighlightValue;
		float actAnimationTime;
		const float totalAnimationTime;
		Sint32 returnCode;
	
	public:
		HighlightableWidget();
		HighlightableWidget(Sint32 returnCode);
	
		virtual ~HighlightableWidget() {}
	
		virtual void paint(void);
		virtual void onSDLEvent(SDL_Event *event);
	};
	
	//! The screen is the widget container and has a background
	class Screen
	{
	protected:
		//! the widgets
		std::set<Widget *> widgets;
	
		//! true while execution is running, no need for serialisation
		bool run;
		//! the return code, no need for serialisation
		Sint32 returnCode;
		
		//! the graphic context associated with this screen
		GAGCore::DrawableSurface *gfx;
		
	public:
		//! The animation frame for screen creation
		int animationFrame;
		
	public:
		Screen();
	
		virtual ~Screen();
	
		//! Method called for each timer's tick
		virtual void onTimer(Uint32 tick) { }
		//! Method called for each SDL_Event
		virtual void onSDLEvent(SDL_Event *event) { }
		//! Method called when a widget produces an Action
		virtual void onAction(Widget *source, Action action, int par1, int par2)=0;
		//! Full screen paint, call paint(0, 0, gfx->getW(), gfx->getH())
		virtual void paint(void);
		
		//! Run the screen until someone call endExecute(returnCode). Return returnCode
		virtual int execute(GAGCore::DrawableSurface *gfx, int stepLength);
		//! Call this methode to stop the execution of the screen
		void endExecute(int returnCode);
		//! Add widget, added widget are garbage collected
		void addWidget(Widget* widget);
		//! Remove widget, note that removed widget are not garbage collected
		void removeWidget(Widget* widget);
		//! Call onSDLEvent on each widget after having called onSDLEvent on the screen itself
		void dispatchEvents(SDL_Event *event);
		//! Call onTimer on each widget after having called onTimer on the screen itself
		void dispatchTimer(Uint32 tick);
		//! Call init on each widget before the first call
		void dispatchInit(void);
		//! Call paint on each widget after having called paint on the screen itself. Do a full update after
		void dispatchPaint(void);
		//! Return the associated drawable surface
		GAGCore::DrawableSurface *getSurface(void) { return gfx; }
		//! Return the width of the screen
		int getW(void);
		//! Return the height of the screen
		int getH(void);
	};
	
	
	//! Base class used for screen that don't take full frame and/or are non-blocking
	class OverlayScreen:public Screen
	{
	public:
		//! Int to say when we have finished
		int endValue;
		//! Displacement from top-left corner of screen
		unsigned decX, decY;
	
	public:
		//! Constructor, take the context in which the overlay must be create and its dimensions in w and h
		OverlayScreen(GAGCore::GraphicContext *parentCtx, unsigned w, unsigned h);
		//! Destructor
		virtual ~OverlayScreen();
	
		//! Run the OverlayScreen, call Screen::execute with the correct DrawableSurface
		virtual int execute(GAGCore::DrawableSurface *gfx, int stepLength);
		//! Call thisinstead of dispatch event
		virtual void translateAndProcessEvent(SDL_Event *event);
		//! Paint a part of the background of screen
		virtual void paint(void);
	};
}

#endif
