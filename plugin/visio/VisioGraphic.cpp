/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: Details at https://graphviz.org
 *************************************************************************/

#include "config.h"

#include <algorithm>

#ifdef _MSC_VER
#include <cfloat>
#define isfinite _finite
#endif

#ifdef __GNUC__
#include <cmath>
#endif

#ifdef __SUNPRO_CC
#include <ieeefp.h>
#define isfinite(x) finite(x)
#endif

#include <cstdlib>
#include "VisioGraphic.h"

#include <cgraph/prisize_t.h>
#include <gvc/gvcjob.h>
#include <gvc/gvio.h>

namespace Visio
{
	using namespace std;
	
	static const float INCHES_PER_POINT = 1.0f / 72.0f;
	
	Fill::Fill(unsigned char red, unsigned char green, unsigned char blue, double transparency):
		_red(red),
		_green(green),
		_blue(blue),
		_transparency(transparency)
	{
	}
	
	void Fill::Print(GVJ_t* job) const
	{
		gvputs(job, "<Fill>\n");
		gvprintf(job, "<FillForegnd>#%02X%02X%02X</FillForegnd>\n", _red, _green, _blue);	/* VDX uses hex colors */
		gvprintf(job, "<FillForegndTrans>%f</FillForegndTrans>\n", _transparency); 
		gvputs(job, "</Fill>\n");
	}
	
	Line::Line(double weight, unsigned char red, unsigned char green, unsigned char blue, unsigned int pattern, unsigned int beginArrow, unsigned int endArrow):
		_weight(weight),
		_red(red),
		_green(green),
		_blue(blue),
		_pattern(pattern),
		_beginArrow(beginArrow),
		_endArrow(endArrow)
	{
	}
	
	void Line::Print(GVJ_t* job) const
	{
		gvputs(job, "<Line>\n");
		gvprintf(job, "<LineWeight>%f</LineWeight>\n", _weight * job->scale.x * INCHES_PER_POINT);	/* scale line weight, VDX uses inches */
		gvprintf(job, "<LineColor>#%02X%02X%02X</LineColor>\n", _red, _green, _blue);	/* VDX uses hex colors */
		if (_pattern)
			gvprintf(job, "<LinePattern>%u</LinePattern>\n", _pattern);
		if (_beginArrow)
			gvprintf(job, "<BeginArrow>%u</BeginArrow>\n", _beginArrow);
		if (_endArrow)
			gvprintf(job, "<EndArrow>%u</EndArrow>\n", _endArrow);	
		gvputs(job, "</Line>\n");
	}
	
	Geom::~Geom()
	{
	}

	Ellipse::Ellipse(pointf* points, bool filled):
		_filled(filled)
	{
		_points[0] = points[0];
		_points[1] = points[1];
	}
	
	void Ellipse::Print(GVJ_t* job, pointf, pointf, bool) const
	{
		gvputs(job, "<Geom>\n");
		if (!_filled)
			gvputs(job, "<NoFill>1</NoFill>\n");	/* omit fill? */	
		gvputs(job, "<MoveTo><X F='Width*0' /><Y F='Height*0.5' /></MoveTo>\n");
		gvputs(job, "<EllipticalArcTo><X F='Width*1' /><Y F='Height*0.5' /><A F='Width*0.5' /><B F='Height*1' /><C>0</C><D F='Width/Height*1' /></EllipticalArcTo>\n");	/* semi ellipse */
		gvputs(job, "<EllipticalArcTo><X F='Geometry1.X1' /><Y F='Geometry1.Y1' /><A F='Width*0.5' /><B F='Height*0' /><C>0</C><D F='Width/Height*1' /></EllipticalArcTo>\n");	/* semi ellipse */
		gvputs(job, "</Geom>\n");
	}
	
	boxf Ellipse::GetBounds() const
	{
		/* point[0] is center, point[1] is one corner */
		boxf bounds;
		bounds.LL.x = _points[0].x + _points[0].x - _points[1].x;
		bounds.LL.y = _points[0].y + _points[0].y - _points[1].y;
		bounds.UR.x = _points[1].x;
		bounds.UR.y = _points[1].y;
		return bounds;
	}
		
	const Connection* Ellipse::GetConnection() const
	{
		return nullptr;
	}
	
	Path::Path(pointf* points, int pointCount)
	{
		/* copy over the points */
		_points.reserve(pointCount);
		for (int i = 0; i< pointCount; ++i) {
			_points.push_back(points[i]);
		}
	}
	
	boxf Path::GetBounds() const
	{
		boxf bounds;
		if (!_points.empty())
		{
			/* lower left is the minimal point, upper right is maximal point */
			bounds.LL.x = bounds.UR.x = _points[0].x;
			bounds.LL.y = bounds.UR.y = _points[0].y;
			for (const pointf &p : _points)
			{
				if (bounds.LL.x > p.x)
					bounds.LL.x = p.x;
				if (bounds.LL.y > p.y)
					bounds.LL.y = p.y;
				if (bounds.UR.x < p.x)
					bounds.UR.x = p.x;
				if (bounds.UR.y < p.y)
					bounds.UR.y = p.y;
			}
		}
		else
		{
			/* no points, return null bounds */
			bounds.LL.x = bounds.UR.x = 0.0;
			bounds.LL.y = bounds.UR.y = 0.0;			
		}
		return bounds;
	}
	
	Bezier::Bezier(pointf* points, int pointCount, bool filled):
		Path(points, pointCount),
		_filled(filled)
	{
	}
	
	const Connection* Bezier::GetConnection() const
	{
		return this;
	}
	
	pointf Bezier::GetFirst() const
	{
		return _points[0];
	}
	
	pointf Bezier::GetLast() const
	{
		return _points[1];
	}
	
	pointf Bezier::GetCenter() const
	{
		if (_points.size() >= 4 && _points.size() % 2 == 0)
		{
			pointf center;
			
			/* the central control polygon for the bezier curve */
			pointf p0 = _points[_points.size() / 2 - 2];
			pointf p1 = _points[_points.size() / 2 - 1];
			pointf p2 = _points[_points.size() / 2];
			pointf p3 = _points[_points.size() / 2 + 1];
			
			/* use de Casteljou's algorithm to get a midpoint */
			center.x = 0.125 * p0.x + 0.375 * p1.x + 0.375 * p2.x + 0.125 * p3.x;
			center.y = 0.125 * p0.y + 0.375 * p1.y + 0.375 * p2.y + 0.125 * p3.y;
			return center;
		}
		else
		/* just return the middle point */
			return _points[_points.size() / 2];
	}
	
	void Bezier::Print(GVJ_t* job, pointf first, pointf last, bool allowCurves) const
	{
		gvputs(job, "<Geom>\n");
		if (!_filled)
			gvputs(job, "<NoFill>1</NoFill>\n");		
		if (!_points.empty())
		{
			double xscale = 1.0 / (last.x - first.x);
			double yscale = 1.0 / (last.y - first.y);
			if (isfinite(xscale) == 0)
				xscale = 0.0;
			if (isfinite(yscale) == 0)
				yscale = 0.0;
				
			gvputs(job, "<MoveTo>");
			gvprintf(job, "<X F='Width*%f' />", (_points[0].x - first.x) * xscale);
			gvprintf(job, "<Y F='Height*%f' />", (_points[0].y - first.y) * yscale);
			gvputs(job, "</MoveTo>\n");
			
			if (allowCurves)
			{
				/* convert Graphviz cubic bezier into VDX NURBS curve: */
				/* NURBS control points == bezier control points */
				/* NURBS order == bezier order == 3 */
				/* NURBS knot vector == { 0, 0, 0, 0,  1, 2 ... } */
				
				gvputs(job, "<NURBSTo>");
				
				/* Ctl[P-1].X */
				gvprintf(job, "<X F='Width*%f'/>", (_points.back().x - first.x) * xscale);
				
				/* Ctl[P-1].Y */
				gvprintf(job, "<Y F='Height*%f'/>", (_points.back().y - first.y) * yscale);
				
				/* Knot[P-1] */
				gvprintf(job, "<A>%" PRISIZE_T "</A>",
				         _points.size() < 4 ? 0 : (_points.size() - 4));
				
				/* Ctl[P-1].Weight */
				gvputs(job, "<B>1</B>");	
				
				/* Knot[0] */
				gvputs(job, "<C>0</C>");
				
				/* Weight[0] */
				gvputs(job, "<D>1</D>");	
				
				/* Knot[P], Degree, XType, YType */
				gvprintf(job, "<E F='NURBS(%" PRISIZE_T ", 3, 0, 0",
				         _points.size() < 3 ? 0 : (_points.size() - 3));
				for (size_t i = 1; i < _points.size(); ++i)
				/* Ctl[i].X, Ctl[i].Y, Knot[i], Ctl[i].Weight */
					gvprintf(job, ", %f, %f, %" PRISIZE_T ", 1",					
							 (_points[i].x - first.x) * xscale,
							 (_points[i].y - first.y) * yscale,
							 i < 3 ? 0 : (i - 3));	
				gvputs(job, ")'/>");
				
				gvputs(job, "</NURBSTo>\n");
			}
			else
			{
				/* output lines only, so skip all the Bezier control points i.e. use every 3rd point */
				
				if (_points.size() == 4)
				{
					/* single point, use VDX LineTo */
					gvputs(job, "<LineTo>");
					gvprintf(job, "<X F='Width*%f' />", (_points[3].x - first.x) * xscale);
					gvprintf(job, "<Y F='Height*%f' />", (_points[3].y - first.y) * yscale);
					gvputs(job, "</LineTo>\n");
				}
				else
				{
					/* multiple points, use VDX PolylineTo */
					gvputs(job, "<PolylineTo>");
					gvprintf(job, "<X F='Width*%f' />", (_points.back().x - first.x) * xscale);
					gvprintf(job, "<Y F='Height*%f' />", (_points.back().y - first.y) * yscale);
					gvputs(job, "<A F='POLYLINE(0, 0");
					for (size_t i = 3; i + 1 < _points.size(); i += 3)
						gvprintf(job, ", %f, %f", (_points[i].x - first.x) * xscale, (_points[i].y - first.y) * yscale);
					gvputs(job, ")' />");
					gvputs(job, "</PolylineTo>\n");
				}
			}
		}
		gvputs(job, "</Geom>\n");
	}
	
	Polygon::Polygon(pointf* points, int pointCount, bool filled):
		Path(points, pointCount),
		_filled(filled)
	{
	}
	
	const Connection* Polygon::GetConnection() const
	{
		return nullptr;
	}
	
	void Polygon::Print(GVJ_t* job, pointf first, pointf last, bool) const
	{
		gvputs(job, "<Geom>\n");
		if (!_filled)
			gvputs(job, "<NoFill>1</NoFill>\n");		
		if (!_points.empty())
		{
			/* compute scale. if infinite, scale by 0 instead */
			double xscale = 1.0 / (last.x - first.x);
			double yscale = 1.0 / (last.y - first.y);
			if (isfinite(xscale) == 0)
				xscale = 0.0;
			if (isfinite(yscale) == 0)
				yscale = 0.0;
					
			gvputs(job, "<MoveTo>");
			gvprintf(job, "<X F='Width*%f' />", (_points[0].x - first.x) * xscale);
			gvprintf(job, "<Y F='Height*%f' />", (_points[0].y - first.y) * yscale);
			gvputs(job, "</MoveTo>\n");
			
			if (_points.size() == 1)
			{
				/* single point, use VDX LineTo */
				gvputs(job, "<LineTo>");
				gvprintf(job, "<X F='Width*%f' />", (_points[0].x - first.x) * xscale);
				gvprintf(job, "<Y F='Height*%f' />", (_points[0].y - first.y) * yscale);
				gvputs(job, "</LineTo>\n");
			}
			else
			{
				/* multiple points, use VDX PolylineTo */
				gvputs(job, "<PolylineTo>");
				gvprintf(job, "<X F='Width*%f' />", (_points[0].x - first.x) * xscale);
				gvprintf(job, "<Y F='Height*%f' />", (_points[0].y - first.y) * yscale);
				gvputs(job, "<A F='POLYLINE(0, 0");
				for (size_t i = 1; i < _points.size(); ++i)
					gvprintf(job, ", %f, %f", (_points[i].x - first.x) * xscale, (_points[i].y - first.y) * yscale);
				gvputs(job, ")' />");
				gvputs(job, "</PolylineTo>\n");
			}
		}
		gvputs(job, "</Geom>\n");
	}

	Polyline::Polyline(pointf* points, int pointCount):
		Path(points, pointCount)
	{
	}
	
	const Connection* Polyline::GetConnection() const
	{
		return nullptr;
	}
	
	void Polyline::Print(GVJ_t* job, pointf first, pointf last, bool) const
	{
		gvputs(job, "<Geom>\n");
		if (!_points.empty())
		{
			/* compute scale. if infinite, scale by 0 instead */
			double xscale = 1.0 / (last.x - first.x);
			double yscale = 1.0 / (last.y - first.y);
			if (isfinite(xscale) == 0)
				xscale = 0.0;
			if (isfinite(yscale) == 0)
				yscale = 0.0;
					
			gvputs(job, "<MoveTo>");
			gvprintf(job, "<X F='Width*%f' />", (_points[0].x - first.x) * xscale);
			gvprintf(job, "<Y F='Height*%f' />", (_points[0].y - first.y) * yscale);
			gvputs(job, "</MoveTo>\n");
			
			if (_points.size() == 2)
			{
				/* single point, use VDX LineTo */
				gvputs(job, "<LineTo>");
				gvprintf(job, "<X F='Width*%f' />", (_points[1].x - first.x) * xscale);
				gvprintf(job, "<Y F='Height*%f' />", (_points[1].y - first.y) * yscale);
				gvputs(job, "</LineTo>\n");
			}
			else
			{
				/* multiple points, use VDX PolylineTo */
				gvputs(job, "<PolylineTo>");
				gvprintf(job, "<X F='Width*%f' />", (_points.back().x - first.x) * xscale);
				gvprintf(job, "<Y F='Height*%f' />", (_points.back().y - first.y) * yscale);
				gvputs(job, "<A F='POLYLINE(0, 0");
				for (size_t i = 1; i + 1 < _points.size(); ++i)
					gvprintf(job, ", %f, %f", (_points[i].x - first.x) * xscale, (_points[i].y - first.y) * yscale);
				gvputs(job, ")' />");
				gvputs(job, "</PolylineTo>\n");
			}
		}
		gvputs(job, "</Geom>\n");
	}

	Graphic* Graphic::CreateEllipse(GVJ_t* job, pointf* A, bool filled)
	{
		unsigned int pattern;
		switch (job->obj->pen)
		{
			case PEN_DASHED:
				pattern = 2;
				break;
			case PEN_DOTTED:
				pattern = 3;
				break;
			default:
				pattern = 1;
				break;
		}
		return new Graphic(
			Line(
				job->obj->penwidth,
				job->obj->pencolor.u.rgba[0],
				job->obj->pencolor.u.rgba[1],
				job->obj->pencolor.u.rgba[2],
				pattern),
			filled ? new Fill(
				job->obj->fillcolor.u.rgba[0],
				job->obj->fillcolor.u.rgba[1],
				job->obj->fillcolor.u.rgba[2],
				(255 - job->obj->fillcolor.u.rgba[3]) / 255.0) : nullptr,	/* Graphviz alpha (00 - FF) to VDX transparency (1.0 - 0.0) */
			new Ellipse(A, filled));
	}
	
	Graphic* Graphic::CreateBezier(GVJ_t* job, pointf* A, int n, bool arrow_at_start, bool arrow_at_end, bool filled)
	{
		unsigned int pattern;
		switch (job->obj->pen)
		{
			case PEN_DASHED:
				pattern = 2;
				break;
			case PEN_DOTTED:
				pattern = 3;
				break;
			default:
				pattern = 1;
				break;
		}
		return new Graphic(
			Line(
				job->obj->penwidth,
				job->obj->pencolor.u.rgba[0],
				job->obj->pencolor.u.rgba[1],
				job->obj->pencolor.u.rgba[2],
				pattern,
				arrow_at_start ? 2 : 0,		/* VDX arrow type 2 == filled solid */
				arrow_at_end ? 2 : 0),		/* VDX arrow type 2 == filled solid */
			filled ? new Fill(
				job->obj->fillcolor.u.rgba[0],
				job->obj->fillcolor.u.rgba[1],
				job->obj->fillcolor.u.rgba[2],
				(255 - job->obj->fillcolor.u.rgba[3]) / 255.0) : nullptr,	/* Graphviz alpha (00 - FF) to VDX transparency (1.0 - 0.0) */
			new Bezier(
				A,
				n,
				filled));
	}
	
	Graphic* Graphic::CreatePolygon(GVJ_t* job, pointf* A, int n, bool filled)
	{
		unsigned int pattern;
		switch (job->obj->pen)
		{
			case PEN_DASHED:
				pattern = 2;
				break;
			case PEN_DOTTED:
				pattern = 3;
				break;
			default:
				pattern = 1;
				break;
		}
		return new Graphic(
			Line(
				job->obj->penwidth,
				job->obj->pencolor.u.rgba[0],
				job->obj->pencolor.u.rgba[1],
				job->obj->pencolor.u.rgba[2],
				pattern),
			filled ? new Fill(job->obj->fillcolor.u.rgba[0],
				job->obj->fillcolor.u.rgba[1],
				job->obj->fillcolor.u.rgba[2],
				(255 - job->obj->fillcolor.u.rgba[3]) / 255.0) : nullptr,	/* Graphviz alpha (00 - FF) to VDX transparency (1.0 - 0.0) */
			new Polygon(
				A,
				n,
				filled));
	}
	
	Graphic* Graphic::CreatePolyline(GVJ_t* job, pointf* A, int n)
	{
		unsigned int pattern;
		switch (job->obj->pen)
		{
			case PEN_DASHED:
				pattern = 2;
				break;
			case PEN_DOTTED:
				pattern = 3;
				break;
			default:
				pattern = 1;
				break;
		}
		return new Graphic(
			Line(
				job->obj->penwidth,
				job->obj->pencolor.u.rgba[0],
				job->obj->pencolor.u.rgba[1],
				job->obj->pencolor.u.rgba[2],
				pattern),
			nullptr,		/* polylines have no fill */
			new Polyline(
				A,
				n));
	}
	
	Graphic::Graphic(const Line &line, Fill* fill, Geom* geom):
		_line(line),
		_fill(fill),
		_geom(geom)
	{
	}
	
	boxf Graphic::GetBounds() const
	{
		return _geom->GetBounds();
	}
	
	const Connection* Graphic::GetConnection() const
	{
		return _geom->GetConnection();
	}
		
	void Graphic::Print(GVJ_t* job, pointf first, pointf last, bool allowCurves) const
	{
		_line.Print(job);
		if (_fill)
			_fill->Print(job);
		if (_geom)
			_geom->Print(job, first, last, allowCurves);
		
	}
	

}
