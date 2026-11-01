//
//   (c) 1998-1999 Monolith Productions, Inc.  All Rights Reserved
//
// ---------------------------------------------------------------
//
//------------------------------------------------------------------
//
//	FILE	  : BasePoly.h
//
//	PURPOSE	  : Defines the CBasePoly class.
//
//	CREATED	  : October 15 1996
//
//
//------------------------------------------------------------------

#ifndef __BASEPOLY_H__
	#define __BASEPOLY_H__


	// Includes....
	#include "editvert.h"
	#include "editray.h"
	#include "editplane.h"
	#include "editbrush.h"


	// Defines....
	class CEditRegion;
	class CEditBrush;
	class CLTANode;
	class CLTAFile;



	class CBasePoly
	{
		public:

								CBasePoly();
								CBasePoly( CEditBrush *pBrush );
								CBasePoly( CEditBrush *pBrush, CMoWordArray &indices );
								CBasePoly( CEditBrush *pBrush, WORD i0, WORD i1, WORD i2, BOOL bUpdatePlane=FALSE );
			// talon: no virtual destructor in 2001: the vtable (0x474470) is
			// CopyAttributes alone (0x44d400), the compiler's destructor
			// (0x40f8a0) stores no vtable, and CEditPoly::UpdateTriangles
			// frees its temporary with a plain operator delete (0x40fec0).

			void				Term();


		// Saving.
		public:


			BOOL				LoadBasePolyTBW( CAbstractIO& InFile );
			void				SaveBasePolyTBW( CAbstractIO& OutFile );

#ifdef DIRECTEDITOR_BUILD
			// talon: Build 69's LTA loading and saving, for DEdit (the 2001
			// one has an LTA world: progress/DEdit_map.md).
			// talon: BOOL in 2001: 0x40f8f8 returns `xor eax; inc eax`, its
			// callers test %eax (0x424e93)
			BOOL				LoadBasePolyLTA( CLTANode* pNode );
			void				SaveBasePolyLTA( CLTAFile* pFile, uint32 level );
#endif

		// Functionality.
		public:

			BOOL				IsValid();

			// Called when a poly is split.
			virtual void		CopyAttributes( CBasePoly *pOther, CStringHolder *pStringHolder=NULL )	{}
			
			// Decrements all points >= index.
			void				DecrementPoints( uint32 index );

			// Tells if the point is in the polygon's plane or in the poly itself.
			BOOL				PointInPlane(const LTVector &point );
			BOOL				PointInPoly(const LTVector &point );

			void				RemoveCollinearVertices();
			BOOL				IsConvex();

			void				GetCenterPoint( CVector &centerPt );

			void				Flip();
			void				UpdatePlane();

			BOOL				GetTriangles( CMoArray<CBasePoly*> &polies );

#ifdef DIRECTEDITOR_BUILD
			// talon: DEdit's (2001, 0x40fcda): Build 69's CEditPoly::IntersectRay
			// on a CBasePoly; CEditPoly's runs it over its triangles (0x4253e3).
			BOOL				IntersectRay( CEditRay &ray, CReal &t, BOOL bBackface );
#endif


		// Useful little helper geometry functions.
		public:

			BOOL				Intersect(const LTVector &a, const LTVector &b, const LTVector &c, const LTVector &d );
			BOOL				Diagonalie( WORD a, WORD b );

			BOOL				LeftOn( WORD a, WORD b, WORD c );
			BOOL				Left( WORD a, WORD b, WORD c );
			BOOL				InsideDiagonal( WORD a, WORD b );
	
			BOOL				ValidTriangle( WORD a, WORD b, WORD c );

		
		// Accessors.
		public:

			CVector&			Normal()				{ return m_Plane.m_Normal; }
			CReal&				Dist()					{ return m_Plane.m_Dist; }

			WORD&				Index( uint32 i )		{ return m_Indices[i]; }
			WORD&				NextIndex( uint32 i )	{ return m_Indices.Next(i); }
			WORD&				LastIndex()				{ return m_Indices.Last(); }
			// talon: a WORD in 2001: UpdatePlane (0x401430) masks it to 16 bits
			WORD				NumVerts() const		{ return (WORD)m_Indices.GetSize(); }
			
			// talon: not inline in 2001: BasePoly.obj has them as functions
			// (0x401ad0, 0x401af0), which RaySelect calls
			CEditVert&			Pt( uint32 i );

			CEditVert&			NextPt( uint32 i );
			CEditVert&			PrevPt( uint32 i )		{ return m_pBrush->m_Points[m_Indices.Prev(i)]; }
			CEditVert&			LastPt()				{ return m_pBrush->m_Points[m_Indices.Last()]; }
			
		
		public:
			
			CEditPlane			m_Plane;

			// talon: WORDs in 2001: CBasePoly( pBrush, indices ) (0x401080)
			// copies them two bytes at a time
			CMoWordArray		m_Indices;
			CEditBrush			*m_pBrush;

	};


#endif  // __BASEPOLY_H__


