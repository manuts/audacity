/**********************************************************************

Audacity: A Digital Audio Editor

ZoomHandle.cpp

Paul Licameli split from TrackPanel.cpp

**********************************************************************/

#include "../../Audacity.h"
#include "ZoomHandle.h"

#include <algorithm>
#include "../../MemoryX.h"

#include <wx/dc.h>
#include <wx/event.h>
#include <wx/gdicmn.h>

#include "../../HitTestResult.h"
#include "../../Project.h"
#include "../../RefreshCode.h"
#include "../../TrackPanelMouseEvent.h"
#include "../../toolbars/ToolsToolBar.h"
#include "../../ViewInfo.h"
#include "../../../images/Cursors.h"

///  This class takes care of our different zoom
///  possibilities.  It is possible for a user to just
///  "zoom in" or "zoom out," but it is also possible
///  for a user to drag and select an area that he
///  or she wants to be zoomed in on.  We use mZoomStart
///  and mZoomEnd to track the beginning and end of such
///  a zoom area.  Note that the ViewInfo
///  actually keeps track of our zoom constant,
///  so we achieve zooming by altering the zoom constant
///  and forcing a refresh.

ZoomHandle::ZoomHandle()
{
}

ZoomHandle &ZoomHandle::Instance()
{
   static ZoomHandle instance;
   return instance;
}

HitTestPreview ZoomHandle::HitPreview
   (const wxMouseEvent &event, const AudacityProject *pProject)
{
   static auto zoomInCursor =
      ::MakeCursor(wxCURSOR_MAGNIFIER, ZoomInCursorXpm, 19, 15);
   static auto zoomOutCursor =
      ::MakeCursor(wxCURSOR_MAGNIFIER, ZoomOutCursorXpm, 19, 15);
   const ToolsToolBar *const ttb = pProject->GetToolsToolBar();
   return {
      ttb->GetMessageForTool(zoomTool),
      (event.ShiftDown() ? &*zoomOutCursor : &*zoomInCursor)
   };
}

HitTestResult ZoomHandle::HitAnywhere
(const wxMouseEvent &event, const AudacityProject *pProject)
{
   return { HitPreview(event, pProject), &Instance() };
}

HitTestResult ZoomHandle::HitTest
(const wxMouseEvent &event, const AudacityProject *pProject)
{
   if (event.ButtonIsDown(wxMOUSE_BTN_RIGHT) || event.RightUp())
      return HitAnywhere(event, pProject);
   else
      return {};
}

ZoomHandle::~ZoomHandle()
{
}

UIHandle::Result ZoomHandle::Click
(const TrackPanelMouseEvent &evt, AudacityProject *)
{
   const wxMouseEvent &event = evt.event;
   if (event.ButtonDown() || event.LeftDClick()) {
      /// Zoom button down, record the position.
      mZoomStart = event.m_x;
      mZoomEnd = event.m_x;
      mRect = evt.rect;
   }
   return RefreshCode::RefreshNone;
}

UIHandle::Result ZoomHandle::Drag
(const TrackPanelMouseEvent &evt, AudacityProject *)
{
   const wxMouseEvent &event = evt.event;
   const int left = mRect.GetLeft();
   const int right = mRect.GetRight();

   mZoomEnd = event.m_x;

   if (event.m_x < left) {
      mZoomEnd = left;
   }
   else if (event.m_x > right) {
      mZoomEnd = right;
   }

   // Refresh tracks ALWAYS.  Even if IsDragZooming() becomes false, make the
   // dashed lines disappear. -- PRL
   return RefreshCode::RefreshAll; // (IsDragZooming() ? RefreshAllTracks : RefreshNone),
}

HitTestPreview ZoomHandle::Preview
(const TrackPanelMouseEvent &evt, const AudacityProject *pProject)
{
   return HitPreview(evt.event, pProject);
}

UIHandle::Result ZoomHandle::Release
(const TrackPanelMouseEvent &evt, AudacityProject *pProject,
 wxWindow *)
{
   const wxMouseEvent &event = evt.event;
   ViewInfo &viewInfo = pProject->GetViewInfo();
   if (mZoomEnd < mZoomStart)
      std::swap(mZoomStart, mZoomEnd);

   const int trackLeftEdge = mRect.x;
   if (IsDragZooming())
   {
      ///  This actually sets the Zoom value when you're done doing
      ///  a drag zoom.
      double left = viewInfo.PositionToTime(mZoomStart, trackLeftEdge);
      double right = viewInfo.PositionToTime(mZoomEnd, trackLeftEdge);

      double multiplier =
         (viewInfo.PositionToTime(mRect.width) - viewInfo.PositionToTime(0)) /
         (right - left);
      if (event.ShiftDown())
         multiplier = 1.0 / multiplier;

      viewInfo.ZoomBy(multiplier);

      viewInfo.h = left;
   }
   else
   {
      /// This handles normal Zoom In/Out, if you just clicked;
      /// IOW, if you were NOT dragging to zoom an area.
      /// \todo MAGIC NUMBER: We've got several in this method.
      const double center_h =
         viewInfo.PositionToTime(event.m_x, trackLeftEdge);

      const double multiplier =
         (event.RightUp() || event.RightDClick() || event.ShiftDown())
         ? 0.5 : 2.0;
      viewInfo.ZoomBy(multiplier);

      if (event.MiddleUp() || event.MiddleDClick())
         viewInfo.SetZoom(ZoomInfo::GetDefaultZoom()); // AS: Reset zoom.

      const double new_center_h =
         viewInfo.PositionToTime(event.m_x, trackLeftEdge);

      viewInfo.h += (center_h - new_center_h);
   }

   mZoomEnd = mZoomStart = 0;

   using namespace RefreshCode;
   return RefreshAll | FixScrollbars;
}

UIHandle::Result ZoomHandle::Cancel(AudacityProject*)
{
   // Cancel is implemented!  And there is no initial state to restore,
   // so just return a code.
   return RefreshCode::RefreshAll;
}

void ZoomHandle::DrawExtras
(DrawingPass pass, wxDC * dc, const wxRegion &, const wxRect &panelRect)
{
   if (pass == Cells) {
      // PRL: Draw dashed lines only if we would zoom in
      // for button up.
      if (!IsDragZooming())
         return;

      wxRect rect;

      dc->SetBrush(*wxTRANSPARENT_BRUSH);
      dc->SetPen(*wxBLACK_DASHED_PEN);

      rect.x = std::min(mZoomStart, mZoomEnd);
      rect.width = 1 + abs(mZoomEnd - mZoomStart);
      rect.y = -1;
      rect.height = panelRect.height + 2;

      dc->DrawRectangle(rect);
   }
}

bool ZoomHandle::IsDragZooming() const
{
   const int DragThreshold = 3;// Anything over 3 pixels is a drag, else a click.
   return (abs(mZoomEnd - mZoomStart) > DragThreshold);
}
