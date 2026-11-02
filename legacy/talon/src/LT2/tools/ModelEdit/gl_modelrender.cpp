//  ----------------------------------------------------------------
//  * setup gl rendering context
//  * render the model using open gl
//  * animation to mesh updates
//  ----------------------------------------------------------------
//
// talon: brought back to the 2001 image's (0x4035cd..0x406db3): one LOD
// index rather than LOD distances, no vertex animation, OBBs, vertex weight
// colors or movement encoding, the viewing setup reading a file-scope
// DrawStruct pointer, and the 2001 triangle subdivision that DrawPiece
// switches to with Ctrl+Shift+Alt held (0x405781).
#include "precompile.h"
#pragma warning (disable:4786)

#include "gl_modelrender.h"
#include "model_ops.h"
#include "pixelformat.h"
#include "geomroutines.h"
#include "transformmaker.h"
#include "model_cleanup.h"
#include "counter.h"
#include <gl/gl.h>
#include <gl/glu.h>
#include <winbase.h>


// ------------------------------------------------------------------------

#define	GLM_TRI_INDEX0	2
#define	GLM_TRI_INDEX1	1
#define	GLM_TRI_INDEX2	0

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")




// ------------------------------------------------------------------------
// StateSet
// pushes and pops GL attribs ..
// examp: 
// StateSet zbuf( GL_DEPTH_TEST , FALSE );
// constructor either enables or disables option, destructor does the opposite
// ------------------------------------------------------------------------
// talon: 2001's glChooseEnable is a function of its own (0x4044c1), called
// from the inline constructor and destructor.
static void glChooseEnable(int option, BOOL bEnable)
{
	if(bEnable)
		glEnable(option);
	else
		glDisable(option);
}

class StateSet
{
	int			m_Option;
	int			m_OldVal;
	
public:
	StateSet(int option, BOOL bEnable)
	{
		m_Option = option;
		m_OldVal = glIsEnabled(option);
		glChooseEnable(option, bEnable);
	}

	~StateSet()
	{
		glChooseEnable(m_Option, m_OldVal);
	}
};


// ------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------
static CMoArray<DVector> g_TransformedVerts;
static CMoArray<DVector*> g_PseudoIndices;
static float g_tuAdd, g_tvAdd;

// talon: the DrawStruct SetupViewingParameters reads (0x493e8c; the name is
// ours): DrawModel sets it.
static DrawStruct *g_pDrawStruct;

// talon: DrawPiece's subdivision (0x48ca14 and 0x48ca18 in .data; the names
// are ours): the number of levels, and how far an edge point is pulled into
// the tangent plane of its vertex.
DWORD	g_nSubdivideLevels = 1;
float	g_fSubdivideTension = 0.7f;


static void DrawModelPolies(GLM_CONTEXT hContext, AnimTracker **pTrackers, DWORD nTrackers, BOOL bDrawBright,
	DMatrix *pRootTransform, BYTE *pSelectedPieces, BOOL bNormals, DWORD iLOD, BOOL bDrawOriginalModel,
	BOOL bTexture, BOOL bCalcRadius, float* pRadius, BOOL bCalcAndDraw);
static void DrawSocketsAndAttachments(DrawStruct *pStruct);
static void DrawPiece(PieceLOD *pPiece, TVert *pVerts);


// ------------------------------------------------------------ //
// Set up the pixel format for windoze
// ------------------------------------------------------------ //
BOOL SetDCPixelFormat(HDC hDC)
{
	int nPixelFormat;
	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1, // Version
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32, // Bit depth
		0, 0, 0, 0, 0, 0,
		0, 0,
		0, 0, 0, 0, 0,
		32, // ZBuffer size
		0,
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0 };

	nPixelFormat = ChoosePixelFormat(hDC, &pfd);
	if(nPixelFormat == 0)
		return FALSE;
		
	return SetPixelFormat(hDC, nPixelFormat, &pfd);
}

// ------------------------------------------------------------ //
// CreateGLMContext( anonymous-window )
// Creates a GL-context from a window.
// ------------------------------------------------------------ //
GLM_CONTEXT CreateGLMContext(void *phWnd)
{
	GLMContext *pContext;
	HDC hDC;
	HWND hWnd;
	char buffer[2048];

	// Set the pixel format.
	hWnd = (HWND)phWnd;
	hDC = GetDC(hWnd);
	if(!SetDCPixelFormat(hDC))
	{
		ReleaseDC(hWnd, hDC);
		return NULL;
	}	

	pContext = new GLMContext;
	pContext->m_hWnd = hWnd;
	pContext->m_hDC = hDC;
	pContext->m_hglrc = wglCreateContext(pContext->m_hDC);
	pContext->m_nCurTexture = 0;
	wglMakeCurrent(pContext->m_hDC, pContext->m_hglrc);

    sprintf( buffer, "OEM:\t\t%s  \n  Version:\t%s\n  Renderer:\t%s\n", glGetString( GL_VENDOR ), glGetString( GL_VERSION ), glGetString( GL_RENDERER ) );
	return pContext;
}

// ------------------------------------------------------------------------
// Kill gl- context
// ------------------------------------------------------------------------
void DeleteGLMContext(GLM_CONTEXT hContext)
{
	GLMContext *pContext;

	pContext = (GLMContext*)hContext;

	// Delete the texture bindings
	for (DWORD i = 0; i < pContext->m_Textures.GetSize(); i++)
		if (pContext->m_Textures[i])
			glDeleteTextures(1, &(pContext->m_Textures[i]));

	wglMakeCurrent(pContext->m_hDC, NULL);
	wglDeleteContext(pContext->m_hglrc);
	ReleaseDC(pContext->m_hWnd, pContext->m_hDC);

	delete pContext;
}

// ------------------------------------------------------------------------
// DrawBox( color, min, max );
// ------------------------------------------------------------------------
void DrawBox (DVector *pColor, DVector *pMin, DVector *pMax)
{
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glColor3f (pColor->x, pColor->y, pColor->z);
	glBegin (GL_LINES);
		// top
		glVertex3f (pMin->x, pMin->y, pMin->z);
		glVertex3f (pMax->x, pMin->y, pMin->z);

		glVertex3f (pMax->x, pMin->y, pMin->z);
		glVertex3f (pMax->x, pMin->y, pMax->z);

		glVertex3f (pMax->x, pMin->y, pMax->z);
		glVertex3f (pMin->x, pMin->y, pMax->z);

		glVertex3f (pMin->x, pMin->y, pMax->z);
		glVertex3f (pMin->x, pMin->y, pMin->z);

		// bottom
		glVertex3f (pMax->x, pMax->y, pMax->z);
		glVertex3f (pMin->x, pMax->y, pMax->z);
		
		glVertex3f (pMin->x, pMax->y, pMax->z);
		glVertex3f (pMin->x, pMax->y, pMin->z);

		glVertex3f (pMin->x, pMax->y, pMin->z);
		glVertex3f (pMax->x, pMax->y, pMin->z);

		glVertex3f (pMax->x, pMax->y, pMin->z);
		glVertex3f (pMax->x, pMax->y, pMax->z);

		// sides

		glVertex3f (pMin->x, pMin->y, pMin->z);
		glVertex3f (pMin->x, pMax->y, pMin->z);

		glVertex3f (pMax->x, pMin->y, pMin->z);
		glVertex3f (pMax->x, pMax->y, pMin->z);

		glVertex3f (pMax->x, pMin->y, pMax->z);
		glVertex3f (pMax->x, pMax->y, pMax->z);

		glVertex3f (pMin->x, pMin->y, pMax->z);
		glVertex3f (pMin->x, pMax->y, pMax->z);

	glEnd();
}

// ------------------------------------------------------------------------
// DrawOriginLines()
// draws coordinate system
// ------------------------------------------------------------------------
void DrawOriginLines()
{
	float size;

	size = 150.0f;

	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBegin(GL_LINES);

		glColor3f(1.0f, 0.0f, 0.0f);
		glVertex3f(-size, 0.0f, 0.0f);
		glVertex3f( size, 0.0f, 0.0f);

		glColor3f(0.0f, 1.0f, 0.0f);
		glVertex3f(0.0f, -size, 0.0f);
		glVertex3f(0.0f,  size, 0.0f);

		glColor3f(0.0f, 0.0f, 1.0f);
		glVertex3f(0.0f, 0.0f, -size);
		glVertex3f(0.0f, 0.0f, size);
	
	glEnd();

	glPointSize(8.0f);
	glBegin(GL_POINTS);
		glVertex3f(size, 0.0f, 0.0f);
		glVertex3f(0.0f, size, 0.0f);
		glVertex3f(0.0f, 0.0f, size);
	glEnd();
}


// ------------------------------------------------------------------------
// AlternateGetVertexPositions
// ------------------------------------------------------------------------
/*
This uses an alternate (mathematically identical) method of setting up the 
transforms that allows it to transform the normals and allows it to only
use one base vertex position:

Current engine method:
NG = node global transform
VG = vertex in global space
PNG = precalculated node global transform
PVL = precalculated vertex in node local space
PVG = precalculated vertex in global space
PVL = ~PNG * PVG

To get a vertex's position.
VG = (NG0 * PVL0) + (NG1 * PVL1)...

// To get away from needing PVL0, PVL1... as input:
VG = NG * (~PNG * PVG) * w1 + NG2 * (~PNG2 * PVG) * w2

So in AlternateGetVertexPositions, we get rid of PVL's and use PVG.

If bNormals is TRUE, then it also transforms each vertex's normal and
places that right after each vertex's position.
*/

BOOL AlternateGetVertexPositions(	Model *pModel,
 									GVPStruct *pStruct, 
									BOOL bSetupTransforms,
									BOOL bNormals,
									BOOL bDrawOriginalModel,
									void **pNextVert /*= NULL*/ )
{
	DWORD i, j, k;
	ModelPiece *pPiece;
	BYTE *pCurOut;
	TransformMaker tMaker;
	float vTemp[4];
	ModelVert *pVert;
	LTMatrix *pTransforms;
	NewVertexWeight *pWeight;
	PieceLOD *pLOD;
	float theVec[4];
	

	// Validate..
	if(!pStruct->m_Vertices)
		return FALSE;

	if(pStruct->m_nAnims > MAX_GVP_ANIMS)
		return FALSE;

	if(bSetupTransforms)
	{
		tMaker.SetupFromGVPStruct(pStruct);
		if(!tMaker.SetupTransforms())
			return FALSE;
	}

	// Apply the inverse global transform according to the equation above.
	pTransforms = pModel->m_Transforms.GetArray();
	for(i=0; i < pModel->m_Transforms; i++)
	{
		pTransforms[i] = pTransforms[i] * pModel->GetNode(i)->GetInvGlobalTransform();
	}

	// Copy the vertex positions over.
	pCurOut = (BYTE*)pStruct->m_Vertices;

	for(i=0; i < pModel->NumPieces(); i++)
	{
		pPiece = pModel->GetPiece(i);
		pLOD = pPiece->GetLOD(pStruct->m_iLOD);

		for(j=0; j < pLOD->m_Verts; j++)
		{
			pVert = &pLOD->m_Verts[j];

			// Apply all the weights.
			if(pVert->m_nWeights > 0 && !bDrawOriginalModel)
			{
				vTemp[0] = vTemp[1] = vTemp[2] = vTemp[3] = 0.0f;
				for(k=0; k < pVert->m_nWeights; k++)
				{
					pWeight = &pVert->m_Weights[k];

					theVec[0] = pWeight->m_Vec[3] * pVert->m_Vec.x;
					theVec[1] = pWeight->m_Vec[3] * pVert->m_Vec.y;
					theVec[2] = pWeight->m_Vec[3] * pVert->m_Vec.z;
					theVec[3] = pWeight->m_Vec[3];

					MatVMul_Add(vTemp, &pTransforms[pWeight->m_iNode], theVec);
				}

				// Divide by w.
				vTemp[3] = 1.0f / vTemp[3];
				((DVector*)pCurOut)->x = vTemp[0] * vTemp[3];
				((DVector*)pCurOut)->y = vTemp[1] * vTemp[3];
				((DVector*)pCurOut)->z = vTemp[2] * vTemp[3];

				// Calculate normals
				if(bNormals)
				{
					vTemp[0] = vTemp[1] = vTemp[2] = vTemp[3] = 0.0f;
					for(k=0; k < pVert->m_nWeights; k++)
					{
						pWeight = &pVert->m_Weights[k];

						theVec[0] = pWeight->m_Vec[3] * pVert->m_Normal.x;
						theVec[1] = pWeight->m_Vec[3] * pVert->m_Normal.y;
						theVec[2] = pWeight->m_Vec[3] * pVert->m_Normal.z;
						theVec[3] = pWeight->m_Vec[3];

						MatVMul_Add_3x3(vTemp, &pTransforms[pWeight->m_iNode], theVec);
					}

					vTemp[3] = 1.0f / vTemp[3];
					((DVector*)(pCurOut + sizeof(DVector)))->x = vTemp[0] * vTemp[3];
					((DVector*)(pCurOut + sizeof(DVector)))->y = vTemp[1] * vTemp[3];
					((DVector*)(pCurOut + sizeof(DVector)))->z = vTemp[2] * vTemp[3];
				}
			}
			else
			{
				((DVector*)pCurOut)->Init();
				((DVector*)pCurOut)->x = pVert->m_Vec.x;
				((DVector*)pCurOut)->y = pVert->m_Vec.y;
				((DVector*)pCurOut)->z = pVert->m_Vec.z;
			}

			pCurOut += pStruct->m_VertexStride;
		}
	}

	if( pNextVert )
		*pNextVert = pCurOut;

	return TRUE;
}


// ------------------------------------------------------------------------
// Draw a line 
// ------------------------------------------------------------------------
void DrawArrow(float r, float g, float b, DVector pos, DVector vec, float length)
{
	DVector pos2;

	glColor3f(r, g, b);

	pos2 = pos + vec*length;
	glBegin(GL_LINES);
		glVertex3f(VEC_EXPAND(pos));
		glVertex3f(VEC_EXPAND(pos2));
	glEnd();
}

// ------------------------------------------------------------------------
// DrawCoordSys
// draw the rotation basis at pos, r/g/b = X/Y/Z
// ------------------------------------------------------------------------
void DrawCoordSys(DMatrix *pMat, float arrowLength)
{
	DVector up, right, forward;
	DVector pos;
	StateSet stateTexture(GL_TEXTURE_2D, FALSE);
	StateSet stateDepthTest(GL_DEPTH_TEST, FALSE);

	Mat_GetBasisVectors(pMat, &right, &up, &forward);
	pos.x = pMat->m[0][3];
	pos.y = pMat->m[0][7];
	pos.z = pMat->m[0][11];

	DrawArrow(1.0f, 0.0f, 0.0f, pos, right, arrowLength);
	DrawArrow(0.0f, 1.0f, 0.0f, pos, up, arrowLength);
	DrawArrow(0.0f, 0.0f, 1.0f, pos, forward, arrowLength);
}

// ------------------------------------------------------------------------
// RecurseAndDrawSkeleton()
// ------------------------------------------------------------------------
void RecurseAndDrawSkeleton(DrawStruct *pStruct, 
							ModelNode  *pNode, 
							LTVector    curNodePos, 
							BOOL       bChainStarted)
{
	DVector myPos;
	DWORD i;
	Model *pModel;
	BOOL bSelected;
	LTMatrix *pMat;

	pModel = pStruct->GetModel();

	// Transform the position.
	pMat = &pModel->m_Transforms[pNode->GetNodeIndex()];
	myPos.x = pMat->m[0][3];
	myPos.y = pMat->m[1][3];
	myPos.z = pMat->m[2][3];

	if(bChainStarted)
	{
		// The chain has already started so draw a line.
		glBegin(GL_LINES);
			glColor3f(1.0f, 1.0f, 1.0f);
			glVertex3f(VEC_EXPAND(curNodePos));
			glColor3f(0.2f, 0.2f, 0.2f);
			glVertex3f(VEC_EXPAND(myPos));
		glEnd();
	}

	// Draw a point if node is selected
	bSelected = pStruct->m_SelectedNodes[pNode->GetNodeIndex()];

	glPointSize(bSelected ? 8.0f : 4.0f);
	glBegin(GL_POINTS);

	if(bSelected)
	{
		// yellow
		glColor3f(1.0f, 1.0f, 0.0f);
	}
	else
	{
		// not yellow
		glColor3f(0.0f, 1.0f, 1.0f);
	}

	glVertex3f(VEC_EXPAND(myPos));

	glEnd();

	if(bSelected)
		DrawCoordSys( pMat, 6.0f );

	// Go into the children.
	for(i=0; i < pNode->m_Children; i++)
	{
		RecurseAndDrawSkeleton(pStruct, pNode->m_Children[i], myPos, TRUE);
	}
}

	
// ------------------------------------------------------------------------
// DrawSkeleton( draw-struct );
// recursively draws the skeleton, showing selected nodes with a large point and
// a coord-sys representation.
// ------------------------------------------------------------------------
void DrawSkeleton(DrawStruct *pStruct)
{
	TransformMaker tMaker;
	LTVector curNodePos;

	pStruct->SetupTransformMaker(&tMaker);
	if(!tMaker.SetupTransforms())
		return;
	
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	
	curNodePos.Init();
	RecurseAndDrawSkeleton(pStruct, pStruct->GetModel()->GetRootNode(), curNodePos, FALSE);
}


void DrawSocket(DrawStruct *pStruct, ModelSocket *pSocket)
{
	LTMatrix socketTransform;

	glLineStipple(2, 0xAAAA); // Dotted lines.
	glEnable(GL_LINE_STIPPLE);

	if(pStruct->GetModel()->GetSocketTransform(pSocket, &socketTransform))
	{
		DrawCoordSys(&socketTransform, 10.0f);
	}

	glDisable(GL_LINE_STIPPLE);
	glLineStipple(1, 0xFFFF);
}


// ------------------------------------------------------------------------
// DrawModel
// Entry point for rendering the model.
// ------------------------------------------------------------------------
void DrawModel(GLM_CONTEXT hContext, DrawStruct *pStruct)
{
	GLMContext *pContext;
	DMatrix baseTransform;
	DVector vMin, vMax;
	Model *pModel;
	uint32 renderTicks;
	RECT rect;
	char str[256];

	pModel = pStruct->GetModel();
	if(!pModel)
		return;
	
	pContext = (GLMContext*)hContext;

	g_pDrawStruct = pStruct;
	SetupViewingParameters(pContext);

	// Draw the model.
	renderTicks = 0;
	{
		CountAdder cntAdd(&renderTicks);

		Mat_Identity(&baseTransform);
		DrawModelPolies(hContext,
						pStruct->m_Times,
						pStruct->NumValidTimes(),
						pStruct->m_bDrawBright,
						&baseTransform,
						pStruct->m_SelectedPieces,
						pStruct->m_bNormals,
						pStruct->m_iLOD,
						pStruct->m_bDrawOriginalModel,
						TRUE,
						pStruct->m_bCalcRadius,
						&(pStruct->m_fModelRadius),
						pStruct->m_bCalcAndDraw);

		// jump out if we're not actually drawing anything
		if( pStruct->m_bCalcRadius && !pStruct->m_bCalcAndDraw )
			return;
	}

	if(pStruct->m_bDrawSkeleton)
	{
		DrawSkeleton(pStruct);
	}

	if (pStruct->m_bDims)
	{
		vMax = pModel->GetAnimInfo(pStruct->m_Times[0]->m_TimeRef.m_Prev.m_iAnim)->m_vDims;
		vMin = -vMax;
		DrawBox ((DVector*)&pStruct->m_DimsColor, &vMin, &vMax);
	}


	// Draw sockets.
	if(pStruct->m_bDrawAttachments || pStruct->m_bDrawSockets)
	{
		DrawSocketsAndAttachments(pStruct);
	}

	// Draw the normal reference.
	if(pModel->m_bNormalRef && pStruct->m_bShowNormalRef)
	{
		LTMatrix mNormalRef = pModel->m_Transforms[pModel->m_iNormalRefNode] * pModel->m_mNormalRef;
		DrawCoordSys(&mNormalRef, 15.0f);	// RETAIL: 15 (0x00474574), not the later 30
	}

	if (pStruct->m_bAnimBox)
	{
		vMax.x = vMax.y = vMax.z = pModel->m_GlobalRadius;
		vMin = -vMax;
		DrawBox ((DVector*)&pStruct->m_AnimBoxColor, &vMin, &vMax);
	}

	SwapBuffers(pContext->m_hDC);

	// Draw some status stuff..
	if(pStruct->m_bProfile)
	{
		rect.left = rect.top = 0;
		rect.right = 1000;
		rect.bottom = 100;
		sprintf(str, "Tris: %d, Ticks: %d", pModel->CalcNumTris(pStruct->m_iLOD), renderTicks);
		DrawText(pContext->m_hDC, str, strlen(str), &rect, DT_LEFT);
	}
}


// ------------------------------------------------------------------------
// SetupViewingParameters( GL-context )
// sets up the camera view for the the gl-context, lights and stuff, from
// g_pDrawStruct
// ------------------------------------------------------------------------
void SetupViewingParameters(GLMContext *pContext)
{
	RECT rect;
	float aspect;
	GLfloat ambient[4], diffuse[4], lightPos[4];
	int i;

	wglMakeCurrent(pContext->m_hDC, pContext->m_hglrc);

	GetClientRect(pContext->m_hWnd, &rect);
	aspect = (float)(rect.right - rect.left) / (rect.bottom - rect.top);

	// Set the viewport.
	glViewport(0, 0, rect.right, rect.bottom);

	// Setup the projection matrix.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, aspect, 0.1f, 5000.0f);

	// Setup the ModelView matrix.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	gluLookAt(EXPANDVEC(g_pDrawStruct->m_ViewerPos),
		EXPANDVEC(g_pDrawStruct->m_LookAt), 0.0f, 1.0f, 0.0f);

	// This puts it in Lithtech's (left-handed) coordinate system.
	glScalef(1.0f, 1.0f, -1.0f);	

	// Clear the screen.
	glClearColor(pContext->m_bgColor[0],pContext->m_bgColor[1],
				 pContext->m_bgColor[2],0.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	
	// Gouraud...
	glShadeModel(GL_SMOOTH);


	if(g_pDrawStruct->m_bWireframe)
	{
		glDisable(GL_LIGHTING);

		glDisable(GL_CULL_FACE);

		glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		// No ZBuffering..
		glDisable(GL_DEPTH_TEST);
	}
	else
	{
		// Setup lighting.
		glEnable(GL_LIGHTING);

		// Lights!
		for(i=0; i < MAX_GLM_LIGHTS; i++)
		{
			if(i < g_pDrawStruct->m_nLights)
			{
				ambient[0] = (g_pDrawStruct->m_LightColors[i].x / 255.0f) * 0.3f;
				ambient[1] = (g_pDrawStruct->m_LightColors[i].y / 255.0f) * 0.3f;
				ambient[2] = (g_pDrawStruct->m_LightColors[i].z / 255.0f) * 0.3f;
				ambient[3] = 1.0f;

				diffuse[0] = (g_pDrawStruct->m_LightColors[i].x / 255.0f) * 0.7f;
				diffuse[1] = (g_pDrawStruct->m_LightColors[i].y / 255.0f) * 0.7f;
				diffuse[2] = (g_pDrawStruct->m_LightColors[i].z / 255.0f) * 0.7f;
				diffuse[3] = 1.0f;

				lightPos[0] = g_pDrawStruct->m_LightPositions[i].x;
				lightPos[1] = g_pDrawStruct->m_LightPositions[i].y;
				lightPos[2] = -g_pDrawStruct->m_LightPositions[i].z;
				lightPos[3] = 1.0f;

				glLightfv(GL_LIGHT0+i, GL_AMBIENT, ambient);
				glLightfv(GL_LIGHT0+i, GL_DIFFUSE, diffuse);
				glLightfv(GL_LIGHT0+i, GL_POSITION, lightPos);
				glEnable(GL_LIGHT0+i);
			}
			else
			{
				glDisable(GL_LIGHT0+i);
			}
		}


		// Cull faces.
		glFrontFace(GL_CCW);
		glEnable(GL_CULL_FACE);

		// Set the polygon mode.
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// ZBuffering on.
		glEnable(GL_DEPTH_TEST);
	}
}


// ------------------------------------------------------------------------
// DrawModelPolies( )
// ------------------------------------------------------------------------
static void DrawModelPolies(GLM_CONTEXT hContext,
							AnimTracker **pTrackers,
                            DWORD nTrackers, 
							BOOL bDrawBright,
                            DMatrix *pRootTransform,
                            BYTE *pSelectedPieces,
							BOOL bNormals,
                            DWORD iLOD,
                            BOOL bDrawOriginalModel,
                            BOOL bTexture,
							BOOL bCalcRadius,
							float* pRadius,
							BOOL bCalcAndDraw )
{
	DWORD i, j, nWantedVerts;
	ModelPiece *pPiece;
	GVPStruct gvp;
	DVector v1, v2;
	TVert *pCurVert;
	PieceLOD *pLOD;
	static CMoArray<TVert> tVerts;
	Model *pModel;
	GLMContext *pContext;
	GLuint pieceTexture;

	if(nTrackers == 0)
		return;

	pModel = pTrackers[0]->m_TimeRef.m_pModel;
	if(!pModel)
		return;

	nTrackers = DMIN(nTrackers, MAX_GVP_ANIMS);

	// Size our list.
	nWantedVerts = pModel->GetTotalNumVerts() * 2;
	if(tVerts.GetSize() < nWantedVerts)
	{
		if(!tVerts.SetSize(nWantedVerts))
			return;
	}

	// Use the model code to setup the vertices.
	gvp.m_nAnims = 0;
	for(i=0; i < nTrackers; i++)
	{
		gvp.m_Anims[i] = pTrackers[i]->m_TimeRef;
		gvp.m_nAnims++;
	}

	gvp.m_Vertices = tVerts.GetArray();
	gvp.m_VertexStride = sizeof(TVert);
	gvp.m_BaseTransform = *pRootTransform;
	gvp.m_iLOD = iLOD;

	TVert* pEndVert;

	if(!AlternateGetVertexPositions(pModel, &gvp, TRUE, TRUE, bDrawOriginalModel, (void**)&pEndVert))
		return;

	// calculate the radius of this rendered frame of the model
	if( bCalcRadius && pRadius )
	{
		// get the inverse of the base transform
		LTMatrix mInvBase = pRootTransform->MakeInverse();
		// find the maximum magsqr of the vertex locations
		*pRadius = 0.0f;
		for( TVert* pCurVert = tVerts.GetArray(); pCurVert < pEndVert; ++pCurVert )
		{
			// put the vertex in world coords
			LTVector vWorldPos;
			mInvBase.Apply( pCurVert->m_vPos, vWorldPos );
			float fCurMag = vWorldPos.MagSqr();
			if( fCurMag > *pRadius )
				*pRadius = fCurMag;
		}
		// the sqrt of that is what we want
		*pRadius = (float)sqrt( *pRadius );
		if( !bCalcAndDraw )
			return;
	}


	if(bDrawBright)
	{
		glDisable(GL_LIGHTING);
		glColor3f(1.0f, 1.0f, 1.0f);
	}
	else
	{
		glColor3f(0.0f, 1.0f, 0.0f);
	}	

	if(bTexture)
		glEnable(GL_TEXTURE_2D);

	// Start with the first texture there is.
	pContext = (GLMContext *)hContext;
	if(pContext && bTexture && !pContext->m_nCurTexture)
	{
		for(i=0; i < pContext->m_Textures.GetSize(); i++)
		{
			if(pContext->m_Textures[i])
			{
				pContext->m_nCurTexture = pContext->m_Textures[i];
				glBindTexture(GL_TEXTURE_2D, pContext->m_nCurTexture);
				break;
			}
		}
	}

	glBegin(GL_TRIANGLES);

		pCurVert = tVerts.GetArray();
		for(i=0; i < pModel->NumPieces(); i++)
		{
			pPiece = pModel->GetPiece(i);

			// swap in the texture if required
			if(pContext && bTexture && (uint32)pPiece->m_iTexture < pContext->m_Textures.GetSize())
			{
				pieceTexture = pContext->m_Textures[pPiece->m_iTexture];
				if(pieceTexture && pieceTexture != pContext->m_nCurTexture)
				{
					glEnd();
					pContext->m_nCurTexture = pieceTexture;
					glBindTexture(GL_TEXTURE_2D, pContext->m_nCurTexture);
					glBegin(GL_TRIANGLES);
				}
			}

			pLOD = pPiece->GetLOD(iLOD);
			DrawPiece(pLOD, pCurVert);
			pCurVert += pLOD->m_Verts.GetSize();
		}

	glEnd();

	// Draw vertex normals.
	if(bNormals)
	{
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_LIGHTING);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glColor3f(0.0f, 1.0f, 0.0f);

		pCurVert = tVerts.GetArray();
		for(i=0; i < pModel->NumPieces(); i++)
		{
			pPiece = pModel->GetPiece(i);
			pLOD = pPiece->GetLOD(iLOD);

			for(j=0; j < pLOD->NumVerts(); j++)
			{
				v1 = pCurVert->m_vPos;
				v2 = v1 + pCurVert->m_vNormal * 5.0f;

				glBegin(GL_LINES);
					glVertex3f(EXPANDVEC(v1));
					glVertex3f(EXPANDVEC(v2));
				glEnd();

				pCurVert++;
			}
		}
	}


	glDisable(GL_TEXTURE_2D);
	if(bDrawBright)
	{
		glColor3f(0.0f, 1.0f, 0.0f);
	}


	DrawOriginLines();


	// If there are any selections, draw those too.
	glDisable(GL_LIGHTING);
	glEnable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if(pSelectedPieces)
	{
		glColor3f(1.0f, 1.0f, 0.0f);
		glBegin(GL_TRIANGLES);

			pCurVert = tVerts.GetArray();
			for(i=0; i < pModel->NumPieces(); i++)
			{
				pPiece = pModel->GetPiece(i);
				pLOD = pPiece->GetLOD(iLOD);
				
				if(pSelectedPieces[i])
				{
					DrawPiece(pLOD, pCurVert);
				}

				pCurVert += pLOD->m_Verts.GetSize();
			}

		glEnd();
	}
}


// ------------------------------------------------------------------------
// SubdivVert (talon: 2001's, the name is ours)
// A transformed vertex with its texture coordinates, which DrawPiece's
// subdivision interpolates.
// ------------------------------------------------------------------------
class SubdivVert : public TVert
{
public:
	inline SubdivVert&	operator=(SubdivVert other)
	{
		m_vPos = other.m_vPos;
		m_vNormal = other.m_vNormal;
		m_UV[0] = other.m_UV[0];
		m_UV[1] = other.m_UV[1];
		return *this;
	}

	SubdivVert	operator+(SubdivVert other)
	{
		SubdivVert ret;
		ret.m_vPos = m_vPos + other.m_vPos;
		ret.m_vNormal = m_vNormal + other.m_vNormal;
		ret.m_UV[0] = m_UV[0] + other.m_UV[0];
		ret.m_UV[1] = m_UV[1] + other.m_UV[1];
		return ret;
	}

	SubdivVert	operator-(SubdivVert other)
	{
		SubdivVert ret;
		ret.m_vPos = m_vPos - other.m_vPos;
		ret.m_vNormal = m_vNormal - other.m_vNormal;
		ret.m_UV[0] = m_UV[0] - other.m_UV[0];
		ret.m_UV[1] = m_UV[1] - other.m_UV[1];
		return ret;
	}

	SubdivVert	operator*(float scale)
	{
		SubdivVert ret;
		ret.m_vPos = m_vPos * scale;
		ret.m_vNormal = m_vNormal * scale;
		ret.m_UV[0] = m_UV[0] * scale;
		ret.m_UV[1] = m_UV[1] * scale;
		return ret;
	}

	float		m_UV[2];
};


// The point a third of the way from a to b, pulled into a's tangent plane.
// talon: near (0x405bc3, size-exact): the hidden return pointer for the
// operator+ call is pushed before the argument copy where the image pushes
// it after; scheduling only.
static void SubdivEdgePoint(SubdivVert *pOut, SubdivVert *a, SubdivVert *b)
{
	LTVector vDir = b->m_vPos - a->m_vPos;
	float fDot = a->m_vNormal.Dot(vDir) * g_fSubdivideTension;
	LTVector vTangent = vDir - a->m_vNormal * fDot;

	*pOut = *a + (*b - *a) * (1.0f / 3.0f);
	pOut->m_vPos = a->m_vPos + vTangent * (1.0f / 3.0f);
}


// a reflected through the midpoint of b and c.
static void SubdivReflect(SubdivVert *pOut, SubdivVert *a, SubdivVert *b, SubdivVert *c)
{
	SubdivVert vMid;

	vMid = (*b + *c) * 0.5f;
	*pOut = vMid - (*a - vMid);
}


// Draws a triangle, subdivided nLevels times into nine.
// talon: near (0x405781, size-exact): the center's three call temporaries
// and its by-value copy sit in permuted frame slots; instructions equal.
static void DrawSubdividedTri(SubdivVert **ppVerts, DWORD nLevels)
{
	SubdivVert *pTri[3];
	SubdivVert center;
	SubdivVert edges[6];
	SubdivVert outer[3];
	DWORD i;

	if(nLevels == 0)
	{
		LTVector vOrigin = ppVerts[0]->m_vPos;
		LTVector vNormal;
		vNormal = (ppVerts[1]->m_vPos - vOrigin).Cross(ppVerts[2]->m_vPos - vOrigin);
		vNormal.Norm();

		for(i=0; i < 3; i++)
		{
			glNormal3f(EXPANDVEC(ppVerts[i]->m_vNormal));
			glTexCoord2f(ppVerts[i]->m_UV[0] + g_tuAdd, ppVerts[i]->m_UV[1] + g_tvAdd);
			glVertex3f(EXPANDVEC(ppVerts[i]->m_vPos));
		}
		return;
	}

	// Two points on each edge.
	for(i=0; i < 3; i++)
	{
		SubdivEdgePoint(&edges[i*2], ppVerts[i], ppVerts[(i+1)%3]);
		SubdivEdgePoint(&edges[i*2+1], ppVerts[(i+1)%3], ppVerts[i]);
	}

	// The center.
	for(i=0; i < 3; i++)
	{
		SubdivReflect(&outer[i], ppVerts[i], &edges[i*2], &edges[(i*2-1) % 6]);
	}

	center = (outer[0] + outer[1] + outer[2]) * (1.0f / 3.0f);

	pTri[0] = ppVerts[0];	pTri[1] = &edges[0];	pTri[2] = &edges[5];	DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = &edges[0];	pTri[1] = &center;		pTri[2] = &edges[5];	DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = &edges[5];	pTri[1] = &center;		pTri[2] = &edges[4];	DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = &edges[4];	pTri[1] = &center;		pTri[2] = &edges[3];	DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = ppVerts[2];	pTri[1] = &edges[4];	pTri[2] = &edges[3];	DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = &edges[0];	pTri[1] = &edges[1];	pTri[2] = &center;		DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = &center;		pTri[1] = &edges[1];	pTri[2] = &edges[2];	DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = &center;		pTri[1] = &edges[2];	pTri[2] = &edges[3];	DrawSubdividedTri(pTri, nLevels - 1);
	pTri[0] = ppVerts[1];	pTri[1] = &edges[2];	pTri[2] = &edges[1];	DrawSubdividedTri(pTri, nLevels - 1);
}


// ------------------------------------------------------------------------
// Draw an LOD piece 
// 
// ------------------------------------------------------------------------
static void DrawPiece(PieceLOD *pPiece, TVert *pVerts)
{
	DWORD count;
	ModelTri *pCurTri;
	DVector *v[3];
	DVector normal;
	int i;
	SubdivVert verts[3];
	SubdivVert *pTri[3];

	// Draw the tris.
	pCurTri = pPiece->m_Tris.GetArray();
	count = pPiece->m_Tris.GetSize();

	if(g_nSubdivideLevels > 0 && (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
		(GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState(VK_MENU) & 0x8000))
	{
		pTri[0] = &verts[0];
		pTri[1] = &verts[1];
		pTri[2] = &verts[2];

		while(count)
		{
			count--;

			(TVert&)verts[0] = pVerts[pCurTri->m_Indices[GLM_TRI_INDEX0]];
			(TVert&)verts[1] = pVerts[pCurTri->m_Indices[GLM_TRI_INDEX1]];
			(TVert&)verts[2] = pVerts[pCurTri->m_Indices[GLM_TRI_INDEX2]];

			for(i=0; i < 3; i++)
			{
				verts[i].m_UV[0] = pCurTri->m_UVs[2-i].tu;
				verts[i].m_UV[1] = pCurTri->m_UVs[2-i].tv;
			}

			DrawSubdividedTri(pTri, g_nSubdivideLevels);
			pCurTri++;
		}
	}
	else
	{
		while(count)
		{
			count--;
	        
			v[0] = &pVerts[pCurTri->m_Indices[GLM_TRI_INDEX0]].m_vPos;
			v[1] = &pVerts[pCurTri->m_Indices[GLM_TRI_INDEX1]].m_vPos;
			v[2] = &pVerts[pCurTri->m_Indices[GLM_TRI_INDEX2]].m_vPos;

			normal = (*v[1] - *v[0]).Cross(*v[2] - *v[0]);
			normal.Norm();
			
			for(i=0; i < 3; i++)
			{
				glNormal3f(EXPANDVEC(pVerts[pCurTri->m_Indices[2-i]].m_vNormal));
				glTexCoord2f(
					pCurTri->m_UVs[2-i].tu + g_tuAdd,
					pCurTri->m_UVs[2-i].tv + g_tvAdd);
				glVertex3f(EXPANDVEC(*v[i]));
			}
			
			pCurTri++;
		}
	}
}


// ------------------------------------------------------------------------
// DrawAttachement( model, socket-transform, lod-level, solid )
// Draw the model attached to a socket.
// ------------------------------------------------------------------------
static void DrawAttachment(Model *pAttachment,
                           DMatrix *pTransform,
                           DWORD iLOD,
                           BOOL bSolid)
{
	AnimTracker tracker, *pTracker;
	StateSet stateLighting(GL_LIGHTING, bSolid);
	StateSet stateCull(GL_CULL_FACE, bSolid);
	StateSet stateDepth(GL_DEPTH_TEST, bSolid);
	GLenum eOldPolyMode;
	glGetIntegerv(GL_POLYGON_MODE, (int *)&eOldPolyMode);
	glPolygonMode(GL_FRONT_AND_BACK, (bSolid) ? GL_FILL : GL_LINE);

	tracker.m_TimeRef.Init(pAttachment, 0, 0, 0, 0, 0.0f);
	pTracker = &tracker;

	DrawModelPolies(NULL,
                    &pTracker,
                    1,
                    FALSE,
                    pTransform,
                    NULL,
                    FALSE,
                    iLOD,
                    FALSE,
                    TRUE,
                    FALSE,
                    NULL,
                    FALSE);

	glPolygonMode(GL_FRONT_AND_BACK, eOldPolyMode);
}


static void DrawSocketsAndAttachments(DrawStruct *pStruct)
{
	uint32 i;
	ModelSocket *pSocket;
	Model *pModel;
	TransformMaker tMaker;
	LTMatrix mSocket;

	// Setup transforms.  
	pStruct->SetupTransformMaker(&tMaker);
	if(!tMaker.SetupTransforms())
		return;

	pModel = pStruct->GetModel();
	for(i=0; i < pModel->NumSockets(); i++)
	{
		pSocket = pModel->GetSocket(i);
		
		if(pStruct->m_bDrawAttachments && pSocket->m_pAttachment)
		{
			if(pModel->GetSocketTransform(pSocket, &mSocket))
			{
				DrawAttachment(
					pSocket->m_pAttachment, 
					&mSocket, 
					pStruct->m_iLOD, 
					pStruct->m_bSolidAttachments);
			}
		}

		if(pStruct->m_bDrawSockets)
		{
			DrawSocket(
				pStruct, 
				pSocket);
		}
	}
}


BOOL SetGLMTexture(GLM_CONTEXT hContext, TextureData *pTexture, DWORD nTextureNum)
{
	BYTE *p32BitTexture;
	FormatMgr formatMgr;
	PFormat srcFormat, destFormat;
	ConvertRequest request;
	DRESULT dResult;
	TextureMipData *pMip;
	DWORD err;
	GLMContext *pContext;


	if(!(pContext = (GLMContext*)hContext))
		return FALSE;

	wglMakeCurrent(pContext->m_hDC, pContext->m_hglrc);
	pMip = &pTexture->m_Mips[0];

	// Convert to 32-bit.
	p32BitTexture = new BYTE[pMip->m_Width * pMip->m_Height * 4];
	if(!p32BitTexture)
		return FALSE;

	dtx_SetupDTXFormat(pTexture, &srcFormat);
	destFormat.Init(BPP_32, 0xFF000000, 0x000000FF, 0x0000FF00, 0x00FF0000);

	request.m_pSrcFormat = &srcFormat;
	request.m_pSrc = pTexture->m_Mips[0].m_Data;
	request.m_SrcPitch = pMip->m_Pitch;
	request.m_pDestFormat = &destFormat;
	request.m_pDest = p32BitTexture;
	request.m_DestPitch = pMip->m_Width * 4;
	request.m_Width = pMip->m_Width;
	request.m_Height = pMip->m_Height;
	request.m_Flags = 0;

	dResult = formatMgr.ConvertPixels(&request);
	if(dResult != LT_OK)
		return FALSE;

	// Clear the error state
	err = glGetError();

	while (pContext->m_Textures.GetSize() <= nTextureNum)
		pContext->m_Textures.Append(0);

	// Free the texture channel if it's already allocated
	if (pContext->m_Textures[nTextureNum])
	{
		glDeleteLists(pContext->m_Textures[nTextureNum], 1);
		err = glGetError();
	}

	// Create a texture binding for the texture
	GLuint nTextureID;
	glGenTextures(1, &nTextureID);
	pContext->m_Textures[nTextureNum] = nTextureID;
	glBindTexture(GL_TEXTURE_2D, nTextureID);
	err = glGetError();

	glDisable(GL_TEXTURE_1D);
	glEnable(GL_TEXTURE_2D);
	err = glGetError();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);

	glTexImage2D(GL_TEXTURE_2D, 0, 4, pMip->m_Width, pMip->m_Height, 0, 
		GL_RGBA, GL_UNSIGNED_BYTE, p32BitTexture);
	err = glGetError();

	delete p32BitTexture;

	// Adjust for bilinear filtering.
	g_tuAdd = 0.5f / (float)pMip->m_Width;
	g_tvAdd = 0.5f / (float)pMip->m_Height;
	return TRUE;
}

// ----------------------------------------------------------
// SetGLMBackgroundColor( gl-context, COLORREF )
// ----------------------------------------------------------
void SetGLMBackgroundColor( GLM_CONTEXT hContext, COLORREF win_color )
{
	GLMContext *pContext ;
	unsigned char *pColDecomp = (unsigned char*)&win_color;

	if(!(pContext = (GLMContext*)hContext))
	{
		ASSERT( pContext != NULL ); // let windows deal with this error.
		return ;
	}

	// unitize
	pContext->m_bgColor[0] = pColDecomp[0]/256.0f;	// RETAIL: by 256 (0x004745dc's 1/256), not 255
	pContext->m_bgColor[1] = pColDecomp[1]/256.0f;
	pContext->m_bgColor[2] = pColDecomp[2]/256.0f;

}


// ----------------------------------------------------------
// SetGLMBackgroundColor( gl-context, COLORREF )
// ----------------------------------------------------------
void SetGLMBackgroundColor( GLM_CONTEXT hContext, float *color )
{
	GLMContext *pContext ;
	if(!(pContext = (GLMContext*)hContext))
	{
		ASSERT( pContext != NULL ); // let windows deal with this error.
		return ;
	}

	pContext->m_bgColor[0] = color[0];
	pContext->m_bgColor[1] = color[1];
	pContext->m_bgColor[2] = color[2];

}




BOOL DrawStruct::SetupTransformMaker(TransformMaker *pMaker)
{
	DWORD i;

	pMaker->m_nAnims = 0;
	for(i=0; i < NUM_ANIM_INFOS && pMaker->m_nAnims < MAX_GVP_ANIMS; i++)
	{
		if(m_Times[i]->m_TimeRef.m_pModel)
		{
			pMaker->m_Anims[pMaker->m_nAnims] = m_Times[i]->m_TimeRef;
			pMaker->m_nAnims++;
		}
	}

	return pMaker->m_nAnims > 0;
}


DWORD DrawStruct::NumValidTimes()
{
	DWORD i, nTimes;

	nTimes = 0;
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		if(!m_Times[i]->m_TimeRef.m_pModel)
			break;

		++nTimes;
	}

	return nTimes;
}
