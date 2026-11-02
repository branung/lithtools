// ------------------------------------------------------------------------
// model.cpp
// implementation details of models
// ------------------------------------------------------------------------
#include "bdefs.h"
#include "model.h"
#include "model_ops.h"
#include "transformmaker.h"

#include <set>


#pragma warning (disable:4786)

#define MAX_SKELETON_NODES	256
#define NUM_RADIUS_TWEENS	25
#define DEFAULT_MODEL_VIS_RADIUS	50.0f
#define DEFAULT_MODEL_FADE_RANGE	20000.0f	// talon: 0x4770ec

#define DEFAULT_MODEL_AMBIENT_LIGHT	0.25f
#define DEFAULT_MODEL_DIR_LIGHT		0.75f


// ------------------------------------------------------------------------
// globals, and console vars 
// ------------------------------------------------------------------------

// Used so we can track if the filename is allocated or not.
static char *g_pNoModelFilename = "";
float g_CV_DefaultDrawIndexedDist = 500.0f;


// Global value to track model memory usage
unsigned long g_ModelMemory = 0;

// ------------------------------------------------------------------------
// File scope classes 
// ------------------------------------------------------------------------


// Used in ParseCommandString.
class SimpleFloatVar
{
public:
	char	*m_pName;
	float	*m_pVal;
};


class TransformMap
{
public:
				TransformMap() {}

				TransformMap(ModelNode *pNode, uint32 transform)
				{
					m_ScratchData = pNode->ScratchData();
					m_OldTransform = m_NewTransform = transform;
				}

	uint32		m_ScratchData;
	uint32		m_OldTransform;
	uint32		m_NewTransform;
};


// ------------------------------------------------------------------------ //
// Helpers.
// ------------------------------------------------------------------------ //

static TransformMap* FindTransformMapByNode(CMoArray<TransformMap> &tMap, ModelNode *pNode)
{
	uint32 i;
	for(i=0; i < tMap.GetSize(); i++)
	{
		if(tMap[i].m_ScratchData == pNode->ScratchData())
			return &tMap[i];
	}
	return LTNULL;
}

static TransformMap* FindTransformMapByOldIndex(CMoArray<TransformMap> &tMap, uint32 index)
{
	uint32 i;
	for(i=0; i < tMap.GetSize(); i++)
	{
		if(tMap[i].m_OldTransform == index)
			return &tMap[i];
	}
	return LTNULL;
}

// Applies pMat to pRoot and adjusts pRoot's children so they end up the same as they were.
static void ApplyMatrixToKeyframes(AnimNode *pRoot, LTMatrix transform, uint32 iKey)
{
	LTMatrix outMat, myRot;
	uint32 i;
	AnimNode *pChild;
	LTVector right, up, forward, pos;
	NodeKeyFrame *pKeyframe;

	// Apply it to the root.
	pRoot->m_KeyFrames[iKey].m_Quaternion.ConvertToMatrix(myRot);
	MatMul(&outMat, &transform, &myRot);
	pRoot->m_KeyFrames[iKey].m_Quaternion.ConvertFromMatrix(outMat);

	for(i=0; i < pRoot->m_pNode->NumChildren(); i++)
	{
		pChild = pRoot->m_Children[i];
		pKeyframe = &pChild->m_KeyFrames[iKey];

		pKeyframe->m_Quaternion.ConvertToMatrix(myRot);
		MatMul(&outMat, &transform, &myRot);
		pKeyframe->m_Quaternion.ConvertFromMatrix(outMat);

		Mat_GetBasisVectors(&myRot, &right, &up, &forward);
		pos.Init();
		pos += right * pKeyframe->m_Translation.x;
		pos += up * pKeyframe->m_Translation.y;
		pos += forward * pKeyframe->m_Translation.z;

		Mat_GetBasisVectors(&outMat, &right, &up, &forward);
		pKeyframe->m_Translation.x = right.Dot(pos);
		pKeyframe->m_Translation.y = up.Dot(pos);
		pKeyframe->m_Translation.z = forward.Dot(pos);
	}
}


static LTBOOL CreateDummyAnimNodes(Model *pModel, ModelAnim *pAnim, 
	ModelNode *pNode, AnimNode *pAnimNode, AnimNode *pParentAnimNode)
{
	uint32 i;
	AnimNode *pNewNode;

	pAnimNode->m_pNode = pNode;
	pAnimNode->m_pParentNode = pParentAnimNode;
	
	pAnimNode->m_KeyFrames.SetSize(1);
	pAnimNode->m_KeyFrames[0].m_Translation.Init();
	pAnimNode->m_KeyFrames[0].m_Quaternion.Init();

	for(i=0; i < pNode->NumChildren(); i++)
	{
		pNewNode = new AnimNode(pAnim, pAnimNode);
		pAnimNode->m_Children.Append(pNewNode);

		if(!CreateDummyAnimNodes(pModel, pAnim, pNode->m_Children[i], 
			pNewNode, pAnimNode))
		{
			return false;
		}
	}

	return true;
}


static void _ChangeVertexReferences(uint32 from, uint32 to, PieceLOD *pNode)
{
	ModelTri *pTri;
	uint32 i, j;

	for(i=0; i < pNode->m_Tris; i++)
	{
		pTri = &pNode->m_Tris[i];

		for(j=0; j < 3; j++)
		{
			if(pTri->m_Indices[j] == from)
				pTri->m_Indices[j] = (uint16)to;
			
			// Decrement if the vertex is greater than the one we're removing since the 
			// array will slide down.
			if(pTri->m_Indices[j] > from)
				pTri->m_Indices[j]--;
		}
	}

}


static void _ChangeVertexWeightReferences(Model *pModel, 
	PieceLOD *pPiece, 
	uint32 iVert,
	uint32 iReplaceWith)
{
	uint32 iNode;
	
	for(iNode=0; iNode < pModel->NumNodes(); iNode++)
	{
		pModel->GetNode(iNode)->ChangeVertexReferences(pPiece, iVert, iReplaceWith);		
	}
}


// Convert the transforms from local to global space.
void _TransformsLtoG(AnimNode *pAnimNode, uint32 iFrame, LTMatrix *pParentTransform)
{
	LTMatrix localTransform, globalTransform;
	uint32 i;

	pAnimNode->m_KeyFrames[iFrame].ConvertToMatrix(localTransform);
	MatMul(&globalTransform, pParentTransform, &localTransform);
	pAnimNode->m_KeyFrames[iFrame].ConvertFromMatrix(globalTransform);

	for(i=0; i < pAnimNode->m_Children; i++)
	{
		_TransformsLtoG(pAnimNode->m_Children[i], iFrame, &globalTransform);
	}
}


// Convert the transforms from global to local space.
// Global = Parent * Local so:
// Local = ~Parent * Global
void _TransformsGtoL(AnimNode *pAnimNode, uint32 iFrame, LTMatrix *pParentTransform)
{
	LTMatrix inverseParent, localTransform, globalTransform;
	uint32 i;

	pAnimNode->m_KeyFrames[iFrame].ConvertToMatrix(globalTransform);
	Mat_InverseTransformation(pParentTransform, &inverseParent);
	MatMul(&localTransform, &inverseParent, &globalTransform);	

	pAnimNode->m_KeyFrames[iFrame].ConvertFromMatrix(localTransform);

	for(i=0; i < pAnimNode->m_Children; i++)
	{
		_TransformsGtoL(pAnimNode->m_Children[i], iFrame, &globalTransform);
	}
}


// Sets the given node's keyframe to identity.
void _ResetNodeKeyFrames(AnimNode *pAnimNode, ModelNode *pNode, uint32 iFrame)
{
	uint32 i;

	if(pAnimNode->m_pNode == pNode)
	{
		pAnimNode->m_KeyFrames[iFrame].Identity();
	}

	for(i=0; i < pAnimNode->m_Children; i++)
	{
		_ResetNodeKeyFrames(pAnimNode->m_Children[i], pNode, iFrame);
	}
}


LTRESULT DefaultLoadChildFn(ModelLoadRequest *pRequest, Model **pModel)
{
	return LT_NOCHANGE;
}


void UpdateDistSqr(CMoArray< LTVector > &verts, float &maxDistSqr)
{
	uint32 i;
	LTVector *pCurVert;
	float testDist;

	pCurVert = verts.GetArray();
	for(i=0; i < verts; i++)
	{
		testDist = pCurVert->MagSqr();
		if(testDist > maxDistSqr)
		{
			maxDistSqr = testDist;
		}
	
		pCurVert++;
	}
}


LTBOOL VerifyChildModel_R(ModelNode *pParentNode, ModelNode *pChildNode, ModelNode* &pErrNode)
{
	uint32 i;

	if(pParentNode->NumChildren() != pChildNode->NumChildren())
	{
		pErrNode = pParentNode;
		return false;
	}

	for(i=0; i < pParentNode->NumChildren(); i++)
	{
		if(!VerifyChildModel_R(pParentNode->GetChild(i), pChildNode->GetChild(i), pErrNode))
			return false;
	}

	return true;
}



// ------------------------------------------------------------------------ //
// Exposed helpers.
// ------------------------------------------------------------------------ //

char* model_FindExporterToken(char *pName, char *pToken)
{
	char *pCur, *pCurName, *pCurToken;
	bool bSame;

	pCur = pName;
	while(*pCur)
	{
		if(pCur[0] == '_' && pCur[1] == 'z')
		{
			bSame = true;
			pCurName = &pCur[2];
			pCurToken = pToken;
			while(*pCurToken)
			{
				if(toupper(*pCurToken) != toupper(*pCurName))
				{
					bSame = false;
					break;
				}

				pCurToken++;
				pCurName++;
			}

			if(bSame)
				return &pCur[2];
		}
	
		pCur++;
	}
	
	return LTNULL;
}


// ------------------------------------------------------------------------ //
// ModelStringList.
// ------------------------------------------------------------------------ //

ModelStringList::ModelStringList(LAlloc *pAlloc)
{
	m_pAlloc = pAlloc;
	m_StringList = LTNULL;
}


ModelStringList::~ModelStringList()
{
	Term();
}


void ModelStringList::Term()
{
	ModelString *pCurString, *pNextString;

	pCurString = m_StringList;
	while(pCurString)
	{
		pNextString = pCurString->m_pNext;
		LDelete(GetAlloc(), pCurString);
		pCurString = pNextString;
	}
	m_StringList = LTNULL;
}


char* ModelStringList::AddString(const char *pString)
{
	ModelString *pCur, *pRet;
	int len;
	uint32 dwSize;

	// Quick exit..
	if(!pString || pString[0] == 0)
		return "";

	pCur = m_StringList;
	while(pCur)
	{
		if(strcmp(pCur->m_String, pString) == 0)
			return pCur->m_String;
	
		pCur = pCur->m_pNext;
	}

	// Ok, add a new string.
	len = strlen(pString) + 1;
	dwSize = sizeof(ModelString) - 1 + len;
	pRet = (ModelString*)GetAlloc()->Alloc(dwSize);
	if(!pRet)
		return "";

	pRet->m_AllocSize = dwSize;
	pRet->m_pNext = m_StringList;
	m_StringList = pRet;

	strcpy(pRet->m_String, pString);
	return pRet->m_String;
}


LTBOOL ModelStringList::SetAlloc(LAlloc *pAlloc)
{
	if(m_StringList)
	{
		ASSERT(false);
		return false;
	}

	m_pAlloc = pAlloc;
	return true;
}



// ------------------------------------------------------------------------ //
// AnimTimeRef.
// ------------------------------------------------------------------------ //

LTBOOL AnimTimeRef::IsValid()
{
	return m_pModel && 
		m_Prev.m_iAnim < m_pModel->m_Anims && 
		m_Cur.m_iAnim < m_pModel->m_Anims &&
		m_Prev.m_iFrame < m_pModel->GetAnim(m_Prev.m_iAnim)->m_KeyFrames &&
		m_Cur.m_iFrame < m_pModel->GetAnim(m_Cur.m_iAnim)->m_KeyFrames &&
		m_Percent >= 0.0f && m_Percent <= 1.0f;
}

ModelAnim* AnimTimeRef::GetAnim1()
{
	ASSERT(m_Prev.m_iAnim < m_pModel->m_Anims);
	return m_pModel->GetAnim(m_Prev.m_iAnim);
}

ModelAnim* AnimTimeRef::GetAnim2()
{
	ASSERT(m_Cur.m_iAnim < m_pModel->m_Anims);
	return m_pModel->GetAnim(m_Cur.m_iAnim);
}



// ------------------------------------------------------------------------ //
// NewVertexWeight.
// ------------------------------------------------------------------------ //

NewVertexWeight::NewVertexWeight()
{
	m_Vec[0] = m_Vec[1] = m_Vec[2] = m_Vec[3] = 0.0f;
	m_iNode = 0;
}


// ------------------------------------------------------------------------ //
// ModelVert.
// ------------------------------------------------------------------------ //

ModelVert::ModelVert()
{
	m_Weights = LTNULL;
	m_nWeights = 0;
	m_Vec.Init();
	m_Normal.Init();
}


// note :
// this allocates memory that may not be freed.
// in modeledit, the lod builder uses this method 
// memory created here from that is not properly dealloc'd
NewVertexWeight* ModelVert::AddWeight()
{
	NewVertexWeight *pNewList;
	uint32 i;

	pNewList = new NewVertexWeight[m_nWeights+1];
	if(!pNewList)
		return LTNULL;

	for(i=0; i < m_nWeights; i++)
		pNewList[i] = m_Weights[i];

	delete m_Weights;
	m_Weights = pNewList;
	m_nWeights++;

	return &m_Weights[m_nWeights-1];
}

// ------------------------------------------------------------------------
// CDefVertexList
/* CDefVertexList's data :
	
		struct defVert {
			float pos[3];
		};
		// data array
		CMoArray<defVert, NoCache> m_vVertexList ;
		LAlloc                    *m_pAlloc ;
*/
// ------------------------------------------------------------------------

// default constructor 	
CDefVertexLst::	CDefVertexLst()
{
	//nothing 
}

// array create constructor
CDefVertexLst::CDefVertexLst( int size, LAlloc *alloc )
{
	m_pAlloc = alloc ;
	m_vVertexList.SetSize2( size, alloc );
}

CDefVertexLst::~CDefVertexLst()
{
	m_vVertexList.Term(m_pAlloc);
	m_pAlloc = NULL ;
}

// pre set the size of the array
void CDefVertexLst::setSize( int size , LAlloc *alloc )
{
	/* possible weirdness :
		if m_vVertexList is alloced already and alloc is different from m_pAlloc.
	*/
	m_pAlloc = alloc ;
	m_vVertexList.SetSize2( size, alloc );
}

// get value at index (copy of data )
// index is unchecked
void CDefVertexLst::getValue( int index, float val[3] )    
{
	defVert & dv = m_vVertexList[index];
	memcpy(val, dv.pos, sizeof(float)*3);
}

// get value at index (pointer to actual data )
// note: don't use pointer as the head of an array, defVert may 
// change..
float * CDefVertexLst::getValue( int index )
{
	defVert & dv = m_vVertexList[index];
	return (float*)&dv.pos;
}

// add values at the end of the list
void CDefVertexLst::append( float val[3] )
{
	defVert dv ;
	for( int i = 0 ;i < 3 ; i++ )
		dv.pos[i] = val[i];
	//memcpy( dv.pos, val, sizeof(float)*3);
	//int lsize = m_vVertexList.GetSize();
	//m_vVertexList.Insert2(lsize, dv , m_pAlloc );
	m_vVertexList.Append( dv );
}

// append 
void CDefVertexLst::append( float x, float y, float z)
{
	defVert dv;
	dv.pos[0] = x ;dv.pos[1] = y ;dv.pos[2] = z ;
	//int lsize = m_vVertexList.GetSize();
	//m_vVertexList.Insert2(lsize, dv, m_pAlloc );
	m_vVertexList.Append( dv );
}


// ------------------------------------------------------------------------ //
// ChildModel.
// ------------------------------------------------------------------------ //

ChildInfo::ChildInfo()
{
	m_AnimOffset = 0;
	m_pFilename = "";
	m_pModel = LTNULL;
	m_pParentModel = LTNULL;
	m_bBoundRadiusValid = false;
	m_bTreeValid = true;
}

ChildInfo::~ChildInfo()
{
	Term();
}

// Note this does not seem to delete the child model.
void ChildInfo::Term()
{
	m_pModel = LTNULL;
	m_pFilename = "";
	m_Relation.Term(GetAlloc());
}


// ------------------------------------------------------------------------ //
// AnimKeyFrame.
// ------------------------------------------------------------------------ //

AnimKeyFrame::AnimKeyFrame()
{
	m_Time = 0;
	m_pString = "";
	m_KeyType = KEYTYPE_POSITION;
	m_Callback = LTNULL;
	m_pUser1 = LTNULL;
}


// ------------------------------------------------------------------------ //
// AnimNode.
// 
// ------------------------------------------------------------------------ //

// ------------------------------------------------------------------------
// Constructor
// ------------------------------------------------------------------------
AnimNode::AnimNode()
{
	Clear();
}

AnimNode::AnimNode(ModelAnim *pAnim, AnimNode *pParent)
{
	Clear();
	m_pAnim = pAnim;
	m_pParentNode = pParent;
}

AnimNode::~AnimNode()
{
	Term();
}

void AnimNode::Clear()
{
	m_pAnim = LTNULL;
	m_pParentNode = LTNULL;
	m_pNode = LTNULL;
	m_KeyFrames = LTNULL;
}

void AnimNode::Term()
{
	m_KeyFrames.Term(GetAlloc());
	DeleteAndClearArray(m_Children, GetAlloc());
}


void AnimNode::SetAnim(ModelAnim *pAnim)
{
	m_pAnim = pAnim;
}


AnimNode* AnimNode::AllocateSameKind(ModelAnim *pAnim, AnimNode *pParent)
{
	return new AnimNode(pAnim, pParent);
}


LTBOOL AnimNode::FillAnimNodeList(uint32 &curNodeIndex)
{
	unsigned long i;

	if(!m_pAnim->m_AnimNodes || curNodeIndex >= GetModel()->NumNodes())
	{
		ASSERT(false);
		return false;
	}

	m_pAnim->m_AnimNodes[curNodeIndex] = this;
	++curNodeIndex;

	for(i=0; i < NumChildren(); i++)
	{
		if(!m_Children[i]->FillAnimNodeList(curNodeIndex))
			return false;
	}

	return true;
}


LTBOOL AnimNode::SetNodePointers(ModelNode *pNode)
{
	uint32 i;

	m_pNode = pNode;

	if(m_Children.GetSize() != pNode->m_Children.GetSize())
	{
		ASSERT(false);
		return false;
	}

	for(i=0; i < m_Children; i++)
	{
		m_Children[i]->SetNodePointers(pNode->m_Children[i]);
	}

	return true;
}


Model* AnimNode::GetModel()
{
	return m_pAnim->GetModel();
}


LTBOOL AnimNode::RemoveKeyFrame(uint32 iKeyFrame)
{
	uint32 i;

	ASSERT(iKeyFrame < NumKeyFrames());

	// Slide our keyframes back one.
	m_KeyFrames.Remove(iKeyFrame);

	// Recurse.
	for(i=0; i < m_Children; i++)
	{
		if(!m_Children[i]->RemoveKeyFrame(iKeyFrame))
			return false;
	}

	return true;
}


uint32 AnimNode::NumKeyFrames()
{
	return m_pAnim->NumKeyFrames();
}


LTBOOL AnimNode::CopyKeyFrame(uint32 iKeyFrame, AnimNode *pDest)
{
	uint32 i;
	AnimNode *pNewChild;


	if(iKeyFrame >= NumKeyFrames())
	{
		ASSERT(false);
		return false;
	}


	// Add the keyframe.
	ASSERT(!pDest->m_KeyFrames);
	pDest->m_KeyFrames.SetSize(1);
	if(!pDest->m_KeyFrames)
		return false;

	pDest->m_KeyFrames[0] = m_KeyFrames[iKeyFrame];

	// Recurse.
	for(i=0; i < m_Children; i++)
	{
		pNewChild = AllocateSameKind(m_pAnim, pDest);
		if(!pNewChild)
			return false;

		if(!m_Children[i]->CopyKeyFrame(iKeyFrame, pNewChild) || !pDest->m_Children.Append(pNewChild))
		{
			delete pNewChild;
			return false;
		}
	}

	return true;
}


LTBOOL AnimNode::PasteKeyFrame(AnimNode *pSrc, uint32 iBefore)
{
	uint32 i;


	// Make sure we're compatible.
	if(!pSrc->m_KeyFrames || pSrc->m_Children.GetSize() != m_Children.GetSize())
		return false;

	// Add to the keyframe array.
	m_KeyFrames.Insert(iBefore, pSrc->m_KeyFrames[0]);

	// Recurse.
	for(i=0; i < m_Children; i++)
	{
		if(!m_Children[i]->PasteKeyFrame(pSrc->m_Children[i], iBefore))
			return false;
	}

	return true;
}

// talon: 2001's (0x429dd3), not in Build 69; the name is ours. Sets keyframe
// iDest to the interpolation of iFrom and iTo at t, over the tree. Retail
// bug: the indices are checked with > against NumKeyFrames(), so an index
// equal to the count gets through (RETAIL_BUGS.md).
LTBOOL AnimNode::InterpolateKeyFrame(uint32 iDest, uint32 iFrom, uint32 iTo, float t)
{
	NodeKeyFrame *pDest, *pFrom, *pTo;
	uint32 i;

	if(iDest > NumKeyFrames() || iFrom > NumKeyFrames() || iTo > NumKeyFrames() || iFrom > iTo)
		return FALSE;

	if(t < 0.0f || t > 1.0f)
		return FALSE;

	pDest = &m_KeyFrames[iDest];
	pFrom = &m_KeyFrames[iFrom];
	pTo = &m_KeyFrames[iTo];
	if(!pDest || !pFrom || !pTo)
		return FALSE;

	pDest->m_Quaternion.Slerp(pFrom->m_Quaternion, pTo->m_Quaternion, t);

	if(!(m_pNode->m_Flags & MNODE_ROTATIONONLY))
	{
		pDest->m_Translation.x = pFrom->m_Translation.x + (pTo->m_Translation.x - pFrom->m_Translation.x) * t;
		pDest->m_Translation.y = pFrom->m_Translation.y + (pTo->m_Translation.y - pFrom->m_Translation.y) * t;
		pDest->m_Translation.z = pFrom->m_Translation.z + (pTo->m_Translation.z - pFrom->m_Translation.z) * t;
	}

	for(i=0; i < m_Children; i++)
	{
		if(m_Children[i])
			m_Children[i]->InterpolateKeyFrame(iDest, iFrom, iTo, t);
	}

	return TRUE;
}

// ------------------------------------------------------------------------ //
// ModelAnim functions.
// ------------------------------------------------------------------------ //

ModelAnim::ModelAnim(Model *pModel)
{
	m_pModel = pModel;
	m_pName = "";
	m_AnimNodes = LTNULL;
	m_ModelEditWeightSet = INVALID_MODEL_WEIGHTSET;
	m_InterpolationMS = 200;

	m_pRootNode = &m_DefaultRootNode;
	m_pRootNode->SetAnim(this);
}


ModelAnim::~ModelAnim()
{
	Term();
}


void ModelAnim::Term()
{
	if(m_AnimNodes)
	{
		LDelete_Array(GetAlloc(), m_AnimNodes, m_pModel->NumNodes());
		m_AnimNodes = LTNULL;
	}

	m_KeyFrames.Term(GetAlloc());
	m_DefaultRootNode.Term();
	DeleteRootNode();
}


void ModelAnim::SetRootNode(AnimNode *pNode)
{
	DeleteRootNode();

	m_pRootNode = pNode;
}


void ModelAnim::SetName(const char *pName)
{
	m_pName = m_pModel->AddString(pName);
}


void ModelAnim::SetModel(Model *pModel)
{
	m_pName = pModel->AddString(m_pName);
	m_pModel = pModel;

	// Resetup the AnimNode's ModelNode pointers.
	GetRootNode()->SetNodePointers(m_pModel->GetRootNode());
}


ModelAnim* ModelAnim::AllocateSameKind(Model *pModel)
{
	return new ModelAnim(pModel);
}


void ModelAnim::DeleteRootNode()
{
	if(m_pRootNode && m_pRootNode != &m_DefaultRootNode)
	{
		delete m_pRootNode;
	}

	m_pRootNode = &m_DefaultRootNode;
}


LTBOOL ModelAnim::PrecalcNodeLists(LTBOOL bRebuild)
{
	return BuildAnimNodeList(bRebuild);
}


LTBOOL ModelAnim::BuildAnimNodeList(LTBOOL bRebuild)
{
	uint32 curNodeIndex;

	if(!bRebuild && m_AnimNodes)
		return true;

	if(m_AnimNodes)
	{
		LDelete_Array(GetAlloc(), m_AnimNodes, GetModel()->NumNodes());
	}

	m_AnimNodes = LNew_Array(GetAlloc(), AnimNode*, m_pModel->NumNodes(), false);
	if(!m_AnimNodes)
		return false;

	curNodeIndex = 0;
	return GetRootNode()->FillAnimNodeList(curNodeIndex);
}


ModelAnim* ModelAnim::CopyKeyFrame(uint32 iKeyFrame)
{
	ModelAnim *pRet;

	pRet = LTNULL;

	if(iKeyFrame >= NumKeyFrames())
	{
		ASSERT(false);
		goto Error;
	}

	pRet = AllocateSameKind(m_pModel);
	if(!pRet)
		goto Error;

	// Copy our keyframe.
	if(!pRet->m_KeyFrames.Append(m_KeyFrames[iKeyFrame]))
	{
		goto Error;
	}

	// Recursively add keyframes.
	pRet->SetRootNode(GetRootNode()->AllocateSameKind(pRet, LTNULL)); // So it's the same class..
	if(!GetRootNode()->CopyKeyFrame(iKeyFrame, pRet->GetRootNode()))
		goto Error;

	return pRet;

// Errors go here.
Error:;

	if(pRet)
		delete pRet;

	return LTNULL;
}


LTBOOL ModelAnim::PasteKeyFrame(ModelAnim *pFrame, uint32 iBefore)
{
	if(!pFrame || pFrame->NumKeyFrames() == 0)
		return false;

	if(!GetRootNode()->PasteKeyFrame(pFrame->GetRootNode(), iBefore))
		return false;

	// NOTE: it MUST do this last so the AnimNodes have a correct keyframe count
	// while they do their paste.
	if(!m_KeyFrames.Insert(iBefore, pFrame->m_KeyFrames[0]))
		return false;
	
	return true;
}


ModelAnim* ModelAnim::CutKeyFrame(uint32 iKeyFrame)
{
	ModelAnim *pRet;

	pRet = CopyKeyFrame(iKeyFrame);
	if(pRet)
	{
		if(RemoveKeyFrame(iKeyFrame))
		{
			return pRet;
		}
		else
		{
			delete pRet;
		}
	}

	return false;
}


// talon: 2001's (0x42a213), not in Build 69; the name is ours. ModelEditDlg
// calls it after pasting a copy of a keyframe (0x42084e). The same > checks
// as AnimNode's.
LTBOOL ModelAnim::InterpolateKeyFrame(uint32 iDest, uint32 iFrom, uint32 iTo, float t)
{
	AnimKeyFrame *pDest, *pFrom, *pTo;

	if(iDest > NumKeyFrames() || iFrom > NumKeyFrames() || iTo > NumKeyFrames() || iFrom > iTo)
		return FALSE;

	if(t < 0.0f || t > 1.0f)
		return FALSE;

	pDest = &m_KeyFrames[iDest];
	pFrom = &m_KeyFrames[iFrom];
	pTo = &m_KeyFrames[iTo];
	if(!pDest || !pFrom || !pTo)
		return FALSE;

	pDest->m_Time = pFrom->m_Time + (uint32)((float)(pTo->m_Time - pFrom->m_Time) * t + 0.5f);

	if(!GetRootNode()->InterpolateKeyFrame(iDest, iFrom, iTo, t))
		return FALSE;

	return TRUE;
}


LTBOOL ModelAnim::RemoveKeyFrame(uint32 iKeyFrame)
{
	uint32 i, nShortenedBy;

	// there must be at least two keyframes in an animation
	if(m_KeyFrames.GetSize() <= 2)
	{
		return false;
	}

	// if it's the first or last frame, adjust the animation time
	if(iKeyFrame == 0)
	{
		nShortenedBy = m_KeyFrames[1].m_Time;
		for(i = 1; i < m_KeyFrames; i++)
		{
			m_KeyFrames[i].m_Time -= nShortenedBy;
		}
	}
	
	// now remove the actual translation and rotation keyframe from each model node
	if(!GetRootNode()->RemoveKeyFrame(iKeyFrame))
		return false;

	// Remove it from our list of keyframes.
	// This MUST come last so the AnimNodes have a correct keyframe count while
	// they remove their keyframes.
	m_KeyFrames.Remove(iKeyFrame);

	return true;
}

uint32 ModelAnim::GetAnimTime()
{
	if(m_KeyFrames.GetSize() > 0)
		return m_KeyFrames.Last().m_Time;
	else
		return 0;
}


// ------------------------------------------------------------------------ //
// AnimInfo.
// ------------------------------------------------------------------------ //

AnimInfo::AnimInfo()
{
	m_pAnim = LTNULL;
	m_pChildInfo = LTNULL;
	
	// Init to a wierd number.. animators should always set it.
	m_vDims.Init(128.0f, 16.0f, 16.0f);
	m_vTranslation.Init();
}
		
		
Model* AnimInfo::GetAnimOwner()
{
	if(m_pChildInfo->m_pModel == m_pAnim->GetModel())
	{
		return m_pChildInfo->m_pModel;
	}
	else
	{
		ASSERT(false);
		return LTNULL;
	}
}



// ------------------------------------------------------------------------ //
// ModelNode.
// ------------------------------------------------------------------------ //

ModelNode::ModelNode()
{
	Clear();
	m_pModel = LTNULL;
	
}

ModelNode::ModelNode(Model *pModel)
{
	Clear();
	m_pModel = pModel;
}

ModelNode::~ModelNode()
{
	Term();
}


void ModelNode::Term()
{
	DeleteAndClearArray(m_Children, GetAlloc());
	Clear();
}


void ModelNode::Clear()
{
	m_pName = "";
	m_NodeIndex = 0;
	m_Flags = 0;
	m_vOffsetFromParent.Init();
	m_mGlobalTransform.Identity();
	m_mInvGlobalTransform.Identity();
}


void ModelNode::SetName(char *pName)
{
	m_pName = m_pModel->AddString(pName);
}


ModelNode* ModelNode::AllocateSameKind(Model *pModel)
{
	return new ModelNode(pModel);
}


void ModelNode::SetModel(Model *pModel)
{
	uint32 i;


	m_pModel = pModel;
	m_pName = m_pModel->AddString(m_pName);

	for(i=0; i < m_Children; i++)
	{
		m_Children[i]->SetModel(pModel);
	}
}


uint32 ModelNode::CalcNumNodes()
{
	int total;
	unsigned long i;

	total = 1;
	for(i=0; i < NumChildren(); i++)
	{
		total += m_Children[i]->CalcNumNodes();
	}

	return total;
}


LTBOOL ModelNode::FillNodeList(uint32 &curNodeIndex)
{
	unsigned long i;

	if(curNodeIndex >= m_pModel->m_FlatNodeList.GetSize())
		return false;

	m_NodeIndex = (unsigned short)curNodeIndex;
	m_pModel->m_FlatNodeList[curNodeIndex] = this;
	++curNodeIndex;

	for(i=0; i < NumChildren(); i++)
	{
		if(!m_Children[i]->FillNodeList(curNodeIndex))
			return false;
	}

	return true;
}


void ModelNode::SetParent_R(uint32 iParent)
{
	uint32 i;

	m_iParentNode = iParent;
	
	for(i=0; i < NumChildren(); i++)
	{
		GetChild(i)->SetParent_R(m_NodeIndex);
	}
}


// ------------------------------------------------------------------------ //
// PieceLOD.
// ------------------------------------------------------------------------ //

PieceLOD::PieceLOD(Model *pModel)
{					 
	Init(pModel);
}


PieceLOD::PieceLOD()
{					 
	Init(LTNULL);
}


void PieceLOD::Init(Model *pModel)
{
	m_pModel= pModel;
}

// talon: 2001's (0x42a694) has no NULL-model case.
PieceLOD::~PieceLOD()
{	
	m_Tris.Term(GetAlloc());
	m_Verts.Term(GetAlloc());
}

// ------------------------------------------------------------------------ //
// ModelPiece.
// ------------------------------------------------------------------------ //

// talon: 2001's (0x42a712): the piece is its own base LOD.
ModelPiece::ModelPiece(Model *pModel) : PieceLOD(pModel)
{
	m_Name[0] = 0;
	m_iTexture = 0;
	m_pModel = pModel;
	m_SpecularPower = 5.0f;
	m_SpecularScale = 1.0f;
	m_LODWeight = 1.0f;
}


ModelPiece::~ModelPiece()
{
	Term();
}


void ModelPiece::Term()
{
	m_LODs.Term(GetAlloc());
}


void ModelPiece::SetName(const char *pName)
{
	SAFE_STRCPY(m_Name, pName);
}


// talon: 2001's (0x42a7e4) copies; Build 69 had it #if(0)'d out.
LTBOOL ModelPiece::CopyUVs(ModelPiece *pSrc)
{
	uint32 i, j;

	// Make sure the nodes match.
	if(pSrc->m_Tris.GetSize() != m_Tris.GetSize())
	{
		return false;
	}

	for(i=0; i < m_Tris; i++)
	{
		for(j=0; j < 3; j++)
		{
			m_Tris[i].m_UVs[j] = pSrc->m_Tris[i].m_UVs[j];
		}
	}

	return true;
}


// talon: ModelPiece's in 2001 (its m_pModel, 0x42a86f).
LTBOOL ModelPiece::ReplaceVertex(uint32 iVert, uint32 iReplaceWith)
{
	if(iVert >= m_Verts.GetSize() || iReplaceWith >= m_Verts.GetSize())
		return false;

	_ChangeVertexReferences(iVert, iReplaceWith, this);
	
	// (in case this is in an exporter, we have to update the EnvelopeInfo vertex refs).
	_ChangeVertexWeightReferences(GetModel(), this, iVert, iReplaceWith);
	
	m_Verts.Remove(iVert);
	return true;
}

// ------------------------------------------------------------------------ //
// WeightSet.
// ------------------------------------------------------------------------ //

WeightSet::WeightSet(Model *pModel)
{
	m_Name[0] = 0;
	m_pModel = pModel;
}

WeightSet::~WeightSet()
{
	m_Weights.Term(GetAlloc());
}

void WeightSet::SetName(const char *pName)
{
	SAFE_STRCPY(m_Name, pName);
}

LTBOOL WeightSet::InitWeights(uint32 nWeights)
{
	return m_Weights.SetSizeInit2(nWeights, 0.0f);
}


// ------------------------------------------------------------------------ //
// ModelSocket
// ------------------------------------------------------------------------ //

ModelSocket::ModelSocket()
{
	m_Name[0] = 0;
	m_iNode = 0;
	m_Pos.Init();
	m_Rot.Init();
	m_pAttachment = LTNULL;
}

void ModelSocket::SetName(const char *pName)
{
	SAFE_STRCPY(m_Name, pName);
}




// ------------------------------------------------------------------------ //
// Model functions.
// ------------------------------------------------------------------------ //

// talon: the 2001 constructor (0x42a9fd): the command-string defaults,
// the fade range and the model LODs; no movement node or LTB fields.
Model::Model(LAlloc *pAlloc, LAlloc *pDefAlloc) :
	m_StringList(pAlloc)
{
	m_pAlloc = pAlloc;
	m_pDefAlloc = pDefAlloc;

	m_GlobalRadius = 96.0f;
	m_Link.m_pData = this;

	m_pFilename = g_pNoModelFilename;

	m_nTotalVerts = 0;
	m_Flags = 0;
	m_RefCount=0;
	m_FlatNodeList = LTNULL;

	m_nDWordNodes = 0;
	
	m_VisRadius = DEFAULT_MODEL_VIS_RADIUS;
	m_bNoAnimation = FALSE;
	SetFadeRange(DEFAULT_MODEL_FADE_RANGE, DEFAULT_MODEL_FADE_RANGE);
	m_Unknown0c0 = 0;
	m_FadeSpriteSizeX = m_FadeSpriteSizeY = 100.0f;
	m_AmbientLight = DEFAULT_MODEL_AMBIENT_LIGHT;
	m_DirLight = DEFAULT_MODEL_DIR_LIGHT;
	m_bFOVOffset = FALSE;
	m_bSpecularEnable = FALSE;
	
	m_bShadowEnable = FALSE;
	m_fShadowProjectLength = 200.0f;
	m_fShadowLightDist = 200.0f;
	m_fShadowSizeX = 50.0f;
	m_fShadowSizeY = 50.0f;
	m_vShadowCenterOffset.Init();

	m_iNormalRefNode = INVALID_MODEL_NODE;
	m_iNormalRefAnim = INVALID_MODEL_ANIM;
	m_bRigid = FALSE;
	m_bNormalRef = FALSE;

	m_Transforms = LTNULL;
	m_CommandString = "";

	m_pRootNode = &m_DefaultRootNode;
	m_pRootNode->SetModel(this);

	m_nChildModels = 1;
	m_ChildModels[0] = &m_SelfChildModel;
	GetSelfChildModel()->m_pModel = GetSelfChildModel()->m_pParentModel = this;

	SetSaveIndex((uint32)rand());

	m_Unknown190 = 0;

	// Memory tracking
	g_ModelMemory += sizeof(Model);
}


Model::~Model()
{
	ASSERT(m_RefCount == 0);
	Term(true);

	// Memory tracking
	g_ModelMemory -= sizeof(Model);
	g_ModelMemory -= m_BlockAlloc.GetBlockSize();
}
						  

void Model::Term(LTBOOL bDeleteChildModels)
{
	m_Callbacks.Term();

	if(m_pRootNode != &m_DefaultRootNode)
	{
		LDelete(GetAlloc(), m_pRootNode);
	}
	m_DefaultRootNode.Term();
	m_pRootNode = &m_DefaultRootNode;
	DeleteAndClearArray2(m_Pieces, GetAlloc());

	TermAnims();
	TermChildModels(bDeleteChildModels);
	DeleteAndClearArray2(m_Sockets, GetAlloc());
	DeleteAndClearArray2(m_WeightSets, GetAlloc());
	m_Transforms.Term(GetAlloc());

	m_VertexWeights.Term(GetAlloc());

	m_FlatNodeList.Term(GetAlloc());

	// dfree all the strings.
	m_StringList.Term();

	FreeFilename();

	m_LODs.Term(GetAlloc());

	m_nTotalVerts = 0;
}


void Model::TermAnims()
{
	uint32 i;
	ModelAnim *pAnim;

	// Delete all the anims we own.
	for(i=0; i < m_Anims; i++)
	{
		pAnim = GetAnim(i);

		if(pAnim->GetModel() == this)
		{
			LDelete(GetAlloc(), pAnim);
		}
	}

	m_Anims.Term(GetDefAlloc());
}


void Model::TermChildModels(LTBOOL bDeleteChildModels)
{
	uint32 i;
	ChildInfo *pChildModel;

	for(i=0; i < NumChildModels(); i++)
	{
		pChildModel = m_ChildModels[i];

		if(!pChildModel)
			continue;

		// Possibly free the model itself.
		if(pChildModel != GetSelfChildModel())
		{
			if(pChildModel->m_pModel && pChildModel->m_pModel != this)
			{
				// talon: 2001 counts the references (0x42af66).
				if(pChildModel->m_pModel->m_RefCount > 0)
					pChildModel->m_pModel->m_RefCount--;

				if(bDeleteChildModels && pChildModel->m_pModel->IsFreeable())
					delete pChildModel->m_pModel;
			}
		}

		pChildModel->Term();

		// Free the ChildModel object.
		if(pChildModel != GetSelfChildModel())
		{
			if(pChildModel != &m_SelfChildModel)
			{
				LDelete(GetAlloc(), pChildModel);
			}
		}
	}

	m_nChildModels = 1;
	m_ChildModels[0] = &m_SelfChildModel;
}

void Model::SetRootNode(ModelNode *pRoot)
{
	// Delete the old one.
	if(m_pRootNode && m_pRootNode != &m_DefaultRootNode)
		delete m_pRootNode;

	m_pRootNode = pRoot;
}


LTBOOL Model::AddWeightSet(WeightSet *pSet)
{
	return m_WeightSets.Append(pSet);
}


void Model::RemoveWeightSet(uint32 index)
{
	if(index >= NumWeightSets())
	{
		ASSERT(false);
		return;
	}

	delete m_WeightSets[index];
	m_WeightSets.Remove(index);
}

// ------------------------------------------------------------------------
// PrecalcNodeLists( )
// returns false on fure anywhere in list below
// 1. resizes m_Transforms ; 
// 2. builds the flat node list
// 3. sets an offset value for every piece.
// 4. calls PrecalcNodeListys on all animation nodes.
// 5. finds the number of verts and num tris the model contains.
// ------------------------------------------------------------------------
LTBOOL Model::PrecalcNodeLists(LTBOOL bRebuild)
{
	uint32 i, curOffset;
	ModelAnim *pAnim;

	
	// BuildTransformList must be called first.
	if(!BuildTransformList(bRebuild) || !BuildFlatNodeList(bRebuild))
	{
		return false;
	}

	// Precalculate ModelNode::m_VertOffset.
	curOffset = 0;
	for(i=0; i < NumPieces(); i++)
	{
		GetPiece(i)->m_VertOffset = curOffset;
		curOffset += GetPiece(i)->m_Verts.GetSize();
	}

	for(i=0; i < m_Anims; i++)
	{
		pAnim = GetAnim(i);

		if(pAnim->GetModel() == this)
		{
			if(!pAnim->PrecalcNodeLists(bRebuild))
				return false;
		}
	}

	m_nTotalVerts = CalcNumVerts();
	m_nTotalTris = CalcNumTris();
	return true;
}


LTBOOL Model::BuildTransformList(LTBOOL bRebuild)
{
	uint32 nNodes;

	if(!bRebuild && m_Transforms.GetSize() == GetRootNode()->CalcNumNodes())
		return true;

	m_Transforms.Term();


	nNodes = GetRootNode()->CalcNumNodes();
	if (!m_Transforms.SetSize2(nNodes, GetAlloc()))
		return false;
	
	return true;
}


LTBOOL Model::BuildFlatNodeList(LTBOOL bRebuild)
{
	uint32 nNodes, curNodeIndex;


	if(!bRebuild && m_FlatNodeList.GetSize() == NumNodes())
		return true;
			
	m_FlatNodeList.Term(GetAlloc());

	// Figure out how many there are.
	nNodes = NumNodes();
	if(!m_FlatNodeList.SetSizeInit4(nNodes, LTNULL, GetAlloc()))
		return false;

	// Fill in the list.
	curNodeIndex = 0;
	if(!GetRootNode()->FillNodeList(curNodeIndex))
		return false;

	// Precalculate m_nDWordNodes;
	m_nDWordNodes = nNodes >> 2;
	if(nNodes % 4 != 0)
		++m_nDWordNodes;
	
	GetRootNode()->SetParent_R(NODEPARENT_NONE);
	return true;
}

ModelPiece* Model::FindPiece(const char *pName, uint32 *index)
{
	uint32 i;
	ModelPiece *pPiece;

	for(i=0; i < NumPieces(); i++)
	{
		pPiece = GetPiece(i);

		if(stricmp(pName, pPiece->GetName()) == 0)
		{
			if(index)
				*index = i;

			return pPiece;
		}
	}

	return LTNULL;
}


WeightSet* Model::FindWeightSet(const char *pName, uint32 *index)
{
	uint32 i;
	WeightSet *pSet;

	for(i=0; i < NumWeightSets(); i++)
	{
		pSet = GetWeightSet(i);

		if(stricmp(pName, pSet->GetName()) == 0)
		{
			if(index)
				*index = i;
			
			return pSet;
		}
	}

	return LTNULL;
}


LTBOOL Model::SwapPieces(uint32 iPiece1, uint32 iPiece2)
{
	ModelPiece *pTemp;

	// Validate..
	if(iPiece1 >= NumPieces() || iPiece2 >= NumPieces())
		return false;

	// Swap them.
	pTemp = m_Pieces[iPiece1];
	m_Pieces[iPiece1] = m_Pieces[iPiece2];
	m_Pieces[iPiece2] = pTemp;

	// TODO :
	// alert anims that a change in organization has occured...
	return true;
}



char* Model::AddString(const char *pString)
{
	return m_StringList.AddString(pString);
}


ModelNode* Model::FindNode(const char *pName, uint32 *index)
{
	uint32 i;

	for(i=0; i < NumNodes(); i++)
	{
		if(stricmp(m_FlatNodeList[i]->GetName(), pName) == 0)
		{
			if(index)
				*index = i;
			
			return m_FlatNodeList[i];
		}
	}

	return LTNULL;
}


ModelNode* Model::FindParent(ModelNode *pNode, ModelNode *pRoot, uint32 *index)
{
	uint32 i;
	ModelNode *pTest;

	for(i=0; i < pRoot->NumChildren(); i++)
	{
		if(pRoot->m_Children[i] == pNode)
		{
			if(index)
				*index = i;

			return pRoot;
		}
	
		pTest = FindParent(pNode, pRoot->m_Children[i], index);
		if(pTest)
			return pTest;
	}					 
	
	return LTNULL;
}


void Model::UnrotateAnimNode(ModelAnim *pAnim, AnimNode *pAnimNode)
{
	uint32 j;
	LTVector pos, *pUp, *pRight, *pForward;
	LTMatrix mat;
	NodeKeyFrame *pKeyframe;

	for(j=0; j < pAnim->m_KeyFrames; j++)
	{
		pKeyframe = &pAnimNode->m_KeyFrames[j];
		pKeyframe->m_Quaternion.ConvertToMatrix(mat);
		
		// Adjust the translation to compensate..
		pRight = (LTVector*)&mat.m[0][0];
		pUp = (LTVector*)&mat.m[1][0];
		pForward = (LTVector*)&mat.m[2][0];
		pos.Init();
		pos += *pRight * pKeyframe->m_Translation.x;
		pos += *pUp * pKeyframe->m_Translation.y;
		pos += *pForward * pKeyframe->m_Translation.z;
		
		// Apply this matrix to all the other nodes on this keyframe.
		ApplyMatrixToKeyframes(pAnimNode, mat, j);

		// Reset the root node's rotation.
		//THIS SEEMS TO MESS UP THE TRANSLATIONS..??! VEC_COPY(pKeyframe->m_Translation, pos);
		pKeyframe->m_Quaternion[0] = pKeyframe->m_Quaternion[1] = pKeyframe->m_Quaternion[2] = 0.0f;
		pKeyframe->m_Quaternion[3] = 1.0f;
	}
}


void Model::UnrotateNode(ModelNode *pNode)
{
	uint32 i;

	for(i=0; i < m_Anims; i++)
	{
		ModelAnim *pAnim = GetAnimIfOwner(i);
		if(pAnim)
		{
			UnrotateAnimNode(pAnim, pAnim->m_AnimNodes[pNode->GetNodeIndex()]);
		}
	}
}


LTBOOL Model::RemoveNode(ModelNode *pNode)
{
	ModelAnim *pAnim;
	uint32 i, j, k, nodeIndex, oldTransformIndex;
	AnimNode *pAnimNode;
	ModelNode *pParent;
	AnimNode *pParentAnimNode, *pChild;
	CMoArray<TransformMap> tMap;
	TransformMap *pMap;
	LTMatrix parentTransform;
	ModelPiece *pPiece;
	ModelVert *pVert;
	NewVertexWeight *pWeight;

	
	// Must be a null node and can't be the root node.
	if(!pNode || 
		pNode == GetRootNode() || 
		!(pNode->m_Flags & MNODE_REMOVABLE) ||
		AnyVertsWeightedToNode(pNode->m_NodeIndex))
	{
		return false;
	}

	oldTransformIndex = pNode->GetNodeIndex();

	pParent = FindParent(pNode, GetRootNode(), &nodeIndex);
	if(!pParent)
		return false;

	Mat_Identity(&parentTransform);


	// Go Local->Global (it converts back to local below).
	// This effectively preserves the transforms for all the nodes.
	for(i=0; i < m_Anims; i++)
	{
		pAnim = GetAnimIfOwner(i);
		if(pAnim)
		{
			for(j=0; j < pAnim->NumKeyFrames(); j++)
			{
				_TransformsLtoG(pAnim->GetRootNode(), j, &parentTransform);
			}
		}
	}

	
	// Backup the old transform indices of the nodes.
	for(i=0; i < NumNodes(); i++)
	{
		m_FlatNodeList[i]->ScratchData() = (uint8)i; // Give it its identifier.
		tMap.Append(TransformMap(m_FlatNodeList[i], m_FlatNodeList[i]->GetNodeIndex()));
	}


	// Update the AnimNode trees.
	for(i=0; i < m_Anims; i++)
	{
		pAnim = GetAnimIfOwner(i);
		if(pAnim)
		{
			pAnimNode = pAnim->m_AnimNodes[pNode->GetNodeIndex()];
			pParentAnimNode = pAnim->m_AnimNodes[pParent->GetNodeIndex()];

			// Redo the children's parent pointers and add the node's children to its parent.
			for(j=0; j < pNode->NumChildren(); j++)
			{
				pChild = pAnimNode->m_Children[j];

				pParentAnimNode->m_Children.Append(pChild);
				pChild->m_pParentNode = pParentAnimNode;
			}

			// Clear its list (but don't actually delete them since they're in the parent now).
			pAnimNode->m_Children.Term();

			// Delete the node and remove it from its parent's list.
			pParentAnimNode->m_Children.Remove(pParentAnimNode->m_Children.FindElement(pAnimNode));
			delete pAnimNode;
		}
	}

	// Add its children to its parent.
	for(i=0; i < pNode->NumChildren(); i++)
	{
		pParent->m_Children.Append(pNode->m_Children[i]);
	}

	// Clear its list (but don't actually delete them since they're in the parent now).
	pNode->m_Children.Term();

	// Remove the node itself from its parent.
	pParent->m_Children.Remove(pParent->m_Children.FindElement(pNode));
	delete pNode;


	// Redo the node lists.
	PrecalcNodeLists();


	// Convert the transforms back to local space for the nodes.
	for(i=0; i < m_Anims; i++)
	{
		pAnim = GetAnimIfOwner(i);
		if(pAnim)
		{
			for(j=0; j < pAnim->NumKeyFrames(); j++)
			{
				_TransformsGtoL(pAnim->GetRootNode(), j, &parentTransform);
			}
		}
	}


	// Finish the transform map.
	for(i=0; i < NumNodes(); i++)
	{
		pMap = FindTransformMapByNode(tMap, m_FlatNodeList[i]);
		if(pMap)
		{
			pMap->m_NewTransform = m_FlatNodeList[i]->GetNodeIndex();
		}
		else
		{
			// Since we just removed a node, it should always find the nodes.
			ASSERT(false);
		}
	}

	
	// Now remap all the vert weight indices.
	for(i=0; i < NumPieces(); i++)
	{
		pPiece = GetPiece(i);
		for(j=0; j < pPiece->m_Verts; j++)
		{
			pVert = &pPiece->m_Verts[j];

			for(k=0; k < pVert->m_nWeights; k++)
			{
				pWeight = &pVert->m_Weights[k];
			
				pWeight->m_iNode = tMap[pWeight->m_iNode].m_NewTransform;
			}
		}
	}

	// Rebuild our relation to ourself.
	if(!SetupChildModelRelation(GetSelfChildModel(), this, false))
		return false;
	
	return true;
}


LTBOOL Model::RemoveRemovableNodes()
{
	uint32 i;
	ModelNode *pNode;
	bool bRet;


	bRet = true;
	for(i=0; i < NumNodes(); i++)
	{
		pNode = GetNode(i);

		if(pNode->m_Flags & MNODE_REMOVABLE)
		{
			if(RemoveNode(pNode))
			{
				// Start iteration over..
				i = (uint32)-1;
			}
			else
			{
				bRet = false;
			}
		}
	}

	return bRet;
}


AnimInfo* Model::AddAnim(ModelAnim *pAnim, ChildInfo *pInfo)
{
	AnimInfo info, *pNewAnim;
	uint32 offset;

	info.m_pAnim = pAnim;
	info.m_pChildInfo = pInfo;

	if(pInfo->m_pModel)
		offset = pInfo->m_AnimOffset + pInfo->m_pModel->CalcNumParentAnims();
	else
		offset = pInfo->m_AnimOffset;

	if(m_Anims.Insert(offset, info))
	{
		pNewAnim = &m_Anims[offset];
		ASSERT(pNewAnim->m_pAnim == pAnim);
		return pNewAnim;
	}
	else
	{
		return LTNULL;
	}
}


LTBOOL Model::AddDummyAnimation(char *pName)
{
	ModelAnim *pAnim;
	AnimKeyFrame frame;
	AnimInfo *pInfo;


	pAnim = new ModelAnim(this);
	if(!pAnim)
		return false;

	pInfo = AddAnim(pAnim, &m_SelfChildModel);
	if(!pInfo)
	{
		delete pAnim;
		return false;
	}

	pAnim->SetName(pName);
	
	frame.m_Time = 1;
	pAnim->m_KeyFrames.Append(frame);

	pAnim->SetRootNode(new AnimNode(pAnim, LTNULL));
	if(!CreateDummyAnimNodes(this, pAnim, GetRootNode(), pAnim->GetRootNode(), LTNULL))
		return false;

	// Update other data.
	pAnim->PrecalcNodeLists();

	return true;
}


ModelAnim* Model::FindAnim(char *pName, uint32 *index, AnimInfo **ppInfo)
{
	uint32 i;
	ModelAnim *pAnim;

	for(i=0; i < m_Anims; i++)
	{
		pAnim = GetAnim(i);

		if(stricmp(pAnim->m_pName, pName) == 0)
		{
			if(index)
				*index = i;
			
			if(ppInfo)
				*ppInfo = &m_Anims[i];

			return pAnim;
		}
	}

	return LTNULL;
}


AnimInfo* Model::FindAnimInfo(char *pAnimName, Model *pOwner, uint32 *index)
{
	uint32 i;
	AnimInfo *pInfo;

	
	for(i=0; i < m_Anims; i++)
	{
		pInfo = &m_Anims[i];
	
		if(pInfo->m_pAnim->GetModel() == pOwner)
		{
			if(stricmp(pInfo->m_pAnim->GetName(), pAnimName) == 0)
			{
				if(index)
					*index = i;

				return pInfo;
			}
		}
	}

	return LTNULL;
}


LTBOOL Model::GetAnimInfoIndex(AnimInfo *pInfo, uint32 &index)
{
	uint32 i;

	for(i=0; i < m_Anims; i++)
	{
		if(&m_Anims[i] == pInfo)
		{
			index = i;
			return true;
		}
	}

	return false;
}


// talon: ForceAnimOrder is model_save's in 2001 (0x432a9a).


LTBOOL Model::CopyUVs(Model *pSrc)
{
	uint32 i;

	if(NumPieces() != pSrc->NumPieces())
		return false;

	// Go thru all the nodes and copy UVs.
	for(i=0; i < NumPieces(); i++)
	{
		if(!GetPiece(i)->CopyUVs(pSrc->GetPiece(i)))
			return false;
	}
	
	return true;
}


inline void _MatVMul_Deform(LTVector *pDest, LTMatrix *pMat, LTVector *pSrc)
{
	// Making this assumption makes the matrix multiply faster.
	ASSERT(fabs(pMat->m[3][0]) < 0.0001 && fabs(pMat->m[3][1]) < 0.0001 && 
		fabs(pMat->m[3][2]) < 0.0001 && fabs(1.0 - pMat->m[3][3]) < 0.0001);
	
	pDest->x = (pMat->m[0][0]*pSrc->x + pMat->m[0][1]*pSrc->y + pMat->m[0][2]*pSrc->z + pMat->m[0][3]);
	pDest->y = (pMat->m[1][0]*pSrc->x + pMat->m[1][1]*pSrc->y + pMat->m[1][2]*pSrc->z + pMat->m[1][3]);
	pDest->z = (pMat->m[2][0]*pSrc->x + pMat->m[2][1]*pSrc->y + pMat->m[2][2]*pSrc->z + pMat->m[2][3]);
}


LTBOOL Model::MakeUniqueAnimName(char *pBase, char *pName)
{
	uint32 i;

	if(!FindAnim(pBase))
	{
		strcpy(pName, pBase);
		return true;
	}

	for(i=0; i < 32768; i++)
	{
		sprintf(pName, "%s%d", pBase, i);
		if(!FindAnim(pName))
		{
			return true;
		}
	}

	return false;
}


void Model::SetNodeParentOffsets()
{
	uint32 i;
	ModelNode *pNode, *pParent;
	LTMatrix mLocal;


	for(i=0; i < NumNodes(); i++)
	{
		pNode = GetNode(i);

		if(pNode->m_iParentNode < NumNodes())
		{
			pParent = GetNode(pNode->m_iParentNode);
			mLocal = pParent->GetGlobalTransform().MakeInverseTransform() * pNode->GetGlobalTransform();
			mLocal.GetTranslation(pNode->m_vOffsetFromParent);
		}
		else
		{
			pNode->m_vOffsetFromParent.Init();
		}
	}
}

// ------------------------------------------------------------------------
// BOOL GetVertexPositions( GVPStruct , bool setup-transforms, weight )
//
// This is the 'reference' function for how to get the vertex positions on a given 
// frame of animation.
// ------------------------------------------------------------------------	
LTBOOL Model::GetVertexPositions(GVPStruct *pStruct, LTBOOL bSetupTransforms, LTBOOL bWeight)
{
	uint32 i, j, k;
	ModelPiece *pPiece;
	uint8 *pCurOut;
	TransformMaker tMaker;
	float vTemp[4];
	ModelVert *pVert;
	LTMatrix *pTransforms;
	NewVertexWeight *pWeight;
	PieceLOD *pLOD;

	// Validate..
	if(!pStruct->m_Vertices)
		return false;

	if(pStruct->m_nAnims > MAX_GVP_ANIMS)
		return false;

	if(bSetupTransforms)
	{
		tMaker.SetupFromGVPStruct(pStruct);
		if(!tMaker.SetupTransforms())
			return false;
	}

	pTransforms = m_Transforms.GetArray();
	
	// Copy the vertex positions over.
	pCurOut = (uint8*)pStruct->m_Vertices;
	for(i=0; i < NumPieces(); i++)
	{
		pPiece = GetPiece(i);
		pLOD = pPiece->GetLOD(pStruct->m_iLOD);

		for(j=0; j < pLOD->m_Verts; j++)
		{
			pVert = &pLOD->m_Verts[j];

			// Apply all the weights.
			if(pVert->m_nWeights > 0)
			{
				vTemp[0] = vTemp[1] = vTemp[2] = vTemp[3] = 0.0f;
				for(k=0; k < pVert->m_nWeights; k++)
				{
					pWeight = &pVert->m_Weights[k];
					ASSERT(pWeight->m_iNode < NumNodes());
					MatVMul_Add(vTemp, &pTransforms[pWeight->m_iNode], pWeight->m_Vec);
				}

				// Divide by w.
				vTemp[3] = 1.0f / vTemp[3];
				((LTVector*)pCurOut)->x = vTemp[0] * vTemp[3];
				((LTVector*)pCurOut)->y = vTemp[1] * vTemp[3];
				((LTVector*)pCurOut)->z = vTemp[2] * vTemp[3];
			}
			else
			{
				((LTVector*)pCurOut)->Init();
			}

			pCurOut += pStruct->m_VertexStride;
		}
	}

	return true;
}


uint32 Model::CalcNumTris(uint32 iLOD)
{
	uint32 i, total;
	PieceLOD *pLOD;

	total = 0;
	for(i=0; i < NumPieces(); i++)
	{
		pLOD = GetPiece(i)->GetLOD(iLOD);
		if(pLOD)
		{
			total += pLOD->m_Tris;
		}
		else
		{
			ASSERT(false);
		}
	}

	return total;
}

uint32 Model::CalcNumVerts()
{
	uint32 i, total;

	// talon: 2001's counts the base LODs only (0x42bbef).
	total = 0;
	for(i=0; i < NumPieces(); i++)
		total += GetPiece(i)->m_Verts.GetSize();

	return total;
}


uint32 Model::CalcNumVertexWeights()
{
	uint32 i, j, iLOD, total;
	ModelPiece *pPiece;
	PieceLOD *pLOD;
	
	total = 0;
	for(i=0; i < NumPieces(); i++)
	{
		pPiece = GetPiece(i);

		for(iLOD=0; iLOD < pPiece->NumLODs(); iLOD++)
		{
			pLOD = pPiece->GetLOD(iLOD);
			
			for(j=0; j < pLOD->m_Verts; j++)
			{
				total += pLOD->m_Verts[j].m_nWeights;
			}
		}
	}
	
	return total;
}


uint32 Model::CalcNumChildModelAnims(LTBOOL bIncludeSelf)
{
	uint32 i, total;
	ChildInfo *pChildModel;

	total = 0;
	for(i=0; i < NumChildModels(); i++)
	{
		pChildModel = GetChildModel(i);

		if(!pChildModel->m_pModel)
			continue;

		if(pChildModel == GetSelfChildModel() && !bIncludeSelf)
			continue;

		total += pChildModel->m_pModel->m_Anims.GetSize();
	}

	return total;
}


uint32 Model::CalcNumParentAnims()
{
	uint32 i, total;

	total = 0;
	for(i=0; i < m_Anims; i++)
	{
		if(GetAnim(i)->m_pModel == this)
			++total;
	}

	return total;
}


void Model::ParseCommandString()
{
	ConParse parse;
	TransformMaker tMaker;
	uint32 i;
	SimpleFloatVar floatVars[] =
	{
		"VisRadius", &m_VisRadius,
		"AmbientLight", &m_AmbientLight,
		"DirLight", &m_DirLight,
		"ShadowProjectLength", &m_fShadowProjectLength,
		"ShadowLightDist", &m_fShadowLightDist,
		"ShadowSizeX", &m_fShadowSizeX,
		"ShadowSizeY", &m_fShadowSizeY
	};


	
	if(!m_CommandString)
		return;

	m_bFOVOffset = FALSE;

	// talon: 2001's (0x42bceb) switches on the argument count, and has the
	// fade range and the fade sprite size.
	// Parse for level of detail settings...
	parse.Init(m_CommandString);
	while(parse.Parse())
	{
		switch(parse.m_nArgs)
		{
			case 1:
			{
				if(stricmp("NoAnimation", parse.m_Args[0]) == 0)
				{
					m_bNoAnimation = TRUE;
				}
				else if(stricmp("ShadowEnable", parse.m_Args[0]) == 0)
				{
					m_bShadowEnable = TRUE;
				}
				else if(stricmp("SpecularEnable", parse.m_Args[0]) == 0)
				{
					m_bSpecularEnable = TRUE;
				}
				else if(stricmp("Rigid", parse.m_Args[0]) == 0)
				{
					m_bRigid = TRUE;
				}
			}
			break;

			case 2:
			{
				// Read in the easy ones.
				for(i=0; i < (sizeof(floatVars) / sizeof(floatVars[0])); i++)
				{
					if(stricmp(floatVars[i].m_pName, parse.m_Args[0]) == 0)
						*floatVars[i].m_pVal = (float)atof(parse.m_Args[1]);
				}

				// Check for non-trivial ones..
				if(stricmp("FadeRangeMin", parse.m_Args[0]) == 0)
				{
					SetFadeRange((float)atof(parse.m_Args[1]), m_FadeRangeMax);
				}
				else if(stricmp("FadeRangeMax", parse.m_Args[0]) == 0)
				{
					SetFadeRange(m_FadeRangeMin, (float)atof(parse.m_Args[1]));
				}
				else if(stricmp("FovXOffset", parse.m_Args[0]) == 0)
				{
					m_xFOVOffset = MATH_DEGREES_TO_RADIANS((float)atof(parse.m_Args[1]));
					m_bFOVOffset = TRUE;
				}
				else if(stricmp("FovYOffset", parse.m_Args[0]) == 0)
				{
					m_yFOVOffset = MATH_DEGREES_TO_RADIANS((float)atof(parse.m_Args[1]));
					m_bFOVOffset = TRUE;
				}
			}
			break;

			case 3:
			{
				if(stricmp("FadeSpriteSize", parse.m_Args[0]) == 0)
				{
					m_FadeSpriteSizeX = (float)atof(parse.m_Args[1]);
					m_FadeSpriteSizeY = (float)atof(parse.m_Args[2]);
				}
				else if(stricmp("NormalRef", parse.m_Args[0]) == 0)
				{
					m_bNormalRef = FALSE;
					m_iNormalRefNode = INVALID_MODEL_NODE;
					if(FindNode(parse.m_Args[1], &m_iNormalRefNode))
					{
						m_iNormalRefAnim = INVALID_MODEL_ANIM;
						if(FindAnim(parse.m_Args[2], &m_iNormalRefAnim))
						{
							tMaker.m_Anims[0].Init(this, 
								m_iNormalRefAnim, 0, 
								m_iNormalRefAnim, 0,
								0.0f);
							tMaker.m_nAnims = 1;
													
							if(tMaker.SetupTransforms())
							{
								m_mNormalRef = m_Transforms[m_iNormalRefNode];
								m_bNormalRef = TRUE;
							}							
						}					
					}
				}
			}
			break;

			case 4:
			{
				if(stricmp("ShadowCenterOffset", parse.m_Args[0]) == 0)
				{
					m_vShadowCenterOffset.x = (float)atof(parse.m_Args[1]);
					m_vShadowCenterOffset.y = (float)atof(parse.m_Args[2]);
					m_vShadowCenterOffset.z = (float)atof(parse.m_Args[3]);
				}
			}
			break;
		}
	}
}

// talon: 2001's (0x42c0e2).
void Model::SetFadeRange(float fMin, float fMax)
{
	m_FadeRangeMin = fMin;
	m_FadeRangeMinSqr = fMin * fMin;
	m_FadeRangeMax = fMax;
	m_FadeRangeMaxSqr = fMax * fMax;
}


const char* Model::GetFilename()
{
	return m_pFilename;
}


LTBOOL Model::SetFilename(const char *pInFilename)
{
	char *pFilename;

	FreeFilename();

	pFilename = new char[strlen(pInFilename) + 1];
	if(!pFilename)
	{
		return false;
	}

	strcpy(pFilename, pInFilename);
	m_pFilename = pFilename;
	return true;
}


void Model::FreeFilename()
{
	if(m_pFilename != g_pNoModelFilename)
	{
		delete m_pFilename;
	}

	m_pFilename = g_pNoModelFilename;
}


LTBOOL Model::VerifyChildModelTree(Model *pChild, ModelNode* &pErrNode)
{
	return VerifyChildModel_R(GetRootNode(), pChild->GetRootNode(), pErrNode);
}


// 
// N = O * X
// N * ~O = X
// Global = Parent * Local so:
// Local = ~Parent * Global
void _SetupChildModelRelation_R(ChildInfo *pInfo, 
	LTMatrix *pParentT1, LTMatrix *pParentT2,
	ModelNode *pNode1, ModelNode *pNode2)
{
	LTMatrix inverse, local1, local2, mFinal;
	uint32 i;


	// Get the local transforms.
	Mat_InverseTransformation(pParentT1, &inverse);
	MatMul(&local1, &inverse, &pNode1->GetGlobalTransform());

	Mat_InverseTransformation(pParentT2, &inverse);
	MatMul(&local2, &inverse, &pNode2->GetGlobalTransform());

	// Setup the relation.
	Mat_InverseTransformation(&local2, &inverse);
	MatMul(&mFinal, &inverse, &local1);
	
	pInfo->m_Relation[pNode1->GetNodeIndex()].ConvertFromMatrix(mFinal);

	for(i=0; i < pNode1->NumChildren(); i++)
	{
		_SetupChildModelRelation_R(pInfo, 
			&pNode1->GetGlobalTransform(), &pNode2->GetGlobalTransform(),
			pNode1->GetChild(i), pNode2->GetChild(i));
	}
}


LTBOOL Model::SetupChildModelRelation(ChildInfo *pInfo, Model *pChild, LTBOOL bScaleSkeleton)
{
	LTMatrix ident;
	uint32 i;
	
	if(!pInfo->m_Relation.SetSize(NumNodes()))
		return false;

	Mat_Identity(&ident);
	if(bScaleSkeleton)
	{
		_SetupChildModelRelation_R(pInfo, &ident, &ident, GetRootNode(), pChild->GetRootNode());
	}
	else
	{
		for(i=0; i < pInfo->m_Relation; i++)
		{
			pInfo->m_Relation[i].ConvertFromMatrix(ident);
		}
	}

	return true;
}


ChildInfo* Model::AddChildModel(

				Model *pChild,         // ptr to loaded child model
				char *pFilename,	   // name of on-disk file.
				char *pErrStr,         // return error string to caller (must have space)
				LTBOOL bScaleSkeleton)
{
	ChildInfo *pChildModel;
	AnimInfo *pAnim;
	uint32 i;
	ModelNode *pErrNode;

	
	if(FindChildModel(pChild))
	{
		// ooooo you dirty rat %#$@#!
		ASSERT(false);
		sprintf(pErrStr, "Model already exists.");
		return LTNULL;
	}

	// Can't have too many.
	if(m_nChildModels >= MAX_CHILD_MODELS)
	{
		sprintf(pErrStr, "Child model limit reached.");
		return LTNULL;
	}

	// Make sure it's valid.
	if(pChild)
	{
		if(!VerifyChildModelTree(pChild, pErrNode))
		{
			sprintf(pErrStr, "Node trees differ at node %s.", pErrNode->GetName());
			return false;
		}
	}

	// Add it..
	pChildModel = new ChildInfo;
	if(!pChildModel)
		return LTNULL;

	if(!SetupChildModelRelation(pChildModel, pChild, bScaleSkeleton))
	{
		sprintf(pErrStr, "SetupChildModelRelation returned LTFALSE.");
		delete pChildModel;
		return LTNULL;
	}

	pChildModel->m_pFilename = AddString(pFilename);
	if(!pChildModel->m_pFilename)
	{
		delete pChildModel;
		return LTNULL;
	}

	// Add it to the list.
	m_ChildModels[m_nChildModels] = pChildModel;
	m_nChildModels++;

	// Set it up.
	pChildModel->m_pParentModel = this;
	pChildModel->m_pModel = pChild;
	pChildModel->m_AnimOffset = 0;

	if(pChild)
	{
		pChild->AddRef();

		pChildModel->m_AnimOffset = m_Anims.GetSize();
		if(!m_Anims.NiceSetSize(m_Anims.GetSize() + pChild->m_Anims.GetSize()))
		{
			sprintf(pErrStr, "Out of memory.");
			return LTNULL;
		}

		for(i=0; i < pChild->m_Anims; i++)
		{
			pAnim = GetAnimInfo(pChildModel->m_AnimOffset+i);
			*pAnim = pChild->m_Anims[i];
			pAnim->m_pChildInfo = pChildModel;
		}
	}

	return pChildModel;
}

// ------------------------------------------------------------------------
// Add ChildModel as References 
// ------------------------------------------------------------------------
ChildInfo* Model::AddChildModelRef( char *pFilename, 
								char *pErrStr 
							   )
							   	
{
	ChildInfo *pChildModel;
	uint32 index ;

	
	// Check if there's already a child by filenames.
	if(FindChildModelByFilename(pFilename, &index) != LTNULL)
	{
		ASSERT(false);
		sprintf(pErrStr, "Model already exists.");
		return LTNULL;
	}


	// Can't have too many.
	if(m_nChildModels >= MAX_CHILD_MODELS)
	{
		sprintf(pErrStr, "Child model limit reached.");
		return LTNULL;
	}

	// Make sure it's valid.
#if(0)
	// we can't there's no real data.
	if(pChild)
	{
		if(!VerifyChildModelTree(pChild, pErrNode))
		{
			sprintf(pErrStr, "Node trees differ at node %s.", pErrNode->GetName());
			return false;
		}
	}
#endif

	// Add it..
	pChildModel = new ChildInfo;
	if(!pChildModel)
		return LTNULL;

#if(0)
	// ?? 
	if(!SetupChildModelRelation(pChildModel, pChild, bScaleSkeleton))
	{
		sprintf(pErrStr, "SetupChildModelRelation returned LTFALSE.");
		delete pChildModel;
		return LTNULL;
	}
#endif

	pChildModel->m_pFilename = AddString(pFilename);
	if(!pChildModel->m_pFilename)
	{
		delete pChildModel;
		return LTNULL;
	}

	// Add it to the list.
	m_ChildModels[m_nChildModels] = pChildModel;
	m_nChildModels++;

	// Set it up.
	pChildModel->m_pParentModel = this;
	pChildModel->m_pModel = LTNULL; // danger here, the child model is technically broken.
	pChildModel->m_AnimOffset = 0;

#if(0)
	if(pChild)
	{
		pChild->AddRef();

		pChildModel->m_AnimOffset = m_Anims.GetSize();
		if(!m_Anims.NiceSetSize(m_Anims.GetSize() + pChild->m_Anims.GetSize()))
		{
			sprintf(pErrStr, "Out of memory.");
			return LTNULL;
		}

		for(i=0; i < pChild->m_Anims; i++)
		{
			pAnim = GetAnimInfo(pChildModel->m_AnimOffset+i);
			*pAnim = pChild->m_Anims[i];
			pAnim->m_pChildInfo = pChildModel;
		}
	}
#endif

	return pChildModel;
}

// ------------------------------------------------------------------------
// RemoveChildModel
// ------------------------------------------------------------------------
LTBOOL Model::RemoveChildModel(uint32 index)
{
	uint32 i;
	ChildInfo *pInfo;

	if(index >= m_nChildModels || index == 0)
		return false;

	pInfo = m_ChildModels[index];

	// Remove any animations referencing this child model.
	for(i=0; i < m_Anims; i++)
	{
		if(m_Anims[i].m_pChildInfo == pInfo)
		{
			m_Anims.Remove(i);
			--i;
		}
	}

	// Delete and remove it from the list.
	delete pInfo;
	
	// re arrange our static memory to accomodate the missing childmodel
	for( i = index ; i < m_nChildModels-1; i++ )
	{
		m_ChildModels[i] = m_ChildModels[i+1];	
	}

	// talon: 2001 doesn't clear the freed slots (0x42c4ce).
	m_nChildModels--;
	
	return true;
}


LTBOOL Model::InitChildInfo(uint32 index, ChildInfo *pChildModel, Model *pModel, char *pFilename)
{
	if(index >= MAX_CHILD_MODELS)
		return false;

	pChildModel->m_pFilename = pFilename;
	pChildModel->m_pParentModel = this;
	pChildModel->m_pModel = pModel;
	pChildModel->m_AnimOffset = 0;

	if(pModel)
	{
		if(pModel != this)
		{
			pModel->AddRef();
		}
	}

	m_ChildModels[index] = pChildModel;
	return true;
}


ChildInfo* Model::FindChildModel(Model *pModel)
{
	uint32 i;

	for(i=0; i < NumChildModels(); i++)
	{
		if(GetChildModel(i)->m_pModel == pModel)
			return GetChildModel(i);
	}
	
	return LTNULL;
}


ChildInfo* Model::FindChildModelByFilename(char *pFilename, uint32 *index)
{
	uint32 i;
	ChildInfo *pInfo;

	for(i=0; i < NumChildModels(); i++)
	{
		pInfo = GetChildModel(i);

		// talon: stricmp in 2001 (0x42c57f, the same call as FindPiece's).
		if(stricmp(pInfo->m_pFilename, pFilename) == 0)
		{
			if(index)
				*index = i;

			return pInfo;
		}
	}
	
	return LTNULL;
}


ModelSocket* Model::FindSocket(const char *pName, uint32 *index)
{
	uint32 i;
	ModelSocket *pSocket;


	for(i=0; i < NumSockets(); i++)
	{
		pSocket = GetSocket(i);

		if(stricmp(pSocket->GetName(), pName) == 0)
		{
			if(index)
				*index = i;

			return pSocket;
		}
	}

	return LTNULL;
}


LTBOOL Model::AddSocket(ModelSocket *pSocket)
{
	return m_Sockets.Append(pSocket);
}


LTBOOL Model::RemoveSocket(uint32 index)
{
	if(index >= m_Sockets.GetSize())
		return false;

	delete m_Sockets[index];
	m_Sockets.Remove(index);
	return true;
}


LTBOOL Model::GetSocketTransform(ModelSocket *pSocket, LTMatrix *pSocketTransform)
{
	LTMatrix offsetTransform, *pTransform;
	ModelNode *pNode;


	if(pSocket->m_iNode >= NumNodes())
		return false;

	pNode = GetNode(pSocket->m_iNode);
	if(!pNode)
		return false;

	pTransform = GetTransform(pNode->GetNodeIndex());
	pSocket->ConvertToMatrix(offsetTransform);

	MatMul(pSocketTransform, pTransform, &offsetTransform);
	return true;
}


// talon: 2001's looks at the base LOD only (0x42c6c5).
LTBOOL Model::AnyVertsWeightedToNode(uint32 iNode)
{
	uint32 iPiece, iVert, iWeight;
	ModelPiece *pPiece;
	ModelVert *pVert;

	for(iPiece=0; iPiece < NumPieces(); iPiece++)
	{
		pPiece = GetPiece(iPiece);
		for(iVert=0; iVert < pPiece->m_Verts; iVert++)
		{
			pVert = &pPiece->m_Verts[iVert];

			for(iWeight=0; iWeight < pVert->m_nWeights; iWeight++)
				if(pVert->m_Weights[iWeight].m_iNode == iNode)
					return true;
		}
	}

	return false;
}


