// ModelEditDlg.cpp : implementation file
//
// Changes:
//
// {BP 1/4/98} Switched to using DStream instead of MoFileIO

#include "precompile.h"
#include "modeledit.h"
#include "modeleditdlg.h"
#include "renamedlg.h"
#include "continuousdlg.h"
#include "renderwnd.h"
#include "keyframewnd.h"
#include "modelinfodlg.h"
#include "commandstringdlg.h"
#include "model_ops.h"
#include "model_cleanup.h"
#include "mmsystem.h"
#include "streamsim.h"
#include "dimensionsdlg.h"
#include "importanimation.h"
#include "animframeratedlg.h"
#include "animcleanupdlg.h"
#include "keyframetimedlg.h"
#include "translationdlg.h"
#include "uvimportdlg.h"
#include "animnumberdlg.h"
#include "renamenodedlg.h"
#include "rotationdlg.h"
#include "aboutdlg.h"
#include "stringdlg.h"
#include "ltbasedefs.h"
#include "addsocketdlg.h"
#include "geomroutines.h"
#include "transformmaker.h"
#include "animmergedlg.h"
#include "newgenlod.h"
#include "pieceinfodlg.h"
#include "genloddlg.h"
#include "addchildmodeldlg.h"
#include "regmgr.h"
#include "lteulerangles.h"
#include "socketedit.h"
#include "weightsetselectdlg.h"
#include "loadltadlg.h"
#include <process.h>
#include <set>
#include <map>

// talon: what ModelLoadRequest::m_pLoadedSet points at (model_load.cpp
// reads it: child model name -> file names).
typedef std::map<std::string, std::set<std::string> > ChildModelRemap;


// ------------------------------------------------------------------------
/// std c++
#include <fstream>

using namespace std;
   
/// lta parse...
#include "ltaModel.h"

// ------------------------------------------------------------------------

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Externs
extern TCHAR szRegKeyCompany[];
extern TCHAR szRegKeyApp[];
extern TCHAR szRegKeyVer[];

// ------------------------------------------------------------------------
// TempVarSetter
// change value of variable for the life time of the class instance.
//
// TempVarSetter<int> tmpVal( m_IntVal, 100 );
// m_IntVal is equal to 100 until tmpVal gets deconstructed. 
// ------------------------------------------------------------------------
template<class T>
class TempVarSetter
{
public:
			
			TempVarSetter(T *pVar, T newVal)
			{
				m_PrevVal = *pVar;
				m_pValue = pVar;
				*pVar = newVal;
			}

			~TempVarSetter()
			{
				*m_pValue = m_PrevVal;
			}
			 
private:
	T		*m_pValue;
	T		m_PrevVal;
};


// Model transition time (this is a console variable in the engine)..
int g_CV_ModelTransitionMS = 200;


#define MENUCHECK(test) ((test) ? MF_CHECKED : MF_UNCHECKED)

static void SetListColumnWidth(CListCtrl *pList, int width, int iIgnore);

// talon: an LOD's info, inlined at 0x418e7d (OnImport); Build 69's
// Model::GetLOD, which model.h no longer has.
inline LODInfo* GetModelLODInfo(Model *pModel, uint32 i)
{
	if (i == 0)
		return &pModel->m_BaseLODInfo;
	else if (i-1 < pModel->m_LODs.GetSize())
		return &pModel->m_LODs[i-1];
	else
		return NULL;
}
static BOOL ImportKeyStrings(AnimInfo *pDest, AnimInfo *pSrc);
BOOL CreateKeyframe(ModelAnim *pAnim, int iPrev, int iNext, float fPercent);
int IsFilenameLTAExt( const char *filename );


// Used for socket rotation and positioning.
float GetRotAngle(int delta)
{
	return ((delta * MATH_CIRCLE) / 360.0f) * 0.25f;
}

float GetPosAmount(int delta)
{
	return (float)delta * (1.0f / 64.0f);
}

// ------------------------------------------------------------------------
// AddTracker
// ui thing.
// ------------------------------------------------------------------------
void AddTracker(CModelEditDlg *pDlg, COLORREF color, UINT ctlID, RTType type, DWORD iAxis)
{
	RectTracker *pTracker;
	CWnd *pWnd;

	if(pTracker = new RectTracker)
	{
		pTracker->m_Color = color;
		pTracker->m_CtlID = ctlID;

		if(pWnd = pDlg->GetDlgItem(ctlID))
		{
			pWnd->GetClientRect(&pTracker->m_Rect);
			pWnd->ClientToScreen(&pTracker->m_Rect);
			pDlg->ScreenToClient(&pTracker->m_Rect);
		}

		if(pTracker->m_Wnd.Create(
			NULL, 
			"RectTracker", 
			WS_CHILD | WS_VISIBLE, 
			pTracker->m_Rect, 
			pDlg, 
			-1, 
			NULL))
		{
			pTracker->m_Wnd.SetColor(color);
			pTracker->m_Wnd.SetForward(TRUE);
		}

		pTracker->m_Type = type;
		pTracker->m_Axis = iAxis;
		pDlg->m_Trackers.Append(pTracker);
	}
}

///////////////////////////////////////////////////////////////////////////
// ------------------------------------------------------------------------
// MEAnimInfo 
// ------------------------------------------------------------------------

MEAnimInfo::MEAnimInfo()
{
	m_bAnimPlayingForward = FALSE;
	m_dwNumTagged = 0;
}


void MEAnimInfo::Reset(Model *pModel)
{
	m_Wnd.SetActive(FALSE);
	m_Wnd.SetAnim(NULL);
	m_Tracker.m_TimeRef.Init(pModel);
}


// talon: 2001's (0x4162de): back to the start of the current animation.
// The name is ours; OnAnimationCleanup calls it.
void MEAnimInfo::ResetTime()
{
	m_Tracker.m_TimeRef.SetKeyframePosition(CurAnimIndex(), 0, 0, 0.0f);
	m_bAnimPlayingForward = FALSE;
}


void MEAnimInfo::StartPlayback()
{
	StopPlayback();
	m_bAnimPlayingForward = TRUE;
}


void MEAnimInfo::StopPlayback()
{
	m_bAnimPlayingForward = FALSE;
	m_Wnd.ForceNearestKeyframe();
}


BOOL MEAnimInfo::IsValid()
{
	return m_Tracker.m_TimeRef.IsValid();	
}


void MEAnimInfo::SetKeyframeWindowTime()
{
	m_Wnd.SetTime(m_Tracker.m_TimeRef.m_Cur.m_Time);
}


ModelAnim* MEAnimInfo::CurAnim()
{
	if(IsValid())
		return m_Tracker.m_TimeRef.m_pModel->GetAnim(CurAnimIndex());
	else
		return NULL;
}


/////////////////////////////////////////////////////////////////////////////
// CModelEditDlg dialog

CModelEditDlg::CModelEditDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CModelEditDlg::IDD, pParent),
	m_AllocCount(&g_DefAlloc)
{
	DWORD i;
	MEAnimInfo *pInfo;
	LTAnimTracker *animTrackerPointers[NUM_ANIM_INFOS];
	UINT animButtonIDs[NUM_ANIM_INFOS] =
	{
		IDC_ANIMATION1,
		IDC_ANIMATION2,
		IDC_ANIMATION3,
		IDC_ANIMATION4
	};

	//{{AFX_DATA_INIT(CModelEditDlg)
	m_bDrawSkeleton = FALSE;
	//}}AFX_DATA_INIT
	m_bMovePieces = FALSE;
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pModel = NULL;
	m_bEndDialog = FALSE;
	m_bTracking = FALSE;
	m_bInStringChange = FALSE;

	m_nCurrentLOD = 0;

	m_bChangesMade = FALSE;
	m_hAccel = NULL;

	m_RenderWnd.m_pModelEditDlg = this;
	m_bFillingNodeList = FALSE;

	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = GetAnimInfo(i);

		m_LastAnimActiveFlags[i] = 0;
		animTrackerPointers[i] = &pInfo->m_Tracker;
		pInfo->m_Tracker.m_Flags = AT_PLAYING | AT_LOOPING;
		pInfo->m_AnimButtonID = animButtonIDs[i];
		pInfo->m_Wnd.m_iWnd = i;
	}

	m_RenderWnd.InitAnims(animTrackerPointers);
}


CModelEditDlg::~CModelEditDlg()
{
	DeleteAndClearArray(m_Trackers);
	DeleteModel();
}

void CModelEditDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CModelEditDlg)
	DDX_Control(pDX, IDC_ANIMLIST, m_AnimList);
	DDX_Control(pDX, IDC_PIECES, m_PieceList);
	DDX_Control(pDX, IDC_NODELIST, m_NodeList);
	DDX_Control(pDX, IDC_GLOBAL_SPACE, m_GlobalSpace);
	DDX_Control(pDX, IDC_SOCKETLIST, m_SocketList);
	DDX_Control(pDX, IDC_FRAMETIME, m_EditFrameTime);
	DDX_Control(pDX, IDC_FRAMESTRING, m_EditFrameString);
	DDX_Control(pDX, IDC_CHILDMODELS, m_ChildModelList);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CModelEditDlg, CDialog)
	// talon: 2001's map (0x4750f8), entry for entry.
	//{{AFX_MSG_MAP(CModelEditDlg)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_LOADMODEL, OnLoadModel)
	ON_MESSAGE(WM_STARTIDLE, OnStartIdle)
	ON_BN_CLICKED(ID_HELP_ABOUT, OnHelpAbout)
	ON_BN_CLICKED(IDC_FPLAY, OnPlayForward)
	ON_BN_CLICKED(IDC_STOP, OnStop)
	ON_BN_CLICKED(IDC_MOVEDOWN, OnAnimMoveDown)
	ON_BN_CLICKED(IDC_MOVEUP, OnAnimMoveUp)
	ON_BN_CLICKED(IDC_DELETE, OnAnimDelete)
	ON_BN_CLICKED(IDC_NEXTKEY, OnNextKeyframe)
	ON_BN_CLICKED(IDC_PREVKEY, OnPrevKeyframe)
	ON_BN_CLICKED(IDC_IMPORT, OnImport)
	ON_BN_CLICKED(IDC_SAVE, OnSave)
	ON_BN_CLICKED(IDC_SAVEAS, OnSaveAs)
	ON_BN_CLICKED(IDC_CREATE_SINGLE_FRAME, OnCreateSingleFrame)
	ON_BN_CLICKED(IDC_RENAME, OnRenameAnim)
	ON_BN_CLICKED(IDC_WIREFRAME, OnWireframe)
	ON_BN_CLICKED(IDC_CONTINUOUS, OnContinuous)
	ON_BN_CLICKED(IDC_CAMERA_FOLLOW, OnCameraFollow)
	ON_BN_CLICKED(IDC_DIMS, OnDims)
	ON_BN_CLICKED(IDC_ANIM_BOX, OnAnimBox)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_BUILDLODS, OnBuildLODs)
	ON_WM_KEYDOWN()
	ON_BN_CLICKED(IDC_MODELINFO, OnModelInfo)
	ON_BN_CLICKED(IDC_COMMANDSTRING_BUTTON, OnCommandString)
	ON_BN_CLICKED(IDC_DIMENSIONS_BUTTON, OnDimensions)
	ON_BN_CLICKED(IDC_ANIMATION_FRAMERATE, OnAnimationFramerate)
	ON_BN_CLICKED(ID_ANIMATION_CLEANUP, OnAnimationCleanup)
	ON_BN_CLICKED(IDC_ANIMATIONLENGTH, OnAnimationLength)
	ON_BN_CLICKED(IDC_ANIMATIONINTERPOLAION, OnAnimationInterpolation)
	ON_BN_CLICKED(IDC_TRANSLATION_BUTTON, OnTranslationButton)
	ON_BN_CLICKED(IDC_ROTATION_BUTTON, OnRotationButton)
	ON_BN_CLICKED(IDC_XSUB, OnXsub)
	ON_BN_CLICKED(IDC_XADD, OnXadd)
	ON_BN_CLICKED(IDC_YSUB, OnYsub)
	ON_BN_CLICKED(IDC_YADD, OnYadd)
	ON_BN_CLICKED(IDC_ZSUB, OnZsub)
	ON_BN_CLICKED(IDC_ZADD, OnZadd)
	ON_BN_CLICKED(IDC_NUMBER, OnNumberAnim)
	ON_BN_CLICKED(IDC_DUPLICATEANIM, OnDuplicateAnim)
	ON_BN_CLICKED(IDC_RENAMENODE, OnRenameNode)
	ON_BN_CLICKED(IDC_UNROTATE_TOP_NODE, OnUnrotateTopNode)
	ON_BN_CLICKED(ID_GENERATE_VERTEX_NORMALS, OnGenerateVertexNormals)
	ON_BN_CLICKED(ID_REVERSE_ANIMATION, OnReverseAnimation)
	ON_BN_CLICKED(ID_SELECT_NULL_NODES, OnSelectNullNodes)
	ON_BN_CLICKED(ID_MODEL_REMOVENODE, OnRemoveNode)
	ON_BN_CLICKED(ID_PIECE_SET_TEXTURE, OnSetTexture)
	ON_BN_CLICKED(IDC_DRAWSKELETON, OnDrawSkeleton)
	ON_BN_CLICKED(ID_SHOW_SOCKETS, OnShowSockets)
	ON_BN_CLICKED(ID_SHOW_ATTACHMENTS, OnShowAttachments)
	ON_BN_CLICKED(ID_SOLID_ATTACHMENTS, OnSolidAttachments)
	ON_BN_CLICKED(ID_ADDCHILDMODEL, OnAddChildModel)
	ON_BN_CLICKED(ID_REMOVECHILDMODEL, OnRemoveChildModel)
	ON_BN_CLICKED(ID_ADD_SOCKET, OnAddSocket)
	ON_BN_CLICKED(ID_REMOVE_SOCKET, OnRemoveSocket)
	ON_BN_CLICKED(ID_RENAME_SOCKET, OnRenameSocket)
	ON_BN_CLICKED(ID_REBUILDCHILDMODELTREE, OnRebuildChildModelTree)
	ON_BN_CLICKED(ID_SHOW_NORMALS, OnShowNormals)
	ON_BN_CLICKED(ID_SHOW_NORMALREF, OnShowNormalRef)
	ON_BN_CLICKED(ID_OPTIONS_PROFILE, OnProfile)
	ON_WM_TIMER()
	ON_WM_LBUTTONDOWN()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_BN_CLICKED(ID_MODEL_MOOD, OnMood)
	ON_BN_CLICKED(ID_INTERNAL_RADIUS, OnInternalRadius)
	ON_BN_CLICKED(ID_CALC_INTERNAL_RADIUS, OnCalcInternalRadius)
	ON_BN_CLICKED(ID_EDIT_WEIGHTS, OnEditWeights)
	ON_BN_CLICKED(ID_PIECE_INFO, OnPieceInfo)
	ON_BN_CLICKED(ID_PIECE_MERGE, OnPieceMerge)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_NODELIST, OnEndlabeleditNodeList)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_NODELIST, OnItemChangedNodeList)
	ON_WM_DESTROY()
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_PIECES, OnItemChangedPieceList)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_PIECES, OnEndLabelEditPieceList)
	ON_NOTIFY(LVN_BEGINLABELEDIT, IDC_ANIMLIST, OnBeginLabelEditAnimList)
	ON_NOTIFY(LVN_ENDLABELEDIT, IDC_ANIMLIST, OnEndLabelEditAnimList)
	ON_NOTIFY(LVN_ITEMCHANGING, IDC_ANIMLIST, OnItemChangingAnimList)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_ANIMLIST, OnBeginDragAnimList)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDC_SORTANIM_NAME, OnSortAnimName)
	ON_COMMAND(IDC_SORTANIM_RELATION, OnSortAnimRelation)
	ON_LBN_DBLCLK(IDC_SOCKETLIST, OnDblClkSocketList)
	ON_EN_CHANGE(IDC_FRAMESTRING, OnFrameStringChange)
	ON_COMMAND(ID_OPTIONS_NEWBACKGROUND, OnNewBgColor)
	ON_COMMAND(IDC_DELETEANIM, OnAnimDelete)
	ON_COMMAND(IDC_DUPLICATEANIM, OnDuplicateAnim)
	ON_COMMAND(ID_OPTIONS_SHOWORIGINALMODEL, OnOptionsShoworiginalmodel)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CModelEditDlg public member functions


ModelSocket* CModelEditDlg::GetSelectedSocket()
{
	int curSel;

	if (!m_pModel)
		return NULL;

	curSel = m_SocketList.GetCurSel();
	if (curSel >= 0 && curSel < (int)m_pModel->NumSockets())
	{
		return m_pModel->GetSocket((DWORD)curSel);
	}
	else
	{
		return NULL;
	}
}


CButton* CModelEditDlg::GetTransformEdit(UINT id)
{
	CWnd *pWnd;

	pWnd = (CWnd*)GetDlgItem(id);
	if (pWnd /*&& pWnd->IsKindOf(RUNTIME_CLASS(CButton))*/)
		return (CButton*)pWnd;
	
	return NULL;
}


BOOL CModelEditDlg::IsButtonSelected(UINT id)
{
	CButton *pButton;

	if (pButton = GetTransformEdit(id))
	{
		return !!(pButton->GetState() & 0x3);
	}
	else
	{
		return FALSE;
	}
}


// The about dialog
void CModelEditDlg::OnHelpAbout()
{
	CAboutDlg dlg;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	dlg.DoModal();
}

void CModelEditDlg::InitTaggedArrays()
{
	uint32 i;

	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		InitTaggedArray(i);
	}
}

void CModelEditDlg::InitTaggedArray(uint32 iAnimInfo)
{
	GetAnimInfo(iAnimInfo)->m_Wnd.InitTaggedArray();
}

void CModelEditDlg::DeleteModel()
{
	if (m_pModel)
	{
		DeleteSocketAttachments();
		
		m_pModel->Term(TRUE); // We want it to delete the child models.
		delete m_pModel;
		m_pModel = NULL;
	
		ResetAnimInfos();
	}
}


void CModelEditDlg::DeleteSocketAttachments()
{
	DWORD i;
	ModelSocket *pSocket;

	
	if (m_pModel)
	{
		// Delete models attached to sockets.
		for (i=0; i < m_pModel->NumSockets(); i++)
		{
			pSocket = m_pModel->GetSocket(i);

			if (pSocket->m_pAttachment)
			{
				delete pSocket->m_pAttachment;
				pSocket->m_pAttachment = NULL;
			}
		}
	}
}


void CModelEditDlg::GetPlaybackActiveFlags(
	DWORD dwActive[NUM_ANIM_INFOS], 
	BOOL bAllIfNoKeysDown)
{
	DWORD i, nActive;

	// First clear the flags.
	for(i=0; i < NUM_ANIM_INFOS; i++)
		dwActive[i] = 0;

	// See what CTRL+1.. keys they have down.
	nActive = 0;
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		if((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
			(GetAsyncKeyState('1' + i) & 0x8000))
		{
			dwActive[i] |= PA_ACTIVE;
			++nActive;
		}
	}

	// If no special keys are down, apply the command to all animations.
	if(nActive == 0 && bAllIfNoKeysDown)
	{
		for(i=0; i < NUM_ANIM_INFOS; i++)
			dwActive[i] |= PA_ACTIVE;
	}

	// Set the valid flag.
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		if(GetAnimInfo(i)->IsValid())
			dwActive[i] |= PA_VALID;
	}
}


void CModelEditDlg::SetCurrentPosition(
	DWORD iAnimInfo,
	DWORD nKeyframe1, 
	DWORD nKeyframe2, 
	float nPercentBetween)
{
	AnimTimeRef *pRef;
	MEAnimInfo *pInfo;

	
	ASSERT(iAnimInfo < NUM_ANIM_INFOS);
	pInfo = &m_AnimInfos[iAnimInfo];
	
	pRef = &pInfo->m_Tracker.m_TimeRef;
	pRef->SetKeyframePosition(
		pRef->m_Prev.m_iAnim,
		nKeyframe1, 
		nKeyframe2, 
		nPercentBetween);

	if(iAnimInfo == ANIMINFO_MAIN)
	{
		UpdateEditFrameString( );
		UpdateEditFrameTime( );
	}
}
					

void CModelEditDlg::SetCurrentPosition(
	CKeyframeWnd *pWnd,
	DWORD nKeyframe1, 
	DWORD nKeyframe2, 
	float nPercentBetween)
{
	DWORD i;

	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		if(&m_AnimInfos[i].m_Wnd == pWnd)
		{
			SetCurrentPosition(i, nKeyframe1, nKeyframe2, nPercentBetween);
			break;
		}
	}
}
					

/////////////////////////////////////////////////////////////////////////////
// CModelEditDlg protected member functions

void CModelEditDlg::OnIdle()
{
	uint32 i;
	DWORD msDelta, dwTime;
	MEAnimInfo *pInfo;


	// Update the animation playback stuff.
	OnSelChangeAnimList();

	dwTime = timeGetTime();

	msDelta = dwTime - m_LastIdleTime;
	msDelta = DCLAMP(msDelta, 1, 1000);

	m_LastIdleTime = dwTime;


	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = GetAnimInfo(i);

		if (pInfo->m_bAnimPlayingForward)
		{
			InterpForward(i, msDelta);
		}
	}

	DrawActiveAnimRects(FALSE);

	m_RenderWnd.Draw();
}

void CModelEditDlg::InterpForward(uint32 iAnimInfo, DWORD msDelta)
{
	AnimTimeRef *pTimeRef;
	MEAnimInfo *pInfo;

	
	pInfo = GetAnimInfo(iAnimInfo);
	pTimeRef = &pInfo->m_Tracker.m_TimeRef;

	// get the current time in the animation
	trk_Update(&pInfo->m_Tracker, msDelta);

	// tell the keyframe window where are are
	pInfo->SetKeyframeWindowTime();
	
	UpdateEditFrameString();
}

// {BP 1/20/97}
// Verifies that two model files are similar...
BOOL CModelEditDlg::VerifyModels( Model *pModel1, Model *pModel2 )
{
	if ( !RecurseAndVerifyModels( pModel1->GetRootNode(), pModel2->GetRootNode() ))
		return FALSE;

	return TRUE;
}

BOOL CModelEditDlg::RecurseAndVerifyModels( ModelNode* pNode1, ModelNode* pNode2 )
{
	if ( pNode1->m_Children.GetSize() != pNode2->m_Children.GetSize())
	{
		// models don't match
		return FALSE;
	}

	for (DWORD i = 0; i < pNode1->m_Children; i++)
	{
		if (!RecurseAndVerifyModels(pNode1->m_Children[i], pNode2->m_Children[i]))
		{
			return FALSE;
		}
	}

	return TRUE;
}


// ------------------------------------------------------------------------
// load_LTA_Using_FeedBackDialog( filename, memory-allocation-counter )
// helper function to load lta files and showing the load feed-back dialog.
// returns NULL if failed, the alloc'd model file if successful.
// ------------------------------------------------------------------------
Model* CModelEditDlg::load_LTA_Using_FeedBackDialog( const char *pFilename, bool isChildModel  )
{
	DWORD WINAPI LoadLTAThreadMain( LPVOID pUser );
	DWORD threadID;

	ifstream InFile(pFilename);
	if (!InFile)
		return NULL;

	m_AllocCount.ClearCounts();

	//creat the dialog that will exist on this thread to display
	//the status
	CLoadLTADlg LoadDlg;

	//tell it that the thread will now be running
	CLoadLTADlg::SetLoadThreadDone(FALSE);
	CLoadLTADlg::ClearLoadLog();


	//create the parameters for the thread
	CLoadLTAThreadParams Params;
	Params.m_pInFile		= &InFile;
	Params.m_pAllocCount	= &m_AllocCount ;
	Params.m_pModel			= NULL;
	Params.m_bChildLoad     = isChildModel;
	Params.m_Filename       = pFilename ;

	// tell the lta translation status thingy what to call when updateing 
	CLTATranslStatus::SetLoadLogCB( CLoadLTADlg::AppendLoadLog );

	//spawn the new thread to handle the loading of the LTA
	if( !CreateThread(NULL, 0, LoadLTAThreadMain, &Params, 0, &threadID) )
	{
		MessageBox("Failed to create thread to load LTA", "Error!");
		return FALSE;
	}

	//set the filename
	LoadDlg.m_sFilename = pFilename ;

	//tell the dialog to take control. It will not return until
	//the LTA loading thread has finished and the user has closed
	//it
	LoadDlg.DoModal();

	InFile.close();

	//unload the parameters that have been modified by the thread
	return  Params.m_pModel;	
}


// ------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------
Model* LoadChildModel(ModelLoadRequest *pRequest, char *pBaseName, char *pChildFilename)
{
	char dirName[256], newFilename[256];
	Model *pModel;

	
	CHelpers::ExtractNames(pBaseName, dirName, NULL, NULL, NULL);
	if (dirName[0] == 0)
	{
		sprintf(newFilename, "%s", pChildFilename);
	}
	else
	{
		sprintf(newFilename, "%s\\%s", dirName, pChildFilename);
	}

	// talon: 2001 loads the child's .abc (Build 69's only reads LTA).
	pRequest->m_pFile = streamsim_Open(newFilename, "rb");
	if (pRequest->m_pFile)
	{
		pModel = new Model;
		if (pModel)
		{
			if (pModel->Load(pRequest) != LT_OK)
			{
				delete pModel;
				pModel = NULL;
			}

			pRequest->m_pFile->Release();
			return pModel;
		}
	}

	return NULL;
}


DRESULT ME_LoadChildFn(ModelLoadRequest *pRequest, Model **ppModel)
{
	*ppModel = LoadChildModel(pRequest, (char*)pRequest->m_pLoadFnUserData, pRequest->m_pFilename);
	if (*ppModel)
		return LT_OK;
	else
		return LT_NOCHANGE;
}


//this is the thread main function that handles the loading of an LTA file. The parameter
//is a stucture containing a handle to an ifstream, and a pointer to where the model
//will be stored.
DWORD WINAPI LoadLTAThreadMain( LPVOID pUser )
{
	//get the parameters
	CLoadLTAThreadParams* pParams = (CLoadLTAThreadParams*)pUser;

	//load up the model
	if( pParams->m_bChildLoad)
	{
		pParams->m_pModel = ltaLoadChildModel(pParams->m_Filename.c_str(), pParams->m_pAllocCount);
	}
	else
	{
		pParams->m_pModel = ltaModelLoad(*pParams->m_pInFile, *(pParams->m_pAllocCount));
	}

	//tell the dialog we are done, and that it can exit
	CLoadLTADlg::SetLoadThreadDone(TRUE);

	return 0;
}

// ------------------------------------------------------------------------
// DoLoadModel( filenmae , bMessage )
// 
// ------------------------------------------------------------------------
BOOL CModelEditDlg::DoLoadModel(char *pFilename, BOOL bMessage)
{
	CString titleText;
	Model *pModel;
	ModelLoadRequest request;
	DWORD i;
	ILTStream *pFile;
	LTRESULT err;
	char msg[256];
	float mag;

	// Stop any animations...
	StopAllPlayback();

	if( IsFilenameLTAExt(pFilename) )
	{
		pModel = load_LTA_Using_FeedBackDialog( pFilename );

		// talon: RETAIL BUG: no return here; a failed LTA load goes on with
		// a NULL model and DoLoadModel dereferences it.
		if( pModel == NULL )
		{
			MessageBox("Failed to load lta file","load error");
		}
	}
	else
	{
		pFile = streamsim_Open(pFilename, "rb");
		if (!pFile)
		{
			if (bMessage)
				MessageBox("Could not open file", "File Error", MB_OK|MB_ICONEXCLAMATION);

			return FALSE;
		}

		try
		{
			request.m_pFile = pFile;
			request.m_LoadChildFn = ME_LoadChildFn;
			request.m_pLoadFnUserData = pFilename;
			// talon: RETAIL BUG: nothing ever deletes this (a leak per load).
			request.m_pLoadedSet = new ChildModelRemap;

			m_AllocCount.ClearCounts();
			pModel = new Model(&m_AllocCount);

			err = pModel->Load(&request);
			if (err != LT_OK)
			{
				if (pModel)
					delete pModel;

				pFile->Release();

				if (bMessage)
				{
					sprintf(msg, "error number %d on load ", err);
					MessageBox(msg, "File Error", MB_OK|MB_ICONEXCLAMATION);
				}

				return FALSE;
			}
		}
		catch(CLithException &exc)
		{
			pFile->Release();

			if (bMessage)
				MessageBox("Model file is invalid", "File Error", MB_OK|MB_ICONEXCLAMATION);

			return FALSE;
		}

		pFile->Release();
	}
	
	// Get rid of the old one.
	DeleteModel();
	m_pModel = pModel;
	m_strFilename = pFilename;

	SortPiecesByTexture();

	// init dialog controls
	FillAnimList();
	FillNodeList();
	FillSocketList();
	FillChildModelList();
	FillPieceList();

	m_AnimList.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
	for (i = 1; i < (DWORD)m_AnimList.GetItemCount(); i++)
		m_AnimList.SetItemState((int)i, 0, LVIS_SELECTED);

	// Init animations..
	OnSelChangeAnimList();


	m_RenderWnd.AllocSelections( m_pModel->NumPieces(), m_pModel->NumNodes());

	// enable appropriate windows

	m_btnPrevKey.EnableWindow();
	m_btnStop.EnableWindow();
	m_btnFPlay.EnableWindow();
	m_btnNextKey.EnableWindow();
	m_btnMoveUp.EnableWindow();
	m_btnMoveDown.EnableWindow();
	m_btnDelete.EnableWindow();
	m_AnimList.EnableWindow();


	m_bChangesMade = FALSE;


	titleText.FormatMessage(IDS_TITLE_TEXT, pFilename);
	SetWindowText((LPCTSTR)titleText);

	if (m_pModel->m_Anims > 0)
	{
		mag = m_pModel->GetAnimInfo(0)->m_vDims.Mag();

		m_RenderWnd.m_Scale = mag / 200.0f;
		m_RenderWnd.m_Locator.Center();
		m_RenderWnd.m_Locator.m_nDistance = mag * 2;
		m_RenderWnd.m_Locator.UpdateLocation();
	}
	else
	{
		m_RenderWnd.m_Scale = 1.0f;
	}

	m_RenderWnd.ResetLocator();

	UpdateEditFrameString( );

	ProcessCommandString();

	SetCurrentLOD(0);
	InitTheScrollBar(m_pModel->NumLODs() - 1);

	CheckChildModels(&request);

	// Load the sockets that are defined in the registry
	CRegMgr regMgr;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Attachments", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = 256;
		CString csAttachment;
		for (i = 0; i < m_pModel->NumSockets(); i++)
		{
			ModelSocket *pSocket = m_pModel->GetSocket(i);
			if (!pSocket)
				continue;
			dwSize = 256;
			if (regMgr.Get(pSocket->GetName(), csAttachment.GetBufferSetLength(dwSize), dwSize) != NULL)
			{
				csAttachment.ReleaseBuffer(-1);

				if (pSocket->m_pAttachment)
					delete pSocket->m_pAttachment;
				pSocket->m_pAttachment = NULL;

				if (!csAttachment.IsEmpty())
				{
					const TCHAR* filename = csAttachment ;
					if( IsFilenameLTAExt( filename ) )
					{
						Model *pModel = load_LTA_Using_FeedBackDialog( filename, false  );
						if( pModel != NULL )
						{
							pSocket->m_pAttachment = pModel ;
						}
					}
					else 
					{
						// Load the attachment
						if (request.m_pFile = streamsim_Open(csAttachment, "rb"))
						{
							pSocket->m_pAttachment = new Model;
							pSocket->m_pAttachment->Load(&request);
							request.m_pFile->Release();
						}
					}
				}
			}
		}
	}

	CRegMgr renderReg;
	if (renderReg.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Render", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = sizeof(float)*3;
		float bgColor[3];
	
		renderReg.Get("bgColor", bgColor, dwSize);

		SetGLMBackgroundColor( m_RenderWnd.GetContext(), bgColor );	
	}
	
	return TRUE;// Success !!
}


// ------------------------------------------------------------------------
// IsLtaFile
// does this filename have an lta extention, (case insensitive)
// ------------------------------------------------------------------------
int IsFilenameLTAExt( const char *filename )
{
	CString pathName ;

	// not enough chars in the name  
	if( strlen( filename ) < 4 )
		return 0;

	pathName = filename ; 
	CString szFileExt = pathName.Right(4);
	szFileExt.MakeLower();

	if( szFileExt == ".lta" )
		return 1;
	else
		return 0;
}


void CModelEditDlg::UpdateMenuChecks()
{
	CMenu *pMenu;
	CRegMgr regMgr;

	pMenu = GetMenu();
	if (!pMenu)
		return;

	// Since the submenus don't have IDs....
	pMenu = pMenu->GetSubMenu(6);
	if (!pMenu)
		return;

	pMenu->CheckMenuItem(IDC_CAMERA_FOLLOW, MENUCHECK(m_RenderWnd.m_bCameraFollow));
	pMenu->CheckMenuItem(IDC_WIREFRAME, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bWireframe));
	pMenu->CheckMenuItem(IDC_DIMS, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bDims));
	pMenu->CheckMenuItem(IDC_ANIM_BOX, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bAnimBox));
	pMenu->CheckMenuItem(IDC_DRAWSKELETON, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bDrawSkeleton));
	pMenu->CheckMenuItem(ID_OPTIONS_SHOWORIGINALMODEL, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bDrawOriginalModel));
	pMenu->CheckMenuItem(ID_SHOW_SOCKETS, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bDrawSockets));
	pMenu->CheckMenuItem(ID_SHOW_ATTACHMENTS, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bDrawAttachments));
	pMenu->CheckMenuItem(ID_SOLID_ATTACHMENTS, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bSolidAttachments));
	pMenu->CheckMenuItem(ID_SHOW_NORMALS, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bNormals));
	pMenu->CheckMenuItem(ID_SHOW_NORMALREF, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bShowNormalRef));
	pMenu->CheckMenuItem(ID_OPTIONS_PROFILE, MENUCHECK(m_RenderWnd.m_DrawStruct.m_bProfile));

	// talon: 2001 keeps the options in the registry (0x41789d..).
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Render", HKEY_CURRENT_USER))
	{
		regMgr.Set("CameraFollow", m_RenderWnd.m_bCameraFollow ? "1" : "0");
		regMgr.Set("Wireframe", m_RenderWnd.m_DrawStruct.m_bWireframe ? "1" : "0");
		regMgr.Set("Dims", m_RenderWnd.m_DrawStruct.m_bDims ? "1" : "0");
		regMgr.Set("AnimBox", m_RenderWnd.m_DrawStruct.m_bAnimBox ? "1" : "0");
		regMgr.Set("DrawSkeleton", m_RenderWnd.m_DrawStruct.m_bDrawSkeleton ? "1" : "0");
		regMgr.Set("DrawOriginalModel", m_RenderWnd.m_DrawStruct.m_bDrawOriginalModel ? "1" : "0");
		regMgr.Set("DrawSockets", m_RenderWnd.m_DrawStruct.m_bDrawSockets ? "1" : "0");
		regMgr.Set("DrawAttachments", m_RenderWnd.m_DrawStruct.m_bDrawAttachments ? "1" : "0");
		regMgr.Set("SolidAttachments", m_RenderWnd.m_DrawStruct.m_bSolidAttachments ? "1" : "0");
		regMgr.Set("Normals", m_RenderWnd.m_DrawStruct.m_bNormals ? "1" : "0");
		regMgr.Set("ShowNormalRef", m_RenderWnd.m_DrawStruct.m_bShowNormalRef ? "1" : "0");
		regMgr.Set("Profile", m_RenderWnd.m_DrawStruct.m_bProfile ? "1" : "0");
	}
}


void CModelEditDlg::CheckChildModels(ModelLoadRequest *pRequest)
{
	Model *pModel;
	DWORD i;
	ChildInfo *pChild;
	CString str;

	
	pModel = m_pModel;
	if (!pModel)
		return;

	// Regenerate the trees.
	if (!pRequest->m_bTreesValid)
	{
		if (DoMessageBox(IDS_TREESINVALID, MB_YESNO) == IDYES)
		{
			for (i=0; i < pModel->NumChildModels(); i++)
			{
				pChild = pModel->GetChildModel(i);
				
				if (!pChild->m_bTreeValid)
				{
					str.FormatMessage(IDS_CANTCREATETREE, pChild->m_pFilename);
					DoMessageBox(str, MB_OK);
				}
			}
		}
	}

	// Now check to see if it's out of date with any of its child models.
	for (i=0; i < pModel->NumChildModels(); i++)
	{
		pChild = pModel->GetChildModel(i);

		// Was it unable to load?
		if (!pChild->m_pModel)
		{
			str.FormatMessage(IDS_CANT_LOAD_CHILD_MODEL, pChild->m_pFilename);
			DoMessageBox(str, MB_OK);
		}

		if (!pChild->m_bTreeValid)
			continue;
	}
}

// fill a piece list with a given models pieces
// talon: 2001's (0x417b6e): the pieces in texture order.  The name is ours.
void CModelEditDlg::SortPiecesByTexture()
{
	DWORD i, j;
	ModelPiece *pTemp;

	if (m_pModel->NumPieces() == 0)
		return;

	for (i=0; i < m_pModel->NumPieces()-1; i++)
	{
		for (j=i; j < m_pModel->NumPieces(); j++)
		{
			if (m_pModel->m_Pieces[j]->m_iTexture < m_pModel->m_Pieces[i]->m_iTexture)
			{
				pTemp = m_pModel->m_Pieces[j];
				m_pModel->m_Pieces[j] = m_pModel->m_Pieces[i];
				m_pModel->m_Pieces[i] = pTemp;
			}
		}
	}
}


// talon: 2001's piece list is a list view (Build 69's is a tree).
void CModelEditDlg::FillPieceList()
{
	DWORD i;

	m_PieceList.DeleteAllItems();

	if (m_pModel)
	{
		for (i=0; i < m_pModel->NumPieces(); i++)
		{
			m_PieceList.InsertItem(i, m_pModel->GetPiece(i)->GetName());
		}

		SetListColumnWidth(&m_PieceList, 0, -1);
	}
}


// talon: 2001's (0x417c3e): sizes a list view's column to its widest
// item, leaving out iIgnore.  The name is ours.
static void SetListColumnWidth(CListCtrl *pList, int width, int iIgnore)
{
	CString str;
	int i, curWidth, maxWidth;

	maxWidth = width;
	for (i=0; i < pList->GetItemCount(); i++)
	{
		if (i != iIgnore)
		{
			str = pList->GetItemText(i, 0);
			curWidth = pList->GetStringWidth(str);
			if (curWidth > maxWidth)
				maxWidth = curWidth;
		}
	}

	pList->SetColumnWidth(0, maxWidth + 32);
}


void CModelEditDlg::FillNodeList()
{
	DWORD i;
	ModelNode *pNode;


	m_NodeList.DeleteAllItems();
	
	if (!m_pModel)
		return;

	m_bFillingNodeList = TRUE;

		for (i = 0; i < m_pModel->NumNodes(); i++)
		{
			pNode = m_pModel->GetNode(i);
			
			m_NodeList.InsertItem(i, pNode->GetName());
			m_NodeList.SetCheck((int)i, !!(pNode->m_Flags & MNODE_ROTATIONONLY));
		}

	m_bFillingNodeList = FALSE;

	SetListColumnWidth(&m_NodeList, 0, -1);
}


void CModelEditDlg::FillSocketList(BOOL bPreserveSel)
{
	DWORD i;
	ModelSocket *pSocket;
	int iLastSel;

	
	iLastSel = m_SocketList.GetCurSel();

	m_SocketList.ResetContent();

	if (!m_pModel)
		return;

	for (i=0; i < m_pModel->NumSockets(); i++)
	{
		pSocket = m_pModel->GetSocket(i);

		m_SocketList.AddString(pSocket->GetName());
	}

	if (bPreserveSel)
	{
		m_SocketList.SetCurSel(iLastSel);
	}
	else
	{
		m_SocketList.SetCurSel(0);
	}
}


void CModelEditDlg::FillAnimList()
{
	DWORD i;

	m_AnimList.DeleteAllItems();

	if (m_pModel)
	{
		for (i = 0; i < m_pModel->m_Anims; i++)
		{
			m_AnimList.InsertItem(i, m_pModel->GetAnim(i)->GetName());
			m_AnimList.SetCheck(i, (m_pModel->GetAnimInfo(i))->GetAnimOwner() == m_pModel);
		}
	}

	SetListColumnWidth(&m_AnimList, 0, -1);
}


void CModelEditDlg::FillChildModelList()
{
	ChildInfo *pChild;
	DWORD i;

	m_ChildModelList.ResetContent();

	if (m_pModel)
	{
		for (i=0; i < m_pModel->NumChildModels(); i++)
		{
			pChild = m_pModel->GetChildModel(i);

			if (pChild != m_pModel->GetSelfChildModel())
			{
				m_ChildModelList.AddString(pChild->m_pFilename);
			}
		}
	}
}


void CModelEditDlg::InitTheScrollBar(int nLODs)
{
	CScrollBar *pScrollBar;
	SCROLLINFO info;

	pScrollBar = (CScrollBar*)GetDlgItem(IDC_LOD_SCROLL);
	if (pScrollBar)
	{
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		info.nMin = 0;
		info.nMax = nLODs;
		info.nPage = 1;
		info.nPos = info.nTrackPos = 0;
		
		pScrollBar->SetScrollInfo(&info);
	}

	SetLODText(0);
}



/////////////////////////////////////////////////////////////////////////////
// CModelEditDlg message handlers

LONG CModelEditDlg::OnStartIdle (UINT, LONG)
{
	static BOOL bFirst=TRUE;

	// Handle startup options (we do it in the idle handler so we can do MessageBox).
	if (bFirst)
	{
		bFirst = FALSE;

		if (__argc > 1)
		{
			DoLoadModel(__argv[1], FALSE);
		}

		if (__argc > 2)
		{
			SetRenderTexture(__argv[2]);
		}
	}

	MSG msg;
	if (!PeekMessage(&msg, GetSafeHwnd(), 0,0, PM_NOREMOVE))
		OnIdle();
	m_cWinIdle.NextIdle();

	return 0;
}


BOOL CModelEditDlg::OnInitDialog()
{
	CButton *pButton;
	UINT keyframeWndIDs[NUM_ANIM_INFOS] =
	{
		IDC_KEYFRAME1,
		IDC_KEYFRAME2,
		IDC_KEYFRAME3,
		IDC_KEYFRAME4
	};
	UINT animActiveRectIDs[NUM_ANIM_INFOS] =
	{
		IDC_ANIMACTIVE1,
		IDC_ANIMACTIVE2,
		IDC_ANIMACTIVE3,
		IDC_ANIMACTIVE4
	};
	uint32 i;
	MEAnimInfo *pInfo;
	CRegMgr regMgr;
	char buf[32];
	UINT32 dwSize;


	// Run in a low-priority thread so we're friendly in the background..
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);


	CDialog::OnInitDialog();

	CRect rect;

	// Set up the node view
	m_NodeList.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_NodeList.GetWindowRect(&rect);
	m_NodeList.InsertColumn(0, "Nodes", LVCFMT_LEFT, rect.Width() -17);

	// Set up the piece view
	m_PieceList.SetExtendedStyle(LVS_EX_FULLROWSELECT);
	m_PieceList.GetWindowRect(&rect);
	m_PieceList.InsertColumn(0, "Pieces", LVCFMT_LEFT, rect.Width() -17);

	// Set up the animation list
	m_AnimList.SetDropNotify(this, (FDragListCallback)OnDropAnimList);
	m_AnimList.SetExtendedStyle(LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
	m_AnimList.GetWindowRect(&rect);
	m_AnimList.InsertColumn(0, "Name", LVCFMT_LEFT, rect.Width() - 17);

	// Setup the trackers.
	AddTracker(this, RGB(255,0,0), IDC_SOCKETROTRIGHT, RT_ROTATION, 0);
	AddTracker(this, RGB(0,255,0), IDC_SOCKETROTUP, RT_ROTATION, 1);
	AddTracker(this, RGB(0,0,255), IDC_SOCKETROTFORWARD, RT_ROTATION, 2);
	AddTracker(this, RGB(255,0,0), IDC_SOCKETPOSRIGHT, RT_POSITION, 0);
	AddTracker(this, RGB(0,255,0), IDC_SOCKETPOSUP, RT_POSITION, 1);
	AddTracker(this, RGB(0,0,255), IDC_SOCKETPOSFORWARD, RT_POSITION, 2);


	if (pButton = GetTransformEdit(IDC_SOCKET_EDIT))
	{
		pButton->SetCheck(TRUE);
	}

	if (pButton = (CButton*)GetDlgItem(IDC_DIMS_EDIT))
	{
		pButton->SetCheck(TRUE);
	}


	m_hAccel = LoadAccelerators(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ACCELERATOR1));
	SetFocus();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// Initialize the Bitmap Buttons

	m_btnPrevKey.AutoLoad (IDC_PREVKEY, this);
	m_btnStop.AutoLoad (IDC_STOP, this);
	m_btnFPlay.AutoLoad (IDC_FPLAY, this);
	m_btnNextKey.AutoLoad (IDC_NEXTKEY, this);
	m_btnMoveUp.AutoLoad (IDC_MOVEUP, this);
	m_btnMoveDown.AutoLoad (IDC_MOVEDOWN, this);
	m_btnNumber.AutoLoad (IDC_NUMBER, this);
	m_btnDelete.AutoLoad (IDC_DELETE, this);

	CheckDlgButton (IDC_CAMERA_FOLLOW, TRUE);

	// create the render window

	CWnd* pStaticWnd = GetDlgItem (IDC_RENDER);
	CRect rcStatic;
	pStaticWnd->GetWindowRect (rcStatic);
	ScreenToClient (rcStatic);
	if (!m_RenderWnd.Create (NULL, "Render Window", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE, rcStatic, this, -1, NULL))
	{
		MessageBox ("Could not create render window", NULL, MB_OK);
		EndDialog (-1);
	}

	// talon: 2001 reads the options back from the registry (UpdateMenuChecks
	// writes them).
	if (!regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Render", HKEY_CURRENT_USER))
	{
		m_RenderWnd.m_bCameraFollow = TRUE;
		m_RenderWnd.m_DrawStruct.m_bWireframe = FALSE;
		m_RenderWnd.m_DrawStruct.m_bNormals = FALSE;
		m_RenderWnd.m_DrawStruct.m_bDims = FALSE;
		m_RenderWnd.m_DrawStruct.m_bAnimBox = FALSE;
		m_RenderWnd.m_DrawStruct.m_bDrawOriginalModel = FALSE;
		m_RenderWnd.m_DrawStruct.m_bDrawSockets = FALSE;
		m_RenderWnd.m_DrawStruct.m_bDrawAttachments = FALSE;
		m_RenderWnd.m_DrawStruct.m_bSolidAttachments = TRUE;
		m_RenderWnd.m_DrawStruct.m_bShowNormalRef = FALSE;
		m_RenderWnd.m_DrawStruct.m_bProfile = FALSE;
	}
	else
	{
		dwSize = sizeof(buf);

		if (regMgr.Get("CameraFollow", buf, dwSize))
			m_RenderWnd.m_bCameraFollow = buf[0] == '1';
		else
			m_RenderWnd.m_bCameraFollow = TRUE;

		if (regMgr.Get("Wireframe", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bWireframe = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bWireframe = FALSE;

		if (regMgr.Get("Dims", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bDims = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bDims = FALSE;

		if (regMgr.Get("AnimBox", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bAnimBox = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bAnimBox = FALSE;

		if (regMgr.Get("DrawSkeleton", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bDrawSkeleton = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bDrawSkeleton = FALSE;

		if (regMgr.Get("DrawOriginalModel", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bDrawOriginalModel = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bDrawOriginalModel = FALSE;

		if (regMgr.Get("DrawSockets", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bDrawSockets = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bDrawSockets = FALSE;

		if (regMgr.Get("DrawAttachments", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bDrawAttachments = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bDrawAttachments = FALSE;

		if (regMgr.Get("SolidAttachments", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bSolidAttachments = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bSolidAttachments = TRUE;

		if (regMgr.Get("Normals", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bNormals = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bNormals = FALSE;

		if (regMgr.Get("ShowNormalRef", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bShowNormalRef = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bShowNormalRef = FALSE;

		if (regMgr.Get("Profile", buf, dwSize))
			m_RenderWnd.m_DrawStruct.m_bProfile = buf[0] == '1';
		else
			m_RenderWnd.m_DrawStruct.m_bProfile = FALSE;
	}

	UpdateMenuChecks();

	m_RenderWnd.m_DrawStruct.m_FOV = CalcFOV();
	// Update the registry with the FOV, just in case it's not there..
	SetFOV(m_RenderWnd.m_DrawStruct.m_FOV);
	
	InitTheScrollBar(0);

	
	
	// create the keyframe windows.
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = GetAnimInfo(i);

		// Create the keyframe window.
		if(pStaticWnd = GetDlgItem (keyframeWndIDs[i]))
		{
			pStaticWnd->GetWindowRect (rcStatic);
			ScreenToClient (rcStatic);
			if (!pInfo->m_Wnd.Create (NULL, "Keyframe Window", WS_CHILD | WS_VISIBLE, rcStatic, this, -1, NULL))
			{
				MessageBox ("Could not create keyframe window", NULL, MB_OK);
				EndDialog (-1);
			}
		}

		// Create the flags window.
		if(pStaticWnd = GetDlgItem(animActiveRectIDs[i]))
		{
			pStaticWnd->GetWindowRect (rcStatic);
			ScreenToClient (rcStatic);
			if (!pInfo->m_ActiveFlagsWnd.Create (NULL, "Anim Active Flags", WS_CHILD | WS_VISIBLE, rcStatic, this, -1, NULL))
			{
				MessageBox ("Could not create anim flags window", NULL, MB_OK);
				EndDialog (-1);
			}
		}	

		m_AnimInfos[i].m_Wnd.EnableWindow(TRUE);
	}

	// disable appropriate windows 

	m_btnPrevKey.EnableWindow (FALSE);
	m_btnStop.EnableWindow (FALSE);
	m_btnFPlay.EnableWindow (FALSE);
	m_btnNextKey.EnableWindow (FALSE);
	m_btnMoveUp.EnableWindow (FALSE);
	m_btnMoveDown.EnableWindow (FALSE);
	m_btnDelete.EnableWindow (FALSE);
	m_AnimList.EnableWindow (FALSE);

	// Start the idle loop (Note : It is currently set to not idle more quickly than every 20ms)
	m_cWinIdle.StartIdle(GetSafeHwnd(), WM_STARTIDLE, 0,0, 20);
	m_cWinIdle.NextIdle();

	UpdateEditFrameString( );

	return TRUE;  // return TRUE  unless you set the focus to a control
}


// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CModelEditDlg::OnPaint() 
{
	CRect rect;
	CBrush brush;
	CPaintDC dc(this); // device context for painting


	if (IsIconic())
	{
		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CModelEditDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}


void CModelEditDlg::OnLoadModel() 
{
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	// Look up the open directory in the registry
	CRegMgr regMgr;
	CString csOpenDir;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "OpenDir", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = 256;
		regMgr.Get("Model", csOpenDir.GetBufferSetLength(dwSize), dwSize);
		csOpenDir.ReleaseBuffer(-1);
	}

	if (csOpenDir.Find('.') == -1)
		csOpenDir.Empty();

	// get the filename
	char szFilter[] = "Model Files (*.abc)|*.abc|Model Files (*.lta)|*.lta|All Files (*.*)|*.*||";

	CFileDialog dlg (TRUE, "abc", (csOpenDir.GetLength()) ? (LPCTSTR)csOpenDir : NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, szFilter, this);

	if (dlg.DoModal() != IDOK)
	{ 
		return;
	}

	// Store the open directory in the registry
	csOpenDir = dlg.GetPathName();
	csOpenDir = csOpenDir.Left(csOpenDir.GetLength() - dlg.GetFileName().GetLength()) + "*.abc";
	regMgr.Set("Model", (LPCTSTR)csOpenDir);
	
	DoLoadModel((char*)(LPCTSTR)dlg.GetPathName());
}


// import custom lods from another model file


// ------------------------------------------------------------------------
// OnImport stuff from other data files.
// The lta part could be vastly improved by accessing the parts of the
// lta that are needed instead of loading up the whole model and tossing 
// unwanted stuff out. For now this will stand
// ------------------------------------------------------------------------
void CModelEditDlg::OnImport() 
{
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	UVImportDlg uvDlg;
	Model model;
	ModelLoadRequest request;
	BuildLODRequest lodRequest;
	LODRequestInfo lodInfo;
	ILTStream *pFile;
	Model *pLtaModel;
	AnimInfo *pInfo, *pFoundInfo;
	ModelAnim *pAnim, *pFoundAnim;
	ModelSocket *pSocket, *pNewSocket, *pFoundSocket;
	ModelNode *pNode;
	WeightSet *pSet, *pNewSet;
	uint32 iNode, iAnim, iSet;
	DWORD i, k, count;
	char msg[256];

	// Look up the open directory in the registry
	CRegMgr regMgr;
	CString csOpenDir;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "OpenDir", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = 256;
		regMgr.Get("Model", csOpenDir.GetBufferSetLength(dwSize), dwSize);
		csOpenDir.ReleaseBuffer(-1);
	}

	char szFilter[] = "Model Files (*.abc)|*.abc|Model Files (*.lta)|*.lta|All Files (*.*)|*.*||";

	CImportAnimation dlg (TRUE, "abc", (csOpenDir.GetLength()) ? (LPCTSTR)csOpenDir : NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, szFilter, this);

	if (dlg.DoModal() != IDOK)
	{ 
		return;
	}

	// Store the open directory in the registry
	csOpenDir = dlg.GetPathName();
	csOpenDir = csOpenDir.Left(csOpenDir.GetLength() - dlg.GetFileName().GetLength()) + "*.abc";
	regMgr.Set("Model", (LPCTSTR)csOpenDir);

	CWaitCursor waitCursor;

	if (IsFilenameLTAExt(dlg.GetPathName()))
	{
		pLtaModel = load_LTA_Using_FeedBackDialog(dlg.GetPathName());
		model.CopyModel(pLtaModel);
		if (pLtaModel)
			delete pLtaModel;
	}
	else
	{
		if (!(pFile = streamsim_Open(dlg.GetPathName(), "rb")))
		{
			MessageBox("Could not open file", "File Error", MB_OK|MB_ICONEXCLAMATION);
			return;
		}

		try
		{
			request.m_pFile = pFile;
			if (model.Load(&request) != LT_OK)
			{
				MessageBox("Model file is incorrect version", "File Error", MB_OK|MB_ICONEXCLAMATION);
				return;
			}
		}
		catch(CLithException &exc)
		{
			MessageBox("Model file is invalid", "File Error", MB_OK|MB_ICONEXCLAMATION);
			return;
		}

		pFile->Release();
	}

	if (dlg.m_bImportKeystrings)
	{
		count = 0;
		for (i=0; i < model.m_Anims; i++)
		{
			pInfo = &model.m_Anims[i];
			if (pInfo)
			{
				pFoundInfo = m_pModel->FindAnimInfo(pInfo->m_pAnim->GetName(), m_pModel);
				if (pFoundInfo && ImportKeyStrings(pFoundInfo, pInfo))
					count++;
			}
		}

		if (count)
		{
			m_bChangesMade = TRUE;
			ResetAnimInfos();
			OnSelChangeAnimList();
		}

		sprintf(msg, "Imported keystrings from %d animations.", count);
		MessageBox(msg, "ModelEdit");
	}

	if (dlg.m_bImportUserDims)
	{
		count = 0;
		for (i=0; i < model.m_Anims; i++)
		{
			pInfo = &model.m_Anims[i];
			pFoundInfo = m_pModel->FindAnimInfo(pInfo->m_pAnim->GetName(), m_pModel);
			if (pFoundInfo)
			{
				pFoundInfo->m_vDims = pInfo->m_vDims;
				count++;
			}
		}

		sprintf(msg, "Imported user dims from %d animations.", count);
		MessageBox(msg, "ModelEdit");
	}

	if (dlg.m_bImportSockets)
	{
		count = 0;
		for (i=0; i < model.NumSockets(); i++)
		{
			pSocket = model.GetSocket(i);
			pFoundSocket = m_pModel->FindSocket(pSocket->GetName());
			if (pFoundSocket)
			{
				pFoundSocket->m_Pos = pSocket->m_Pos;
				pFoundSocket->m_Rot = pSocket->m_Rot;
				count++;
			}
			else
			{
				if (m_pModel->FindNode(model.GetNode(pSocket->m_iNode)->GetName(), &iNode))
				{
					pNewSocket = new ModelSocket;
					pNewSocket->m_iNode = iNode;
					pNewSocket->SetName(pSocket->GetName());
					pNewSocket->m_Pos = pSocket->m_Pos;
					pNewSocket->m_Rot = pSocket->m_Rot;

					if (m_pModel->AddSocket(pNewSocket))
						count++;
					else
						delete pNewSocket;
				}
			}
		}

		if (count)
			FillSocketList(TRUE);

		sprintf(msg, "Imported %d sockets.", count);
		MessageBox(msg, "ModelEdit");
	}

	if (!VerifyModels(m_pModel, &model))
	{
		MessageBox("Model geometry does not match.  Cannot import from model.", "Error", MB_OK|MB_ICONEXCLAMATION);
		return;
	}

	if (dlg.m_bImportLODs)
	{
		lodRequest.m_pModel = m_pModel;
		count = 0;
		lodRequest.m_PieceWeights.SetSizeInit2(m_pModel->NumPieces(), 1.0f);
		for (i=0; i < model.NumPieces(); i++)
		{
			lodRequest.m_PieceWeights[i] = model.GetPiece(i)->m_LODWeight;
			m_pModel->GetPiece(i)->m_LODWeight = lodRequest.m_PieceWeights[i];
		}

		for (i=0; i < model.NumLODs(); i++)
		{
			if (i)
			{
				lodInfo.m_Dist = GetModelLODInfo(&model, i)->m_Dist;
				lodInfo.m_nTris = (m_pModel->CalcNumTris(0) * model.CalcNumTris(i)) / model.CalcNumTris(0);
				lodRequest.m_LODInfos.Append(lodInfo);
				count++;
			}
		}

		if (!BuildLODs(&lodRequest))
		{
			MessageBox("Unable to build LOD's (% too small?)");
		}

		SetCurrentLOD(0);
		InitTheScrollBar(m_pModel->NumLODs() - 1);

		sprintf(msg, "Imported %d LODs.", count);
		MessageBox(msg, "ModelEdit");
	}

	if (dlg.m_bImportAnimations)
	{
		for (i=0; i < model.m_Anims; i++)
		{
			pAnim = new ModelAnim(m_pModel);
			pAnim->CopyAnim(model.m_Anims[i].m_pAnim);
			m_pModel->AddAnimToSelf(pAnim);
		}

		FillAnimList();
		InitTaggedArrays();
		MessageBox(model.m_Anims < 2 ? "New animation has been added to end of listbox" : "New animations have been added to end of listbox", "Model Editor", MB_OK|MB_ICONINFORMATION);
		m_bChangesMade = TRUE;
	}

	if (dlg.m_bUseUVCoords)
	{
		if (m_pModel->CopyUVs(&model))
			MessageBox("UV Coordinates imported.", "Model Editor", MB_OK|MB_ICONINFORMATION);
		else
			MessageBox("Model geometry does not match.  Cannot import UV coords.", "Error", MB_OK|MB_ICONEXCLAMATION);
	}

	if (dlg.m_bImportTranslations)
	{
		count = 0;
		for (i=0; i < model.m_Anims; i++)
		{
			pAnim = model.m_Anims[i].m_pAnim;
			pFoundAnim = m_pModel->FindAnim(pAnim->GetName(), &iAnim);
			if (pFoundAnim && pFoundAnim->NumKeyFrames() == pAnim->NumKeyFrames())
			{
				count++;
				for (k=0; k < pAnim->NumKeyFrames(); k++)
				{
					pFoundAnim->GetRootNode()->m_KeyFrames[k].m_Translation = pAnim->GetRootNode()->m_KeyFrames[k].m_Translation;
				}
			}
		}

		sprintf(msg, "Imported translations from %d animations.", count);
		MessageBox(msg, "ModelEdit");
	}

	if (dlg.m_bImportWeightSets)
	{
		count = 0;
		for (i=0; i < model.NumWeightSets(); i++)
		{
			pSet = model.GetWeightSet(i);
			if (m_pModel->FindWeightSet(pSet->GetName(), &iSet))
				m_pModel->RemoveWeightSet(iSet);

			pNewSet = new WeightSet(m_pModel);
			if (pNewSet)
			{
				pNewSet->SetName(pSet->GetName());
				if (!pNewSet->m_Weights.CopyArray(pSet->m_Weights) || !m_pModel->AddWeightSet(pNewSet))
				{
					delete pNewSet;
				}
				else
				{
					count++;
				}
			}
		}

		sprintf(msg, "Imported %d weight sets.", count);
		MessageBox(msg, "ModelEdit");
	}
}


// talon: 2001's (0x4197d1): copies pSrc's keyframe strings onto pDest's
// keyframes at the same times, adding keyframes where pDest has none.
// The name is ours.
static BOOL ImportKeyStrings(AnimInfo *pDest, AnimInfo *pSrc)
{
	ModelAnim *pDestAnim, *pSrcAnim;
	AnimKeyFrame *pSrcKey, *pDestKey;
	int iSrc, iDest;
	DWORD t0;

	iDest = 0;
	if (!pDest || !pSrc)
		return FALSE;

	pSrcAnim = pSrc->m_pAnim;
	pDestAnim = pDest->m_pAnim;
	for (iSrc=0; iSrc < (int)pSrcAnim->m_KeyFrames.GetSize(); iSrc++)
	{
		if (iDest >= (int)pDestAnim->m_KeyFrames.GetSize())
			break;

		pSrcKey = &pSrcAnim->m_KeyFrames[iSrc];
		if (pSrcKey->m_pString && pSrcKey->m_pString[0])
		{
			pDestKey = NULL;
			for (; iDest < (int)pDestAnim->m_KeyFrames.GetSize(); iDest++)
			{
				pDestKey = &pDestAnim->m_KeyFrames[iDest];
				if (pDestKey->m_Time >= pSrcKey->m_Time)
				{
					if (pDestKey->m_Time != pSrcKey->m_Time)
					{
						t0 = pDestAnim->m_KeyFrames[iDest-1].m_Time;
						if (!CreateKeyframe(pDestAnim, iDest-1, iDest,
							(float)(pSrcKey->m_Time - t0) / (float)(pDestKey->m_Time - t0)))
						{
							return FALSE;
						}

						pDestKey = &pDestAnim->m_KeyFrames[iDest];
					}
					break;
				}
			}

			if (!pDestKey)
				return FALSE;

			pDestKey->m_pString = pDest->GetAnimOwner()->AddString(pSrcKey->m_pString);
		}
	}

	return TRUE;
}



// ------------------------------------------------------------------------
// OnSetProjectDir
// user asked to change the project dir.
// set the new project dir in the registry for later.
// ------------------------------------------------------------------------


// ------------------------------------------------------------------------
// Save, then call external packer with d3d bound args
// ------------------------------------------------------------------------

// ------------------------------------------------------------------------
// Call external packer with d3d bound args
// ------------------------------------------------------------------------


int CModelEditDlg::DoMessageBox(UINT idString, int style)
{
	CString str, caption;

	str.LoadString(idString);
	caption.LoadString(IDS_MODELEDIT_CAPTION);
	return MessageBox(str, caption, style);
}


int CModelEditDlg::DoMessageBox(LPCTSTR pStr, int style)
{
	CString caption;

	caption.LoadString(IDS_MODELEDIT_CAPTION);
	return MessageBox(pStr, caption, style);
}


BOOL CModelEditDlg::GetEditAnimInfo(DWORD *pAnimIndex, DWORD *pKeyframeIndex)
{
	DWORD iAnim, iKeyframe;

	if(!m_pModel)
		return FALSE;

	iAnim = m_AnimInfos[0].CurAnimIndex();
	if(iAnim >= m_pModel->NumAnims())
		return FALSE;

	if(!m_pModel->GetAnimInfo(iAnim)->IsParentModel())
		return FALSE;

	iKeyframe = m_AnimInfos[0].CurKeyframe();
	if(iKeyframe >= m_pModel->GetAnim(iAnim)->NumKeyframes())
		return FALSE;

	if(pAnimIndex)
		*pAnimIndex = iAnim;

	if(pKeyframeIndex)
		*pKeyframeIndex = iKeyframe;

	return TRUE;
}

// ------------------------------------------------------------------------
// DoSave( filename )
// save out file 
// ------------------------------------------------------------------------
void CModelEditDlg::DoSave(const char *pFilename)
{
	CString str;
	DWORD i, j, nParentAnims;
	ModelAnim *pAnim, *pTest;
	int ret;
	ILTStream *pStream;

	if (!m_pModel)
		return;

	// Look for duplicate animation names. These can cause problems.
	m_pModel->ForceAnimOrder();
	
	nParentAnims = m_pModel->CalcNumParentAnims();
	for(i=0; i < nParentAnims; i++)
	{
		pAnim = m_pModel->GetAnimInfo(i)->m_pAnim;

		for(j=0; j < nParentAnims; j++)
		{
			if(i == j)
				continue;

			pTest = m_pModel->GetAnimInfo(j)->m_pAnim;

			if(strcmp(pAnim->GetName(), pTest->GetName()) == 0)
			{
				str.FormatMessage(IDS_DUPLICATE_ANIM_NAME, pAnim->GetName());
				ret = DoMessageBox(str, MB_YESNOCANCEL);
				if(ret == IDCANCEL)
				{
					return;
				}
				else if(ret == IDYES)
				{
					DoRenameAnim(i);
				}
			}
		}
	}

	// talon: 2001 saves .abc (an .lta name is saved as .abc).
	CString filename = pFilename;
	filename.MakeLower();
	filename.Replace(".lta", ".abc");

	pStream = streamsim_Open(filename, "wb");
	if (!pStream)
	{
		MessageBox("Could not open file for writing.  Check to see that the file is not read-only.", "File Error", MB_OK|MB_ICONEXCLAMATION);
	}
	else
	{
		if (!m_pModel->Save(*pStream))
			MessageBox("Error saving.");

		pStream->Release();
		m_bChangesMade = FALSE;
	}
}


void CModelEditDlg::OnSave() 
{
	DoSave(m_strFilename);
}

// ------------------------------------------------------------------------
// Event Handler: OnSaveAs
// ------------------------------------------------------------------------
void CModelEditDlg::OnSaveAs() 
{
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	// Look up the model directory in the registry
	CRegMgr regMgr;
	CString csOpenDir;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "OpenDir", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = 256;
		regMgr.Get("Model", csOpenDir.GetBufferSetLength(dwSize), dwSize);
		csOpenDir.ReleaseBuffer(-1);
	}

	// get a filename from the user
	char szFilter[] = "Model Files (*.abc)|*.abc|All Files (*.*)|*.*||";

	CFileDialog dlg (FALSE, "abc", (csOpenDir.GetLength()) ? (LPCTSTR)csOpenDir : NULL, OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY, szFilter, this);
	if (dlg.DoModal() != IDOK)
	{
		return;
	}
	
	// Store the model directory in the registry
	csOpenDir = dlg.GetPathName();
	csOpenDir = csOpenDir.Left(csOpenDir.GetLength() - dlg.GetFileName().GetLength()) + "*.abc";
	regMgr.Set("Model", (LPCTSTR)csOpenDir);
	
	m_strFilename = dlg.GetPathName();
	DoSave(m_strFilename);
}

// ------------------------------------------------------------------------
// Event Handler: OnCancel 
// ------------------------------------------------------------------------
void CModelEditDlg::OnCancel() 
{
	if (m_bChangesMade)
	{
		if (MessageBox ("Save changes before exiting?", "Model Editor", MB_YESNO | MB_ICONQUESTION) == IDYES)
		{
			OnSave();

			// Still not saved?
			if (m_bChangesMade)
				return;
		}
	}

	m_bEndDialog = TRUE;
	CDialog::OnCancel();
}


void CModelEditDlg::OnStop() 
{
	uint32 i;
	MEAnimInfo *pInfo;
	DWORD dwActive[NUM_ANIM_INFOS];


	GetPlaybackActiveFlags(dwActive);

	// Apply the cm
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = GetAnimInfo(i);

		if(dwActive[i] & PA_ACTIVE)
		{
			GetAnimInfo(i)->StopPlayback();
		}
	}
}

void CModelEditDlg::OnPlayForward() 
{
	uint32 i;
	MEAnimInfo *pInfo;
	DWORD dwActive[NUM_ANIM_INFOS];


	GetPlaybackActiveFlags(dwActive);


	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = GetAnimInfo(i);

		if(dwActive[i])
		{
			GetAnimInfo(i)->StartPlayback();
		}
	}

	UpdateEditFrameString( );
}

void CModelEditDlg::OnNextKeyframe() 
{
	IncKeyframe(TRUE);
}


void CModelEditDlg::OnPrevKeyframe() 
{
	IncKeyframe(FALSE);
}

int CModelEditDlg::GetSelectedParentAnims(int *pItems, int maxItems)
{
	int i, nRetItems;

	if (!m_pModel)
		return -1;

	nRetItems = 0;
	for (i=0; i < m_AnimList.GetItemCount(); i++)
	{
		if ((i > (int)m_pModel->m_Anims.GetSize()) || (nRetItems >= maxItems))
			break;
		if ((!m_pModel->GetAnimIfOwner(i)) || (m_AnimList.GetItemState(i, LVIS_SELECTED) == 0))
			continue;

		pItems[nRetItems] = i;
		nRetItems++;
	}
	
	return nRetItems;			
}


BOOL CModelEditDlg::HandleButtonDown(int iButton, CPoint point)
{
	m_RenderWnd.ClientToScreen(&point);
	ScreenToClient(&point);
	
	if (GetAsyncKeyState('C') & 0x8000)
	{
		StartTracker(iButton, point);
		return TRUE;
	}
	else if (GetAsyncKeyState('V') & 0x8000)
	{
		StartTracker(iButton+3, point);
		return TRUE;
	}

	return FALSE;
}


BOOL CModelEditDlg::HandleButtonUp(int iButton)
{
	if (m_bTracking)
	{
		SetWindowText(m_OldWindowText);
		ShowCursor(TRUE);
		ReleaseCapture();
		m_bTracking = FALSE;
		SetChangesMade();
		return TRUE;
	}

	return FALSE;
}


void CModelEditDlg::StartTracker(uint32 iTracker, CPoint centerPt)
{
	if (iTracker >= m_Trackers.GetSize())
		return;

	m_TotalDelta = 0;
	GetWindowText(m_OldWindowText);
	ShowCursor(FALSE);
	m_bTracking = TRUE;
	m_iCurTracker = iTracker;
	m_LastCursorPos = centerPt;
	m_TrackerCenterPt = centerPt;
	SetCapture();
}


// Moves selected animations up or down, depending on dir.
void CModelEditDlg::MoveAnims(int dir)
{
	int i, j, iTemp, nItems, items[1024], nSelection, nDest;
	CString strSelection;
	AnimInfo temp;
	BOOL bChecked;
	
	if ((!m_pModel) || (!dir))
		return;
		
	nItems = GetSelectedParentAnims(items, 1024);
	if (nItems == -1)
		return;

	// So we don't have animations pointing at bad data.
	ResetAnimInfos();

	// Process in reverse order for positive directions
	if (dir > 0)
	{
		for (i=0; i < (nItems / 2); i++)
		{
			iTemp = items[i];
			items[i] = items[nItems - (i + 1)];
			items[nItems - (i + 1)] = iTemp;
		}
	}

	for (i=0; i < nItems; i++)
	{
		nSelection = items[i];
		nDest = max(min(nSelection + dir, (int)m_pModel->m_Anims.GetSize() - 1), 0);

		if (nSelection == nDest)
			continue;

		// move the actual anim in the model
		temp = m_pModel->m_Anims[nSelection];
		if (nDest < nSelection)
		{
			for (j=nSelection; j > nDest; j--)
				m_pModel->m_Anims[j] = m_pModel->m_Anims[j - 1];
		}
		else
		{
			for (j=nSelection; j < nDest; j++)
				m_pModel->m_Anims[j] = m_pModel->m_Anims[j + 1];
		}
		m_pModel->m_Anims[nDest] = temp;

		// Move the item in the list
		strSelection = m_AnimList.GetItemText(nSelection, 0);
		bChecked = m_AnimList.GetCheck(nSelection);
		m_AnimList.DeleteItem(nSelection);
		m_AnimList.InsertItem(nDest, strSelection);
		m_AnimList.SetItemState(nDest, LVIS_SELECTED, LVIS_SELECTED);
		m_AnimList.SetCheck(nDest, bChecked);

		m_bChangesMade = TRUE;
	}
}


// talon: 2001's (0x41a2e9): moves the selected pieces up or down the piece
// list.  The name is ours.
void CModelEditDlg::MovePieces(int dir)
{
	CString str;
	int i, j, nItems, items[128], iSrc, iDest;

	if (m_pModel)
	{
		nItems = 0;
		for (i=0; i < m_PieceList.GetItemCount(); i++)
		{
			if (nItems >= 128)
				break;

			if (m_PieceList.GetItemState(i, LVIS_SELECTED))
				items[nItems++] = i;
		}

		if (nItems > 0)
		{
			for (i=0; i < nItems; i++)
			{
				iSrc = items[i];
				iDest = iSrc + dir;
				if (iDest >= 0 && iDest < (int)m_pModel->NumPieces())
				{
					str = m_PieceList.GetItemText(iSrc, 0);
					m_PieceList.DeleteItem(iSrc);
					m_PieceList.InsertItem(iDest, str);

					for (j=i+1; j < nItems; j++)
					{
						if (items[j] >= iDest)
							break;

						items[j]--;
					}

					m_PieceList.SetItemState(iDest, LVIS_SELECTED, LVIS_SELECTED);
					m_pModel->SwapPieces(iSrc, iDest);
				}
			}

			m_bChangesMade = TRUE;
		}
	}
}

void CModelEditDlg::OnAnimMoveDown() 
{
	if (!m_bMovePieces)
		MoveAnims(1);
	else
		MovePieces(1);
}

void CModelEditDlg::OnAnimMoveUp() 
{
	if (!m_bMovePieces)
		MoveAnims(-1);
	else
		MovePieces(-1);
}

void CModelEditDlg::OnAnimDelete() 
{
	int i, nItems, items[1024], iTemp;

	if (!m_pModel) 
		return;

	if (!GetAnimList(items, sizeof(items), nItems))
		return;

	// Sort in descending order.
	for (i=0; i < (nItems-1); i++)
	{
		if (items[i] < items[i+1])
		{
			iTemp = items[i];
			items[i] = items[i+1];
			items[i+1] = iTemp;

			if (i == 0)
				i -= 1;
			else
				i -= 2;
		}
	}

	// Make sure we're not deleting them all.
	if (nItems == (int)m_pModel->m_Anims.GetSize())
	{
		MessageBox ("Cannot remove all animations from a model file.", "Delete Animation", MB_OK | MB_ICONINFORMATION);
		return;
	}

	// make sure they really want to delete it

	int nAnswer = MessageBox ("Are you sure you want to remove these animations from the model file?", "Delete Animation", MB_YESNO | MB_ICONQUESTION);
	if (nAnswer == IDNO)
	{
		return;
	}

	for (i=0; i < nItems; i++)
	{
		m_AnimList.DeleteItem(items[i]);
		// Delete the animation
		ModelAnim *pAnim = m_pModel->GetAnim(items[i]);
		if (pAnim)
			delete pAnim;
		// Remove it from the list
		m_pModel->m_Anims.Remove(items[i]);
		// Update the child models
		for (DWORD j=0; j < m_pModel->NumChildModels(); j++)
		{
			ChildInfo *pChild = m_pModel->GetChildModel(j);
			if ((DWORD)items[i] < pChild->m_AnimOffset)
				pChild->m_AnimOffset--;
		}
	}

	SetListColumnWidth(&m_AnimList, 0, -1);

	// Select the first item.
	m_AnimList.SetItemState(0, LVIS_SELECTED, LVIS_SELECTED);
	for (i = 1; i < m_AnimList.GetItemCount(); i++)
		m_AnimList.SetItemState(i, 0, LVIS_SELECTED);

	ResetAnimInfos();
	
	SetChangesMade();
}



void CModelEditDlg::OnCreateSingleFrame() 
{
	DWORD nAnimation = m_AnimInfos[0].CurAnimIndex();
	DWORD nKeyframe = m_AnimInfos[0].CurKeyframe();
	DWORD i;
	char testName[100];
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	


	if (!m_pModel)
		return;


	StopAllPlayback();


	// get a name for the new model

	CRenameDlg dlg (this);

	// Get a default name.
	for (i=0; i < 1000; i++)
	{
		if (i == 0)
			sprintf(testName, "static_model");
		else
			sprintf(testName, "static_model%d", i);
		
		if (!m_pModel->FindAnim(testName))
		{
			dlg.m_strName = testName;
			break;
		}
	}

	if (dlg.DoModal() == IDCANCEL)
	{
		return;
	}


	// Make a copy of the current anim and strip out all keyframes but the one we want.
	ModelAnim *pAnim;
	ModelAnim *pCopyFrom = m_pModel->GetAnim(nAnimation);
	ChildInfo *pChildInfo = m_pModel->GetAnimInfo(nAnimation)->m_pChildInfo;

	pAnim = new ModelAnim(m_pModel);
	if (!pAnim)
		return;

	if(!pAnim->CopyAnim(pCopyFrom))
		return;
	
	AnimInfo* pNewInfo = m_pModel->AddAnim(pAnim, pChildInfo);

	if (!pNewInfo)
		return;
	
	// Set its name.
	pAnim->SetName((char*)(LPCTSTR)dlg.m_strName);

	//copy over the dimensions
	pNewInfo->m_vDims = m_pModel->GetAnimInfo(nAnimation)->m_vDims;

	// Remove the keyframes leading up.
	for (i=0; i < nKeyframe; i++)
	{
		pAnim->RemoveKeyFrame(0);
	}

	// Copy the first keyframe....

	ModelAnim *pFrame = pAnim->CopyKeyFrame(0);
	pAnim->PasteKeyFrame(pFrame, 1);

	// Remove the ones after.
	while(pAnim->NumKeyFrames() > 2)
	{
		pAnim->RemoveKeyFrame(pAnim->NumKeyFrames() - 1);
	}
	pAnim->m_KeyFrames[0].m_Time = 0;
	pAnim->m_KeyFrames[1].m_Time = 200;

	// add the new anim to the list box and select it
	InitTaggedArrays();

	FillAnimList();
	for (i = 0; i < (DWORD)m_AnimList.GetItemCount(); i++)
		m_AnimList.SetItemState((int)i, 0, LVIS_SELECTED);
	m_AnimList.SetItemState(m_pModel->m_Anims - 1, LVIS_SELECTED, LVIS_SELECTED);

	OnSelChangeAnimList();

	SetChangesMade();
}


void CModelEditDlg::DoRenameAnim(DWORD iAnim)
{
	ModelAnim *pAnim;
	char *pNewName;
	CString str;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	
	if (!m_pModel || 
		iAnim >= m_pModel->NumAnims() ||
		!m_pModel->GetAnimInfo(iAnim)->IsParentModel())
		return;										 

	pAnim = m_pModel->GetAnim(iAnim);

	// make sure the animation is stopped

	OnStop();

	// get the new name

	CRenameDlg dlg (this);
	
	while(1)
	{
		dlg.m_strName = pAnim->GetName();
		if (dlg.DoModal() == IDCANCEL)
		{
			return;
		}

		pNewName = (char*)(LPCTSTR)dlg.m_strName;
		if (m_pModel->FindAnim(pNewName))
		{
			str.FormatMessage(IDS_ENTER_UNIQUE_ANIM_NAME, pNewName);
			if (DoMessageBox(str, MB_YESNO) == IDNO)
			{
				return;
			}
		}
		else
		{
			break;
		}
	}

	// set the new name in the animation
	pAnim->SetName((char*)(LPCTSTR)dlg.m_strName);

	// set the new name in the listbox

	m_AnimList.SetItemText(iAnim, 0, pAnim->GetName());

	SetChangesMade();
}


void CModelEditDlg::OnRenameAnim() 
{
	DoRenameAnim(m_AnimInfos[0].CurAnimIndex());
}

void CModelEditDlg::OnContinuous() 
{
	CContinuousDlg dlg;
	ModelAnim *pCurAnim, *pFrame;
	DWORD nKeyframes, oldLastTime;
	DWORD iAnim;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	iAnim = m_AnimInfos[0].CurAnimIndex();
	if (!m_pModel || 
		iAnim >= m_pModel->NumAnims() ||
		!m_pModel->GetAnimInfo(iAnim)->IsParentModel())
		return;

	// make sure the animation is stopped
	StopAllPlayback();

	// get the delay
	if (dlg.DoModal() == IDCANCEL) 
		return;

	pCurAnim = m_pModel->GetAnim(iAnim);
	if (pCurAnim->m_KeyFrames == 0)
		return;

	nKeyframes = pCurAnim->NumKeyFrames();
	oldLastTime = pCurAnim->m_KeyFrames.Last().m_Time;

	// Copy and paste it..
	pFrame = pCurAnim->CopyKeyFrame(0);
	if (!pFrame)
		return;

	if (!pCurAnim->PasteKeyFrame(pFrame, pCurAnim->NumKeyFrames()))
	{
		delete pFrame;
		return;
	}

	delete pFrame;

	// Adjust its time.
	pCurAnim->m_KeyFrames.Last().m_Time = oldLastTime + dlg.m_nDelay;

	// now add an extra tagged array member
	InitTaggedArrays();

	// update the ui
	ResetAnimInfos();
	OnSelChangeAnimList();
	UpdateEditFrameTime();

	m_bChangesMade = TRUE;
}

void CModelEditDlg::OnDrawSkeleton() 
{
	m_RenderWnd.m_DrawStruct.m_bDrawSkeleton = !m_RenderWnd.m_DrawStruct.m_bDrawSkeleton;	
	UpdateMenuChecks();
}

void CModelEditDlg::OnWireframe() 
{
	 m_RenderWnd.m_DrawStruct.m_bWireframe = !m_RenderWnd.m_DrawStruct.m_bWireframe;
	UpdateMenuChecks();
}

void CModelEditDlg::OnCameraFollow() 
{
	  m_RenderWnd.m_bCameraFollow = !m_RenderWnd.m_bCameraFollow;
	UpdateMenuChecks();
}

void CModelEditDlg::OnDims() 
{
	 m_RenderWnd.m_DrawStruct.m_bDims = !m_RenderWnd.m_DrawStruct.m_bDims;
	UpdateMenuChecks();
}

void CModelEditDlg::OnAnimBox() 
{
	 m_RenderWnd.m_DrawStruct.m_bAnimBox = !m_RenderWnd.m_DrawStruct.m_bAnimBox;
	UpdateMenuChecks();
}


void CModelEditDlg::OnShowSockets() 
{
	 m_RenderWnd.m_DrawStruct.m_bDrawSockets = !m_RenderWnd.m_DrawStruct.m_bDrawSockets;
	UpdateMenuChecks();
}


void CModelEditDlg::OnShowAttachments() 
{
	 m_RenderWnd.m_DrawStruct.m_bDrawAttachments = !m_RenderWnd.m_DrawStruct.m_bDrawAttachments;
	UpdateMenuChecks();
}


void CModelEditDlg::OnSolidAttachments() 
{
	 m_RenderWnd.m_DrawStruct.m_bSolidAttachments = !m_RenderWnd.m_DrawStruct.m_bSolidAttachments;
	UpdateMenuChecks();
}


void CModelEditDlg::OnShowNormals()
{
	m_RenderWnd.m_DrawStruct.m_bNormals = !m_RenderWnd.m_DrawStruct.m_bNormals;
	UpdateMenuChecks();
}


void CModelEditDlg::OnShowNormalRef()
{
	 m_RenderWnd.m_DrawStruct.m_bShowNormalRef = !m_RenderWnd.m_DrawStruct.m_bShowNormalRef;
	UpdateMenuChecks();
}


void CModelEditDlg::OnProfile()
{
	 m_RenderWnd.m_DrawStruct.m_bProfile = !m_RenderWnd.m_DrawStruct.m_bProfile;
	UpdateMenuChecks();
}


void CModelEditDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	CScrollBar *pItem;
	int pos;

	if (!m_pModel)
		return;

	if (nSBCode != SB_ENDSCROLL)
	{
		pos = pScrollBar->GetScrollPos();
		
		pItem = (CScrollBar*)GetDlgItem(IDC_LOD_SCROLL);
		if (pItem && (pScrollBar==pItem) && m_pModel)
		{
			if (nSBCode == SB_PAGELEFT || nSBCode == SB_LINELEFT)
			{
				pos = CLAMP(pScrollBar->GetScrollPos() - 1, 0, (int)(m_pModel->NumLODs() - 1));
			}
			else if (nSBCode == SB_PAGERIGHT || nSBCode == SB_LINERIGHT)
			{
				pos = CLAMP(pScrollBar->GetScrollPos() + 1, 0, (int)(m_pModel->NumLODs() - 1));
			}
			else
			{
				pos = nPos;
			}

			pScrollBar->SetScrollPos(pos);

			if ((DWORD)pos != m_nCurrentLOD)
			{
				SetCurrentLOD(pos);
				SetLODText(m_nCurrentLOD);
				m_RenderWnd.Draw();
			}
		}
	}
	
	CDialog::OnHScroll(nSBCode, nPos, pScrollBar);
}


void CModelEditDlg::SetLODText(int nLOD)
{
	char str[256];
	CWnd *pWnd;

	sprintf(str, "LOD: %d", nLOD);
	if (pWnd = GetDlgItem(IDC_LOD_TEXT))
		pWnd->SetWindowText(str);
}


void CModelEditDlg::OnBuildLODs() 
{
	CString str;
	CGenLodDlg dlg;
	BuildLODRequest request;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	LODRequestInfo info;
	char oldText[256];
	DWORD i;

	if (!m_pModel)
		return;

	request.m_pModel = m_pModel;
	dlg.m_pRequest = &request;

	request.m_PieceWeights.SetSizeInit2(m_pModel->NumPieces(), 1.0f);
	for (i=0; i < m_pModel->NumPieces(); i++)
	{
		request.m_PieceWeights[i] = m_pModel->GetPiece(i)->m_LODWeight;
	}

	for (i=0; i < m_pModel->NumLODs(); i++)
	{
		if (i)
		{
			info.m_Dist = GetModelLODInfo(m_pModel, i)->m_Dist;
			info.m_nTris = m_pModel->CalcNumTris(i);
			request.m_LODInfos.Append(info);
		}
	}

	if (dlg.DoModal() == IDOK)
	{
		GetWindowText(oldText, sizeof(oldText));
		SetWindowText("Building LODs");

		for (i=0; i < m_pModel->NumPieces(); i++)
		{
			m_pModel->GetPiece(i)->m_LODWeight = request.GetPieceWeight(i);
		}

		if (!BuildLODs(&request))
		{
			MessageBox("Unable to build LOD's (% too small?)");
		}

		SetWindowText(oldText);
		SetCurrentLOD(0);
		InitTheScrollBar(m_pModel->NumLODs() - 1);
		m_bChangesMade = TRUE;
	}
}




void CModelEditDlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) 
{
	CDialog::OnKeyDown(nChar, nRepCnt, nFlags);
}


void CModelEditDlg::OnModelInfo()
{
	CModelInfoDlg dlg;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	if (m_pModel)
	{
		dlg.m_pModel = m_pModel;
		dlg.DoModal();
	}
}



void CModelEditDlg::OnCommandString() 
{
	CCommandStringDlg dlg;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	if (m_pModel)
	{
		dlg.m_pModel = m_pModel;
		dlg.DoModal();
		ProcessCommandString();
		SetChangesMade();
	}
}

void CModelEditDlg::OnDimensions() 
{
	CDimensionsDlg dlg;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	if (m_pModel && GetAnimList(dlg.m_Anims, sizeof(dlg.m_Anims), dlg.m_nAnims, FALSE))
	{
		dlg.m_pModel = m_pModel;
		dlg.DoModal();
	}
}

//this will run through each animation in the model, looking for animations that have the X and Z
//dimensions of the animation not equal. It will then prompt them for how to fix.


void CModelEditDlg::OnAnimationFramerate() 
{
	CAnimFramerateDlg dlg;
	char str[20];
	DWORD i, iAnim;
	float theRate;
	int iItem, nSelected;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	if (m_pModel)
	{
		sprintf(str, "%d", 10);
		dlg.m_FramerateString = str;
		dlg.m_bAllAnimations = FALSE;
		
		if (dlg.DoModal() == IDOK)
		{
			// Grab the currently playing animation index
			iAnim = m_AnimInfos[0].CurAnimIndex();

			// Stop playing
			ResetAnimInfos();

			theRate = (float)atof(dlg.m_FramerateString);
			
			if (dlg.m_bAllAnimations)
			{
				for (i=0; i < m_pModel->m_Anims; i++)
				{
					model_SetAnimFramerate(m_pModel, i, (float)(1.0f / theRate));
				}
			}
			else
			{
				// talon: 2001 sets the selected animations' rate, or the
				// current one's when none is selected.
				nSelected = 0;
				for (iItem=0; iItem < m_AnimList.GetItemCount(); iItem++)
				{
					if (m_AnimList.GetItemState(iItem, LVIS_SELECTED) && 
						m_pModel->GetAnimInfo(iItem)->IsParentModel())
					{
						model_SetAnimFramerate(m_pModel, iItem, (float)(1.0f / theRate));
						nSelected++;
					}
				}

				if (nSelected < 1)
				{
					if (iAnim < m_pModel->m_Anims && 
						m_pModel->GetAnimInfo(iAnim)->IsParentModel())
					{
						model_SetAnimFramerate(m_pModel, iAnim, (float)(1.0f / theRate));
					}
				}
			}
		
			SetChangesMade();

			// Update the animation list
			OnSelChangeAnimList();
		}
	}
}


// ------------------------------------------------------------------------
// talon: 2001's Animation Cleanup (Animation > Remove extra frames), from
// the image (0x41b413..0x41bad3).  The names are ours.
// ------------------------------------------------------------------------

// The error at keyframe iKey of pNode and its children if the keyframe
// were removed (interpolated at t between its neighbours instead).
static float CalcRemovalError(BOOL bRotation, DWORD iKey, float t, AnimNode *pNode,
	LTMatrix *pParent, LTMatrix *pInterpParent)
{
	LTMatrix mat, global, interpGlobal;
	LTRotation rot, interpRot;
	LTVector scaled, interpPos, diff, pos, interp;
	NodeKeyFrame *pPrev, *pNext;
	float error, dot;
	DWORD i;

	pNode->m_KeyFrames[iKey].ConvertToMatrix(mat);
	MatMul(&global, pParent, &mat);
	pos.x = global.m[0][3];
	pos.y = global.m[1][3];
	pos.z = global.m[2][3];
	quat_ConvertFromMatrix(rot.m_Quat, global.m);

	pPrev = &pNode->m_KeyFrames[iKey-1];
	pNext = &pNode->m_KeyFrames[iKey+1];
	quat_Slerp(interpRot.m_Quat, pPrev->m_Quaternion.m_Quat, pNext->m_Quaternion.m_Quat, t);

	interp = pPrev->m_Translation;
	interp.Init(pNext->m_Translation.x - interp.x, pNext->m_Translation.y - interp.y, pNext->m_Translation.z - interp.z);
	scaled.Init(interp.x * t, interp.y * t, interp.z * t);
	interpPos.Init(scaled.x + pPrev->m_Translation.x, scaled.y + pPrev->m_Translation.y, scaled.z + pPrev->m_Translation.z);

	quat_ConvertToMatrix(interpRot.m_Quat, mat.m);
	mat.SetTranslation(interpPos.x, interpPos.y, interpPos.z);
	MatMul(&interpGlobal, pInterpParent, &mat);
	interp.x = interpGlobal.m[0][3];
	interp.y = interpGlobal.m[1][3];
	interp.z = interpGlobal.m[2][3];
	quat_ConvertFromMatrix(interpRot.m_Quat, interpGlobal.m);

	diff.Init(pos.x - interp.x, pos.y - interp.y, pos.z - interp.z);
	error = diff.Mag();

	if (bRotation)
	{
		dot = rot.m_Quat[1]*interpRot.m_Quat[1] + rot.m_Quat[2]*interpRot.m_Quat[2] + 
			rot.m_Quat[3]*interpRot.m_Quat[3] + interpRot.m_Quat[0]*rot.m_Quat[0];
		if (dot <= 0.0001f)
			dot = 0.0001f;

		error = (1.0f - dot + error) / dot;
	}

	for (i=0; i < pNode->NumChildren(); i++)
	{
		error += CalcRemovalError(bRotation, iKey, t, pNode->GetChild(i), &global, &interpGlobal);
	}

	return error;
}


// Where keyframe iKey sits between its neighbours.
// talon: RETAIL BUG: the divisor is prev - next, so this is negative.
static float GetKeyframePercent(ModelAnim *pAnim, DWORD iKey)
{
	AnimKeyFrame *pKey = &pAnim->m_KeyFrames[iKey];
	DWORD prevTime = pAnim->m_KeyFrames[iKey-1].m_Time;
	DWORD nextTime = pAnim->m_KeyFrames[iKey+1].m_Time;

	if (prevTime == nextTime)
		return 0.0f;

	return (float)(pKey->m_Time - prevTime) / (float)(prevTime - nextTime);
}


// Finds the keyframe of pAnim whose removal costs the most error under
// maxVariance.  Fills in *pVariance with the error (or the least error
// of any keyframe, or -1, when none qualifies).
static BOOL FindRemovableKeyframe(ModelAnim *pAnim, float maxVariance, BOOL bRotation,
	DWORD *piKey, float *pVariance)
{
	LTMatrix ident;
	BOOL bFound;
	float minVariance, variance;
	DWORD i;

	if (pAnim->NumKeyframes() < 2)
		return FALSE;

	bFound = FALSE;
	minVariance = 1000000.0f;
	*pVariance = 0.0f;
	for (i=1; i < pAnim->NumKeyframes()-1; i++)
	{
		ident.Identity();
		variance = CalcRemovalError(bRotation, i, GetKeyframePercent(pAnim, i), pAnim->GetRootNode(), &ident, &ident);

		if (variance <= minVariance)
			minVariance = variance;

		if (variance > *pVariance && variance < maxVariance)
		{
			bFound = TRUE;
			*piKey = i;
			*pVariance = variance;
		}
	}

	if (!bFound)
	{
		if (pAnim->NumKeyframes() > 2)
			*pVariance = minVariance;
		else
			*pVariance = -1.0f;
	}

	return bFound;
}


void CModelEditDlg::OnAnimationCleanup() 
{
	char str[12], msg[256];
	DWORD i, j, iKey;
	ModelAnim *pAnim;
	CKeyframeWnd *pWnd;
	char *pString;
	float maxVariance, curVariance, variance, nextVariance;
	BOOL bRotation;
	int nRemoved;

	if (!m_pModel)
		return;

	CAnimCleanupDlg dlg;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	for (i=0; i < NUM_ANIM_INFOS; i++)
		m_AnimInfos[i].ResetTime();

	dlg.m_bPreserveRot = FALSE;
	sprintf(str, "%f", 0.1);
	dlg.m_VarianceString = str;

	if (dlg.DoModal() != IDOK)
		return;

	maxVariance = (float)atof(dlg.m_VarianceString);
	bRotation = dlg.m_bPreserveRot;
	nRemoved = 0;
	nextVariance = -1.0f;

	BeginWaitCursor();

	for (i=0; i < (DWORD)m_AnimList.GetItemCount(); i++)
	{
		pWnd = NULL;

		if (m_AnimList.GetItemState(i, LVIS_SELECTED) && 
			m_pModel->GetAnimInfo(i)->IsParentModel())
		{
			pAnim = m_pModel->GetAnimInfo(i)->m_pAnim;

			for (j=0; j < NUM_ANIM_INFOS; j++)
			{
				if (GetAnimInfo(j)->CurAnim() == pAnim)
				{
					pWnd = &GetAnimInfo(j)->m_Wnd;
					break;
				}
			}

			curVariance = maxVariance;
			if (FindRemovableKeyframe(pAnim, maxVariance, bRotation, &iKey, &variance))
			{
				do
				{
					pString = pAnim->m_KeyFrames[iKey].m_pString;
					if (!pString || !pString[0])
					{
						if (pWnd && iKey < pWnd->m_Tagged.GetSize())
						{
							if (pWnd->m_Tagged[iKey])
								goto Tagged;

							pWnd->m_Tagged.Remove(iKey);
						}

						pAnim->RemoveKeyFrame(iKey);
						nRemoved++;
					}
					else
					{
Tagged:;
						curVariance = variance;
					}
				}
				while (FindRemovableKeyframe(pAnim, curVariance, bRotation, &iKey, &variance));
			}

			if (nextVariance <= 0.0f || variance <= maxVariance)
				nextVariance = variance;
			else if (variance <= nextVariance)
				nextVariance = variance;
		}
	}

	EndWaitCursor();

	if (nextVariance >= 0.0f)
	{
		sprintf(msg, "Removed %d keys.  Next removal at variance of %0.2f.", nRemoved, nextVariance);
	}
	else
	{
		if (!nRemoved)
		{
			MessageBox("No applicable animations selected.", "Try again", MB_OK|MB_ICONINFORMATION);
			return;
		}

		sprintf(msg, "Removed %d keys.  No more available keys found.", nRemoved);
	}

	MessageBox(msg, "Hooray!", MB_OK|MB_ICONINFORMATION);
	if (nRemoved)
	{
		m_bChangesMade = TRUE;
		ResetAnimInfos();
		OnSelChangeAnimList();
	}
}

void CModelEditDlg::OnAnimationLength() 
{
	CKeyframeTimeDlg dlg;
	DWORD i, nKeyframes, dwNewKeyframeTime, iAnim;
	float fPosition;
	ModelAnim *pAnim;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	
	iAnim = m_AnimInfos[0].CurAnimIndex();	
	if(!m_pModel ||
		iAnim >= m_pModel->NumAnims() ||
		!m_pModel->GetAnimInfo(iAnim)->IsParentModel())
	{
		return;
	}

	pAnim = m_pModel->GetAnim(iAnim);
	nKeyframes = pAnim->m_KeyFrames.GetSize();
	dlg.m_nCurrentTime = pAnim->GetAnimTime();
	dlg.m_nNewTime = dlg.m_nCurrentTime;
	dlg.m_sCaption = "Animation Length";
	
	if (dlg.DoModal() == IDCANCEL || dlg.m_nNewTime == dlg.m_nCurrentTime)
	{
		return;
	}

	// Do all the frames except first and last...
	for ( i = 1; i < nKeyframes - 1; i++ )
	{
		fPosition = ( float )pAnim->m_KeyFrames[i].m_Time / ( float )pAnim->GetAnimTime();
		dwNewKeyframeTime = ( DWORD )((( float )dlg.m_nNewTime * fPosition ) + 0.5f );
		pAnim->m_KeyFrames[i].m_Time = dwNewKeyframeTime;
	}

	// Make sure the last frame is exactly the full time...
	pAnim->m_KeyFrames[nKeyframes - 1].m_Time = dlg.m_nNewTime;
	
	SetChangesMade();
}

void CModelEditDlg::OnAnimationInterpolation() 
{
	CKeyframeTimeDlg dlg;
	DWORD iAnim;
	int i;
	ModelAnim *pAnim;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	
	iAnim = m_AnimInfos[0].CurAnimIndex();	
	if(!m_pModel ||
		iAnim >= m_pModel->NumAnims() ||
		!m_pModel->GetAnimInfo(iAnim)->IsParentModel())
	{
		return;
	}

	pAnim = m_pModel->GetAnim(iAnim);

	dlg.m_nCurrentTime = pAnim->m_InterpolationMS;
	dlg.m_nNewTime = pAnim->m_InterpolationMS;
	dlg.m_sCaption = "Animation Interpolation";
	
	if (dlg.DoModal() == IDCANCEL || dlg.m_nNewTime == dlg.m_nCurrentTime)
	{
		return;
	}

	// talon: 2001 sets it on every selected animation of this model.
	for (i=0; i < m_AnimList.GetItemCount(); i++)
	{
		if (m_AnimList.GetItemState(i, LVIS_SELECTED) && 
			m_pModel->GetAnimInfo(i)->IsParentModel())
		{
			m_pModel->GetAnim(i)->m_InterpolationMS = dlg.m_nNewTime;
		}
	}

	SetChangesMade();
}

void CModelEditDlg::UpdateEditFrameTime( )
{
	MEAnimInfo *pInfo;
	
	if(!m_pModel)
	{
		m_EditFrameTime.EnableWindow(FALSE);
		return;
	}

	// The edit frame string can only be changed from anim 0.
	pInfo = GetAnimInfo(ANIMINFO_MAIN);
	
	TempVarSetter<BOOL> setter(&m_bInStringChange, TRUE);
	SetDlgItemInt( IDC_FRAMETIME, pInfo->CurAnimTime(), FALSE );
	m_EditFrameTime.EnableWindow(TRUE);
}

void CModelEditDlg::UpdateEditFrameString( )
{
	DWORD iAnim, iKeyframe;
	char *pStr;
	BOOL bEnabled;

	if(!GetEditAnimInfo(&iAnim, &iKeyframe))
		return;

	pStr = m_pModel->GetAnim(iAnim)->m_KeyFrames[iKeyframe].m_pString;
	bEnabled = FALSE;
	
	TempVarSetter<BOOL> setter(&m_bInStringChange, TRUE);
	SetDlgItemText( IDC_FRAMESTRING, pStr );

	if (!m_AnimInfos[ANIMINFO_MAIN].IsAnimPlaying())
		bEnabled = TRUE;

	m_EditFrameString.EnableWindow(bEnabled);
}

void CModelEditDlg::SetEditFrameTime() 
{
	DWORD dwNewKeyframeTime = GetDlgItemInt( IDC_FRAMETIME, NULL, FALSE );
	DWORD nKeyframe = m_AnimInfos[ANIMINFO_MAIN].m_Wnd.ForceNearestKeyframe();
	m_AnimInfos[ANIMINFO_MAIN].m_Wnd.DoEditKeyframeTime( nKeyframe, dwNewKeyframeTime );
	UpdateEditFrameTime( );
}


void CModelEditDlg::SetEditFrameString()
{
	CString sFrameString;
	DWORD nKeyframe;

	GetDlgItemText( IDC_FRAMESTRING, sFrameString );
	nKeyframe = m_AnimInfos[ANIMINFO_MAIN].m_Wnd.ForceNearestKeyframe();
	m_AnimInfos[ANIMINFO_MAIN].m_Wnd.DoEditKeyframeString( nKeyframe, sFrameString );
}


BOOL CModelEditDlg::SetRenderTexture(const char *pFilename, DWORD nTexture)
{
	ILTStream *pStream;
	TextureData *pTexture;
	BOOL bRet;


	bRet = FALSE;
	pStream = streamsim_Open(pFilename, "rb");
	if (pStream)
	{
		if (dtx_Create(pStream, &pTexture, FALSE) == LT_OK)
		{
			bRet = SetGLMTexture(m_RenderWnd.GetContext(), pTexture, nTexture);
			dtx_Destroy(pTexture);
		}

		pStream->Release();
	}

	return bRet;
}


BOOL CModelEditDlg::CheckModifyAnims(int *pAnims, int nAnims, BOOL bParentOnly, BOOL bMessage)
{
	int i, iAnim;
	CString str;
	AnimInfo *pAnimInfo;

	if (!m_pModel)
		return FALSE;

	for (i=0; i < nAnims; i++)
	{
		iAnim = pAnims[i];

		if (iAnim < 0 || iAnim >= (int)m_pModel->NumAnims())
		{
			if (bMessage)
			{
				str.FormatMessage(IDS_INVALIDANIMINDEX);
				DoMessageBox(str, MB_OK);
			}

			return FALSE;
		}

		if (bParentOnly)
		{
			pAnimInfo = m_pModel->GetAnimInfo(iAnim);
			if (pAnimInfo->GetAnimOwner() != m_pModel)
			{
				if (bMessage)
				{
					str.FormatMessage(IDS_CANTEDITBASEMODEL, pAnimInfo->m_pAnim->GetName());
					DoMessageBox(str, MB_OK);
				}

				return FALSE;
			}
		}
	}

	return TRUE;
}


BOOL CModelEditDlg::GetAnimList(int *pAnims, int listSizeBytes, int &nAnims, BOOL bParentOnly, BOOL bMessage)
{
	if ((!m_pModel) || (!pAnims))
		return FALSE;

	nAnims = 0;
	int listSize = listSizeBytes / sizeof(int);
	for (int i = 0; (i < m_AnimList.GetItemCount()) && (nAnims < listSize); i++)
	{
		if (m_AnimList.GetItemState(i, LVIS_SELECTED) != 0)
		{
			pAnims[nAnims] = i;
			nAnims++;
		}
	}
	if (nAnims == 0)
		return FALSE;

	return CheckModifyAnims(pAnims, nAnims, bParentOnly, bMessage);
}


void CModelEditDlg::ProcessCommandString()
{
	ConParse parse;
	ModelLoadRequest request;
	ModelSocket *pSocket;
	DWORD nTexture = 0;
	DDWORD nToken = 1;

	if (!m_pModel)
		return;
	
	// Set its skins.
	parse.Init(m_pModel->m_CommandString);
	while (parse.ParseFind("ModelEditTexture", FALSE, nToken))
	{
		if (parse.m_nArgs > 2)
			nTexture = atoi(parse.m_Args[2]);
		else
			nTexture = 0;
		SetRenderTexture(parse.m_Args[1], nTexture);
		nToken++;
	}

	// Setup socket attachment models.
	DeleteSocketAttachments();

	CRegMgr regMgr;
	regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Attachments", HKEY_CURRENT_USER);

	parse.Init(m_pModel->m_CommandString);
	while(parse.ParseFind("SocketModel", FALSE, 2))
	{
		if (pSocket = m_pModel->FindSocket(parse.m_Args[1]))
		{
			char *filename = parse.m_Args[2];
			
			if( IsFilenameLTAExt( filename ) )
			{
				Model *pModel = load_LTA_Using_FeedBackDialog( filename, false  );
				if( pModel == NULL )
					continue;

				pSocket->m_pAttachment = pModel ;
			}
			else
			{
				if (!(request.m_pFile = streamsim_Open(parse.m_Args[2], "rb")))
					continue;

				pSocket->m_pAttachment = new Model;
				pSocket->m_pAttachment->Load(&request);
				request.m_pFile->Release();
			}

			// Save this attachment in the registry
			regMgr.Set(pSocket->GetName(), parse.m_Args[2]);
		}
	}
}


void CModelEditDlg::OffsetTrans(DVector vec)
{
	int i;

	if (!m_pModel)
		return;

	for (i=0; (i < m_AnimList.GetItemCount()) && (i < (int)m_pModel->m_Anims.GetSize()); i++)
	{
		if (m_AnimList.GetItemState(i, LVIS_SELECTED) == 0)
			continue;
		m_pModel->GetAnimInfo(i)->m_vTranslation += vec;
	}

	SetChangesMade();
}


void CModelEditDlg::OffsetDims(DVector vec)
{
	int i;

	for (i=0; (i < m_AnimList.GetItemCount()) && (i < (int)m_pModel->m_Anims.GetSize()); i++)
	{
		if (m_AnimList.GetItemState(i, LVIS_SELECTED) == 0)
			continue;
		m_pModel->GetAnimInfo(i)->m_vDims += vec;
	}

	SetChangesMade();
}


void CModelEditDlg::DoAnimEdit(DVector vec)
{
	if (IsButtonSelected(IDC_DIMS_EDIT))
		OffsetDims(vec);
	else
		OffsetTrans(vec);
}


void CModelEditDlg::OnTranslationButton() 
{
	TranslationDlg dlg;
	char tempStr[32];
	ModelAnim *pAnim;
	AnimInfo *pAnimInfo;
	NodeKeyFrame *pKeyframe;
	DVector newTrans, offset;
	int items[1024], i, iAnim, nItems;
	DWORD iEditAnim;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	


	if(!GetEditAnimInfo(&iEditAnim, NULL))
		return;

	if (!GetAnimList(items, sizeof(items), nItems, FALSE))
		return;
		
	pAnim = m_pModel->GetAnim(iEditAnim);
	pAnimInfo = m_pModel->GetAnimInfo(iEditAnim);

	pKeyframe = &pAnim->GetRootNode()->m_KeyFrames[0];
	
	sprintf(tempStr, "%.3f", pAnimInfo->m_vTranslation.x);
	dlg.m_xTrans = tempStr;

	sprintf(tempStr, "%.3f", pAnimInfo->m_vTranslation.y);
	dlg.m_yTrans = tempStr;

	sprintf(tempStr, "%.3f", pAnimInfo->m_vTranslation.z);
	dlg.m_zTrans = tempStr;

	if (dlg.DoModal() == IDOK)
	{
		newTrans.x = (float)atof(dlg.m_xTrans);
		newTrans.y = (float)atof(dlg.m_yTrans);
		newTrans.z = (float)atof(dlg.m_zTrans);

		for (i=0; i < nItems; i++)
		{
			iAnim = items[i];

			pAnimInfo = m_pModel->GetAnimInfo(iAnim);
			pAnimInfo->m_vTranslation = newTrans;
		}
	}
}


void CModelEditDlg::OnRotationButton() 
{
	RotationDlg dlg;
	DVector vRot;
	int items[1024], i, iAnim, nItems;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	
	if (!m_pModel)
		return;

	if (!GetAnimList(items, sizeof(items), nItems))
		return;

	dlg.m_sRotX = dlg.m_sRotY = dlg.m_sRotZ = "0.0";

	if (dlg.DoModal() == IDOK)
	{
		vRot.x = MATH_DEGREES_TO_RADIANS(( float )atof( dlg.m_sRotX ));
		vRot.y = MATH_DEGREES_TO_RADIANS(( float )atof( dlg.m_sRotY ));
		vRot.z = MATH_DEGREES_TO_RADIANS(( float )atof( dlg.m_sRotZ ));

		for (i=0; i < nItems; i++)
		{
			iAnim = items[i];

			model_RotateAnim(m_pModel, iAnim, &vRot);
		}

		m_bChangesMade = TRUE;
	}
}


void CModelEditDlg::OnXsub() 
{
	DoAnimEdit(DVector(-1.0f, 0.0f, 0.0f));
}

void CModelEditDlg::OnXadd() 
{
	DoAnimEdit(DVector(1.0f, 0.0f, 0.0f));
}

void CModelEditDlg::OnYsub() 
{
	DoAnimEdit(DVector(0.0f, -1.0f, 0.0f));
}

void CModelEditDlg::OnYadd() 
{
	DoAnimEdit(DVector(0.0f, 1.0f, 0.0f));
}

void CModelEditDlg::OnZsub() 
{
	DoAnimEdit(DVector(0.0f, 0.0f, -1.0f));
}

void CModelEditDlg::OnZadd() 
{
	DoAnimEdit(DVector(0.0f, 0.0f, 1.0f));
}

void CModelEditDlg::OnNumberAnim() 
{
	int i, nItems, items[1024], iTemp, nSelection;
	DWORD insertAt, nTempAnims, nListItems;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	


	if (!m_pModel)
		return;

	if (!GetAnimList(items, sizeof(items), nItems))
		return;
	
	AnimNumberDlg dlg(this, m_pModel->m_Anims - nItems);

	if (dlg.DoModal() == IDOK)
	{	
		// Sort in descending order.
		for (i=0; i < (nItems-1); i++)
		{
			if (items[i] < items[i+1])
			{
				iTemp = items[i];
				items[i] = items[i+1];
				items[i+1] = iTemp;

				if (i == 0)
					i -= 1;
				else
					i -= 2;
			}
		}

		// Remove them all from the listbox.
		for (i=0; i < nItems; i++)
		{
			nSelection = items[i];

			if (nSelection < 0 || nSelection >= (int)m_pModel->m_Anims.GetSize())
				continue;

			m_AnimList.DeleteItem(nSelection);
		}
	
		for (i=0; i < m_AnimList.GetItemCount(); i++)
			m_AnimList.SetItemState(i, 0, LVIS_SELECTED);

		// Add them back in order.
		nListItems = m_pModel->m_Anims - nItems;
		insertAt = MIN(nListItems, (DWORD)dlg.m_MoveNumber);
		for (i=0; i < nItems; i++)
		{
			m_AnimList.InsertItem(insertAt, m_pModel->GetAnim(items[i])->GetName());
			m_AnimList.SetItemState(insertAt, LVIS_SELECTED, LVIS_SELECTED);
		}

		
		// Now rearrange the model's list.
		CMoArray<AnimInfo, NoCache> tempAnims;
		tempAnims.CopyArray(m_pModel->m_Anims);
		nTempAnims = m_pModel->m_Anims;

		for (i=0; i < nItems; i++)
		{
			m_pModel->m_Anims.Remove(items[i]);
		}

		insertAt = MIN(m_pModel->m_Anims.GetSize(), (DWORD)dlg.m_MoveNumber);
		for (i=0; i < nItems; i++)
		{
			m_pModel->m_Anims.Insert(insertAt, tempAnims[items[i]]);
		}
		

		// process the change
		StopAllPlayback();
		m_AnimInfos[ANIMINFO_MAIN].m_Tracker.m_TimeRef.Init(
			m_pModel,
			dlg.m_MoveNumber, 0,
			dlg.m_MoveNumber, 0);

		SetChangesMade();
	}
}


void CModelEditDlg::OnDuplicateAnim() 
{
	int nItems;
	DWORD i;
	ModelAnim *pNewAnim;
	char newName[300];
	AnimInfo *pSrcAnimInfo, *pAnimInfo;
	DVector srcDims;

	nItems = m_AnimList.GetItemCount();
	if (!nItems)
		return;

	// Add the duplicated ones.
	for (i=0; i < (DWORD)nItems; i++)
	{
		if (m_AnimList.GetItemState(i, LVIS_SELECTED) == 0)
			continue;

		pSrcAnimInfo = m_pModel->GetAnimInfo(i);
		srcDims = pSrcAnimInfo->m_vDims;

		sprintf(newName, "%s_copy", pSrcAnimInfo->m_pAnim->GetName());

		pNewAnim = new ModelAnim(m_pModel);
		if (!pNewAnim)
			return;

		if (!pNewAnim->CopyAnim(m_pModel->GetAnim(i)) || 
			!(pAnimInfo = m_pModel->AddAnimToSelf(pNewAnim)))
		{
			delete pNewAnim;
			return;
		}

		pAnimInfo->m_vDims = srcDims;

		pNewAnim->SetName(newName);

		// talon: 2001 selects the copy in place of the original.
		m_AnimList.InsertItem(m_pModel->m_Anims.GetSize() + i, pNewAnim->GetName());
		m_AnimList.SetItemState(m_pModel->m_Anims.GetSize() + i, LVIS_SELECTED, LVIS_SELECTED);
		m_AnimList.SetItemState(i, 0, LVIS_SELECTED);
	}

	InitTaggedArrays();
	FillAnimList();
	SetChangesMade();
}

// ------------------------------------------------------------------------
// FillAnimDataFromTransformNodeData()
// basically take the bind pose and put it in the animation hierarchy.
// ------------------------------------------------------------------------


// ------------------------------------------------------------------------
// create an animation from the bind pose 
// ------------------------------------------------------------------------

void CModelEditDlg::OnRenameNode() 
{
	RenameNodeDlg dlg;
	DWORD i;
	char msg[200];
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	if (!m_pModel)
		return;
	
	if (dlg.DoModal() == IDOK)
	{
		for (i=0; i < m_pModel->m_FlatNodeList; i++)
		{
			if (strcmp(m_pModel->m_FlatNodeList[i]->GetName(), dlg.m_OldName) == 0)
			{
				m_pModel->m_FlatNodeList[i]->SetName((char*)(LPCTSTR)dlg.m_NewName);

				sprintf(msg, "'%s' has been renamed to '%s'.", dlg.m_OldName, (char*)(LPCTSTR)dlg.m_NewName);
				MessageBox(msg, "ModelEdit", MB_OK);
				SetChangesMade();

				// Redo the node list.
				FillNodeList();
				return;
			}
		}
	
		sprintf(msg, "Couldn't find node: '%s'.", dlg.m_OldName);
		MessageBox(msg, "ModelEdit", MB_OK);
	}
}



void UnrotateNodes(Model *pModel, DWORD transformIndex, int *items, int nItems, int nLevels)
{
	int i;
	DWORD j, iAnim;
	ModelAnim *pAnim;
	ModelNode *pNode;


	pNode = pModel->m_FlatNodeList[transformIndex];
	for (i=0; i < nItems; i++)
	{
		iAnim = (DWORD)items[i];
		pAnim = pModel->GetAnim(iAnim);

		pModel->UnrotateAnimNode(pAnim, pAnim->m_AnimNodes[transformIndex]);
	}

	// Do the child nodes too?
	--nLevels;
	if (nLevels > 0)
	{
		for (j=0; j < pNode->m_Children; j++)
		{
			UnrotateNodes(pModel, pNode->m_Children[j]->GetNodeIndex(), items, nItems, nLevels);
		}
	}
}


void CModelEditDlg::OnUnrotateTopNode() 
{
	int status, nLevels, nItems, items[1024];
	char msg[256];


	if (!m_pModel || !GetAnimList(items, sizeof(items), nItems))
		return;

	nLevels = 1;
	if (nLevels > 1)
	{
		sprintf(msg, "There are %d levels of top-level nulls.  Unrotate them all?", nLevels);
		status = MessageBox(msg, "ModelEdit", MB_YESNOCANCEL);
		if (status == IDCANCEL)
			return;
		else if (status == IDNO)
			nLevels = 1;
	}

	if (nLevels == 0)
		nLevels = 1;

	UnrotateNodes(m_pModel, m_pModel->GetRootNode()->GetNodeIndex(), items, nItems, nLevels);
	SetChangesMade();
}


BOOL CModelEditDlg::PreTranslateMessage(MSG* pMsg) 
{
	TranslateAccelerator(m_hWnd, m_hAccel, pMsg);
	
	return CDialog::PreTranslateMessage(pMsg);
}


void CModelEditDlg::OnGenerateVertexNormals()
{
	if (!m_pModel)
		return;

	gn_BuildModelVertexNormals(m_pModel);
	SetChangesMade();
}


void CModelEditDlg::OnReverseAnimation()
{
	int items[1024], nItems, i, j;
	ModelAnim *pAnim, *pFrame1, *pFrame2;
	DWORD nKeyFrames, iKeyframe1, iKeyframe2, oldAnimTime;


	if (!m_pModel || !GetAnimList(items, sizeof(items), nItems))
		return;

	for (i=0; i < nItems; i++)
	{
		if (items[i] >= (int)m_pModel->m_Anims.GetSize())
			continue;

		pAnim = m_pModel->GetAnim(items[i]);
		oldAnimTime = pAnim->GetAnimTime();

		nKeyFrames = pAnim->m_KeyFrames.GetSize();
		for (j=0; j < (int)(nKeyFrames/2); j++)
		{
			iKeyframe1 = j;
			iKeyframe2 = nKeyFrames - j - 1;

			pFrame1 = pAnim->CopyKeyFrame(iKeyframe1);
			pFrame2 = pAnim->CopyKeyFrame(iKeyframe2);

			if (!pFrame1 || !pFrame2)
			{
				if (pFrame1)
					delete pFrame1;

				if (pFrame2)
					delete pFrame2;

				MessageBox("Error copying frames.", "ModelEdit", MB_OK);
				return;
			}
			
			if (!pAnim->PasteKeyFrame(pFrame2, iKeyframe1))
			{
				MessageBox("Error pasting keyframe.");
				return;
			}
			pAnim->RemoveKeyFrame(iKeyframe1+1);

			if (!pAnim->PasteKeyFrame(pFrame1, iKeyframe2))
			{
				MessageBox("Error pasting keyframe.");
				return;
			}
			pAnim->RemoveKeyFrame(iKeyframe2+1);

			delete pFrame1;
			delete pFrame2;
		}

		// Reverse the keyframe times.
		for (i=0; i < (int)pAnim->m_KeyFrames; i++)
		{
			pAnim->m_KeyFrames[i].m_Time = oldAnimTime - pAnim->m_KeyFrames[i].m_Time;
		}
	}
}


void CModelEditDlg::OnSelectNullNodes()
{
}


void CModelEditDlg::OnRemoveNode()
{
	int iIndex;
							 
	if (!m_pModel)
		return;

	iIndex = m_NodeList.GetSelectionMark();
	if (iIndex < 0)
		return;

	if ((DWORD)iIndex < m_pModel->NumNodes())
	{
		m_pModel->RemoveNode(m_pModel->GetNode((DWORD)iIndex));
	}

	FillNodeList();
}


void CModelEditDlg::OnSetTexture()
{
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	DWORD i, nTexture;
	ModelPiece *pPiece;

	nTexture = 0;

	// Look up the texture directory in the registry
	CRegMgr regMgr;
	CString csOpenDir;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "OpenDir", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = 256;
		regMgr.Get("Texture", csOpenDir.GetBufferSetLength(dwSize), dwSize);
		csOpenDir.ReleaseBuffer(-1);
	}

	if (csOpenDir.Find('.') == -1)
		csOpenDir.Empty();

	char szFilter[] = "Texture Files (*.dtx)|*.dtx|All Files (*.*)|*.*||";

	CFileDialog dlg (TRUE, "dtx", (csOpenDir.GetLength()) ? (LPCTSTR)csOpenDir : NULL, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, szFilter, this);

	if (dlg.DoModal() != IDOK)
		return;

	// Store the texture directory in the registry
	csOpenDir = dlg.GetPathName();
	csOpenDir = csOpenDir.Left(csOpenDir.GetLength() - dlg.GetFileName().GetLength()) + "*.dtx";
	regMgr.Set("Texture", (LPCTSTR)csOpenDir);

	// talon: 2001 sets the texture of the first selected piece.
	if (m_pModel)
	{
		for (i=0; i < (DWORD)m_PieceList.GetItemCount(); i++)
		{
			if (m_PieceList.GetItemState(i, LVIS_SELECTED) && (pPiece = m_pModel->GetPiece(i)))
			{
				nTexture = pPiece->m_iTexture;
				break;
			}
		}
	}

	if (!SetRenderTexture(dlg.GetPathName(), nTexture))
	{
		MessageBox("Unable to load or set texture", "ModelEdit");
	}
}


void CModelEditDlg::OnAddChildModel()
{
	CAddChildModelDlg dlg(NULL);
	CString sUnused, str;
	ModelLoadRequest request;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	char szList[2048], szFilename[2048], dirName[1024], newFilename[2048], errStr[4096];
	char *pToken, *pFilename;
	Model *pChild;
	ChildInfo *pInfo;

	if (!m_pModel)
		return;

	dlg.m_bAllowLetters = dlg.m_bAllowNumbers = dlg.m_bAllowOthers = TRUE;
	if (dlg.DoModal(IDS_MODELEDIT_CAPTION, IDS_ADDCHILDMODEL) != IDOK)
		return;

	strcpy(szList, dlg.m_EnteredText.GetBuffer(0));
	// talon: RETAIL BUG: the file name is the whole list, not the token, so
	// with more than one child model each one loads from a bad file name.
	strcpy(szFilename, szList);

	pToken = strtok(szList, "; ");
	while (pToken)
	{
		if (m_pModel->FindChildModelByFilename(pToken))
		{
			DoMessageBox(IDS_NODUPLICATECHILDMODELNAMES, MB_OK);
		}
		else
		{
			CHelpers::ExtractNames(m_strFilename, dirName, NULL, NULL, NULL);
			if (dirName[0] == 0)
			{
				sprintf(newFilename, "%s", szFilename);
			}
			else
			{
				sprintf(newFilename, "%s\\\\%s", dirName, szFilename);
			}

			IsFilenameLTAExt(m_strFilename);
			if (IsFilenameLTAExt(szFilename))
			{
				pChild = load_LTA_Using_FeedBackDialog(newFilename, true);
			}
			else
			{
				pFilename = pToken;
				request.m_bLoadChildModels = FALSE;
				pChild = LoadChildModel(&request, (char*)(LPCTSTR)m_strFilename, pToken);
			}

			if (!pChild)
			{
				str.FormatMessage(IDS_CANTLOADCHILDMODEL, pFilename);
				DoMessageBox(str, MB_OK);
				return;
			}

			if (pInfo = m_pModel->AddChildModel(pChild, pFilename, errStr, dlg.m_bScaleSkeleton))
			{
				pInfo->m_SaveIndex = pChild->GetSaveIndex();

				str.FormatMessage(IDS_ADDEDCHILDSUCCESSFULLY, pFilename);
				DoMessageBox(str, MB_OK);

				FillChildModelList();
				FillAnimList();
				InitTaggedArrays();
				SetChangesMade();
			}
			else
			{
				delete pChild;
				str.FormatMessage(IDS_CANTADDCHILDMODEL, errStr);
				DoMessageBox(str, MB_OK);
			}
		}

		pToken = strtok(NULL, "; ");
	}
}


void CModelEditDlg::OnRemoveChildModel()
{
	int iCurSel;
	uint32 itemIndex;
	CString curFilename, itemText;
							 
	if (!m_pModel)
		return;

	iCurSel = m_ChildModelList.GetCurSel();
	if (iCurSel != LB_ERR)
	{
		m_ChildModelList.GetText(iCurSel, itemText);
		if (m_pModel->FindChildModelByFilename((char*)(LPCTSTR)itemText, &itemIndex))
		{

			if (m_pModel->RemoveChildModel(itemIndex))
			{
				StopAllPlayback();
				m_AnimInfos[ANIMINFO_MAIN].m_Tracker.m_TimeRef.Init(m_pModel);

				FillChildModelList();
				FillAnimList();
				InitTaggedArrays();

				SetChangesMade();
			}
		}
	}
}


void CModelEditDlg::OnRebuildChildModelTree()
{
}


void CModelEditDlg::OnTimer(UINT nIDEvent) 
{
	static BOOL bFirst=TRUE;

	// Handle startup options (we do it in the timer so we can do MessageBox).
	if (bFirst)
	{
		bFirst = FALSE;

		if (__argc > 1)
		{
			DoLoadModel(__argv[1], FALSE);
		}

		if (__argc > 2)
		{
			SetRenderTexture(__argv[2]);
		}
	}

	CDialog::OnTimer(nIDEvent);
}


void CModelEditDlg::OnAddSocket()
{
	DWORD i;
    uint32 nodeIndex;
	int curSel;
	AddSocketDlg dlg;
	char testName[256];
	ModelSocket *pSocket;
	DVector nodePos, parentPos, forwardVec;
	DVector vecs[3];
	ModelNode *pNode, *pParent;
	DMatrix mat, fullTransform, *pNodeTransform, *pParentTransform;
	TransformMaker tMaker;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	


	if (!m_pModel)
		return;

	curSel = m_NodeList.GetSelectionMark();
	if ((DWORD)curSel < m_pModel->NumNodes())
	{
		dlg.m_NodeName = m_pModel->GetNode((DWORD)curSel)->GetName();
	}

	// Fill in a unique name.
	for (i=0; i < m_pModel->NumSockets()+1; i++)
	{
		sprintf(testName, "Socket%d", i);
		if (!m_pModel->FindSocket(testName))
		{
			dlg.m_SocketName = testName;
			break;
		}
	}

	dlg.m_pDlg = this;
	if (dlg.DoModal() == IDOK)
	{
		// No duplicate names..
		if (m_pModel->FindSocket((LPCTSTR)dlg.m_SocketName))
			return;

		// Node must exist.
		if (!m_pModel->FindNode((LPCTSTR)dlg.m_NodeName, &nodeIndex))
			return;

		if (pSocket = new ModelSocket)
		{
			pSocket->SetName((LPCTSTR)dlg.m_SocketName);
			pSocket->m_iNode = nodeIndex;
			
			// Set it up facing down the bone.
			pNode = m_pModel->GetNode(nodeIndex);
			if (pParent = m_pModel->FindParent(m_pModel->GetNode(nodeIndex), m_pModel->GetRootNode()))
			{
				if (m_pModel->NumAnims() > 0)
				{
					tMaker.m_Anims[0].Init(m_pModel, 0, 0, 0, 0, 0.0f);
					tMaker.m_nAnims = 1;
					if (tMaker.SetupTransforms())
					{
						pNodeTransform = m_pModel->GetTransform(pNode->GetNodeIndex());
						pParentTransform = m_pModel->GetTransform(pParent->GetNodeIndex());

						Mat_GetTranslation(*pNodeTransform, nodePos);
						Mat_GetTranslation(*pParentTransform, parentPos);
					
						forwardVec = nodePos - parentPos;
						forwardVec.Norm();

						gr_BuildFrameOfReference(&forwardVec, NULL, &vecs[0], &vecs[1], &vecs[2]);
						Mat_Identity(&mat);
						
						Mat_SetBasisVectors(&mat, &vecs[0], &vecs[1], &vecs[2]);

						fullTransform = pNodeTransform->MakeInverseTransform() * mat;
						pSocket->m_Rot.ConvertFromMatrix(fullTransform);
					}
				}
			}
			
			if (m_pModel->AddSocket(pSocket))
			{
				SetChangesMade();
				FillSocketList();
				m_SocketList.SetCurSel((int)(m_pModel->NumSockets() - 1));
			}
			else
			{
				delete pSocket;
			}
		}
	}
}


void CModelEditDlg::OnRemoveSocket()
{
	int curSel;

	if (!m_pModel)
		return;

	curSel = m_SocketList.GetCurSel();
	if (curSel != LB_ERR && curSel < (int)m_pModel->NumSockets())
	{
		m_pModel->RemoveSocket((DWORD)curSel);
		FillSocketList();
	}
}


void CModelEditDlg::OnRenameSocket()
{
	int curSel;
	CStringDlg dlg;
	LPCTSTR pName;
	ModelSocket *pSocket;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	if (!m_pModel)
		return;

	curSel = m_SocketList.GetCurSel();
	if (curSel != LB_ERR && curSel < (int)m_pModel->NumSockets())
	{
		pSocket = m_pModel->GetSocket((DWORD)curSel);

		dlg.m_bAllowLetters = TRUE;
		dlg.m_bAllowNumbers = TRUE;
		dlg.m_bAllowOthers = TRUE;
		dlg.m_EnteredText = pSocket->GetName();

		if (dlg.DoModal(IDS_MODELEDIT_CAPTION, IDS_ENTER_SOCKETNAME) == IDOK)
		{
			pName = (LPCTSTR)dlg.m_EnteredText;
			if (strlen(pName) == 0 || m_pModel->FindSocket(pName))
			{
				DoMessageBox(IDS_DUPLICATESOCKETNAME, MB_OK);
			}
			else
			{
				pSocket->SetName(pName);
				FillSocketList(TRUE);
			}
		}
	}
}


void CModelEditDlg::OnLButtonDown(UINT nFlags, CPoint point) 
{
	DWORD i;

	CDialog::OnLButtonDown(nFlags, point);

	// See if it's in a tracker.
	for (i=0; i < m_Trackers; i++)
	{
		if (m_Trackers[i]->m_Rect.PtInRect(point))
		{
			StartTracker(i, point);
		}
	}
}


void CModelEditDlg::OnMouseMove(UINT nFlags, CPoint point) 
{
	RectTracker *pTracker;
	CPoint delta, center;
	float rotAngle;
	ModelSocket *pSocket;
	LTMatrix mNewTransform, mOldTransform,mOffset, mFinal, mSocket, mBasis;
	LTVector vecs[3], vOffset, vTrans;
	char buf[512];
	int i;
	DWORD iNode;
	
	AnimInfo *pAnimInfo;
	ChildInfo *pChildInfo;
	CIRelation *pRelation;
	ModelNode *pNode, *pParent;
	TransformMaker tMaker;
	BOOL bGlobal;

	
	if (!m_pModel || !m_bTracking || m_iCurTracker >= m_Trackers.GetSize())
		return;

	pTracker = m_Trackers[m_iCurTracker];
	bGlobal = !!m_GlobalSpace.GetCheck();

	vecs[0].Init(1.0f, 0.0f, 0.0f);
	vecs[1].Init(0.0f, 1.0f, 0.0f);
	vecs[2].Init(0.0f, 0.0f, 1.0f);

	delta = point - m_LastCursorPos;
	if (delta.x == 0)
		return;

	m_TotalDelta += delta.x;

	// Generate some feedback to window's text field
	switch( pTracker->m_Type )
	{
	case RT_ROTATION:
		// rotate x degrees for every pixel
		rotAngle = GetRotAngle( delta.x );
		sprintf( buf, "Rotation: %.1f", (GetRotAngle(m_TotalDelta) * 360.0f) / MATH_CIRCLE );
		break;
	case RT_POSITION:
		sprintf( buf, "Translation: %.1f", GetPosAmount(m_TotalDelta) );
		break;
	}

	// Do socket/relation specific stuff.
	if (IsButtonSelected(IDC_SOCKET_EDIT) && (pSocket = GetSelectedSocket()))
	{
		pSocket->ConvertToMatrix(mSocket);

		// Setup the transforms..
		if(!m_RenderWnd.SetupTransformMaker(&tMaker))
			return;

		if (!tMaker.SetupTransforms())
			return;

		mOldTransform = m_pModel->m_Transforms[pSocket->m_iNode];
		mNewTransform = mOldTransform * mSocket;
			
		mOffset.Identity();

		switch( pTracker->m_Type )
		{
		case RT_ROTATION:
			Mat_SetupRot( &mOffset, &vecs[pTracker->m_Axis], rotAngle );
			break;
		case RT_POSITION:
			vOffset.Init();
			vOffset[pTracker->m_Axis] = GetPosAmount( delta.x );
			mOffset.SetTranslation( vOffset );
			break;
		}

		// Put the offset in local space.
		if (bGlobal)
		{
			mBasis.Identity();
			mNewTransform.GetTranslation(vTrans);
			mBasis.SetTranslation(vTrans);
		}
		else
		{
			mBasis = mNewTransform;
		}

		// Put the offset into the right basis.
		mOffset = mBasis * mOffset * ~mBasis;

		mFinal = ~mOldTransform * (mOffset * mNewTransform);

		pSocket->ConvertFromMatrix(mFinal);
	}
	else
	{
		// Find the first selected animation
		pAnimInfo = NULL;
		for (i=0; (i < m_AnimList.GetItemCount()) && (!pAnimInfo); i++)
		{
			if (m_AnimList.GetItemState(i, LVIS_SELECTED) != 0)
				pAnimInfo = m_pModel->GetAnimInfo(i);
		}
		if (pAnimInfo)
		{
			pChildInfo = pAnimInfo->m_pChildInfo;

			// Change the weights on the selected nodes.
			for (i=0; i < m_NodeList.GetItemCount(); i++)
			{
				// Skip over items that aren't selected
				if (!m_NodeList.GetItemState(i, LVIS_SELECTED))
					continue;

				iNode = (DWORD)i;
				if (iNode >= pChildInfo->m_Relation.GetSize())
					continue;

				pNode = m_pModel->GetNode(iNode);
				pParent = m_pModel->FindParent(pNode, m_pModel->GetRootNode());
				if (!pParent)
					continue;

				pRelation = &pChildInfo->m_Relation[iNode];
				
				// Get the old and new (using the relation) node transforms (so we can setup the relation).
				if(!m_RenderWnd.SetupTransformMaker(&tMaker))
					return;

				if (!tMaker.SetupTransforms())
					return;

				mNewTransform = m_pModel->m_Transforms[iNode];
				
				pRelation->m_Pos.Init();
				pRelation->m_Rot.Init();
				if (!tMaker.SetupTransforms())
					return;

				mOldTransform = m_pModel->m_Transforms[iNode];

				
				// Make an offset.
				mOffset.Identity();
				if (pTracker->m_Type == RT_ROTATION)
				{
					Mat_SetupRot(&mOffset, &vecs[pTracker->m_Axis], rotAngle);
				}
				else
				{
					vOffset.Init();
					vOffset[pTracker->m_Axis] = GetPosAmount(delta.x);
					mOffset.SetTranslation(vOffset);
				}

				
				// Put the offset into the node's local space.
				if (bGlobal)
				{
					mBasis.Identity();
					mNewTransform.GetTranslation(vTrans);
					mBasis.SetTranslation(vTrans);
				}
				else
				{
					mBasis = mNewTransform;
				}

				// Figure out the relation (~old * offset * new... ie: (offset*new) - old)
				mOffset = mBasis * (mOffset * ~mBasis);
				mFinal = ~mOldTransform * (mOffset * mNewTransform);
				pRelation->ConvertFromMatrix(mFinal);
			}
		}
	}

	SetWindowText(buf);

	// Center the cursor.
	center = m_TrackerCenterPt;
	m_LastCursorPos = center;
	ClientToScreen(&center);
	SetCursorPos(center.x, center.y);

	m_RenderWnd.Draw();
	
	CDialog::OnMouseMove(nFlags, point);
}


void CModelEditDlg::OnLButtonUp(UINT nFlags, CPoint point) 
{
	HandleButtonUp(0);
	CDialog::OnLButtonUp(nFlags, point);
}



LRESULT CModelEditDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if (message == WM_MBUTTONUP)
	{
		HandleButtonUp(1);
	}
	else if (message == WM_RBUTTONUP)
	{
		HandleButtonUp(2);
	}
	
	return CDialog::WindowProc(message, wParam, lParam);
}


void CModelEditDlg::OnMood()
{

}


void CModelEditDlg::OnInternalRadius()
{
	CStringDlg dlg;
	char tempBuf[64];
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	if (!m_pModel)
		return;

	dlg.m_bAllowLetters = FALSE;
	dlg.m_bAllowNumbers = TRUE;
	dlg.m_bAllowOthers = FALSE;
	
	sprintf(tempBuf, "%.3f", m_pModel->m_GlobalRadius);
	dlg.m_EnteredText = tempBuf;

	if (dlg.DoModal(IDS_MODELEDIT_CAPTION, IDS_ENTER_GLOBAL_RADIUS) == IDOK)
	{
		m_pModel->m_GlobalRadius = (float)atof(dlg.m_EnteredText);
		SetChangesMade();
	}
}


void CModelEditDlg::OnCalcInternalRadius()
{
	if( !m_pModel )
	{
		MessageBox( "No model loaded.", "Error", MB_OK | MB_ICONERROR );
		return;
	}

	if( MessageBox( "Are you sure?  This could take a while..", "Do you really want to do this?", MB_OKCANCEL ) != IDOK )
		return;

	HCURSOR oldCursor = SetCursor( LoadCursor( NULL, IDC_WAIT ) );

	// calculate the estimated internal radius

	// turn on render distance checking
	// t.f important
	m_RenderWnd.m_bCalcRadius = true;

	// get our main anim info
	MEAnimInfo* pAnimInfo = GetAnimInfo( 0 );

	// save our current animation state
	AnimTimeRef oldTracker = pAnimInfo->m_Tracker.m_TimeRef;
	ModelAnim* pOldAnim = pAnimInfo->CurAnim();

	// initialize the maximums
	float fNewRadius = 0.0f;
	char* pAnimName = NULL;

	// for each animation
	for( uint32 nAnimLoop = 0; nAnimLoop < m_pModel->NumAnims(); ++nAnimLoop )
	{
		// get the animation
		ModelAnim* pAnim = m_pModel->GetAnim( nAnimLoop );
		if( !pAnim )
			continue;
		// switch to that animation
		pAnimInfo->m_Wnd.SetAnim( pAnim );
		trk_Init( &pAnimInfo->m_Tracker, m_pModel, nAnimLoop );

		// resert the maximum radius
		float fAnimRadius = 0.0f;

		// for each keyframe
		for( uint32 nKeyframeLoop = 0; nKeyframeLoop < pAnim->NumKeyFrames(); ++nKeyframeLoop )
		{
			// move to this keyframe
			SetCurrentPosition( (DWORD)0, nKeyframeLoop, nKeyframeLoop, 0.0f );
			// only draw the first frame
			// t.f important
 			m_RenderWnd.m_bCalcAndDraw = (nKeyframeLoop == 0);
			// draw the model
			m_RenderWnd.Draw();
			// get the maximum of the animation and this frame's radii
			// t.f important			
			fAnimRadius = LTMAX( fAnimRadius, m_RenderWnd.m_fCurRadius );
		}

		// new maximum distance
		if( fAnimRadius > fNewRadius )
		{
			// save distance and animation name
			fNewRadius = fAnimRadius;
			pAnimName = m_pModel->GetAnim( nAnimLoop )->GetName();
		}
	}

	// turn off render distance checking
	// t.f important
	m_RenderWnd.m_bCalcRadius = false;

	// add a fudge factor to the distance
	fNewRadius *= 1.01f;

	// restore the old animation state
	pAnimInfo->m_Wnd.SetAnim( pOldAnim );
	pAnimInfo->m_Tracker.m_TimeRef = oldTracker;

	// restore non-wait cursor
	SetCursor( oldCursor );

	// display distance and largest animation and ask user to confirm
	char results[2048];
	sprintf( results, "Results:\n"
					  "\tOld Dimensions: %f\n"
					  "\tNew Dimensions: %f\n"
					  "\tLongest animation: %s\n"
					  "\nUse these results?", m_pModel->m_GlobalRadius, fNewRadius, pAnimName );
	if( MessageBox( results, "Is that better?", MB_YESNO ) != IDYES )
		return;

	// save the new distance
	m_pModel->m_GlobalRadius = fNewRadius;

	SetChangesMade();
}


void CModelEditDlg::OnEditWeights()
{
	WeightEditDlg dlg;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	
	
	if (!m_pModel)
		return;

	dlg.m_pDlg = this;
	dlg.m_pModel = m_pModel;
	dlg.DoModal();
	if (dlg.m_bChangesMade)
		SetChangesMade();
}


void CModelEditDlg::SetCurrentLOD(DWORD iLOD)
{
	m_nCurrentLOD = iLOD;
	m_RenderWnd.SetLOD(iLOD);
}

//this will create a NULL piece LOD at the specified distance from the camera


void CModelEditDlg::OnPieceInfo()
{
	int i;
	ModelPiece *pPiece, *pOther;
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	// talon: 2001's Piece Info dialog edits the first selected piece and
	// copies what changed to the other selected pieces.
	for (i=0; i < m_PieceList.GetItemCount(); i++)
	{
		if (m_PieceList.GetItemState(i, LVIS_SELECTED))
			break;
	}

	if (i < m_PieceList.GetItemCount() && i < (int)m_pModel->NumPieces())
	{
		pPiece = m_pModel->GetPiece(i);
		CPieceInfoDlg dlg(pPiece);
		if (dlg.DoModal() == IDOK)
		{
			// talon: RETAIL BUG: <= lets i reach NumPieces().
			while (++i < m_PieceList.GetItemCount() && i <= (int)m_pModel->NumPieces())
			{
				if (m_PieceList.GetItemState(i, LVIS_SELECTED))
				{
					pOther = m_pModel->GetPiece(i);

					if (dlg.m_ChangedFlags & 1)
						pOther->m_SpecularPower = pPiece->m_SpecularPower;

					if (dlg.m_ChangedFlags & 2)
						pOther->m_SpecularScale = pPiece->m_SpecularScale;

					if (dlg.m_ChangedFlags & 4)
						pOther->m_iTexture = pPiece->m_iTexture;
				}
			}

			m_bChangesMade = TRUE;
		}
	}
}


// talon: stand-in: 2001's OnPieceMerge (0x41ec5d..0x41f530, 2259 bytes:
// merges the selected pieces' coincident vertices, "Finished.\nRemoved %d
// vertices\nMerged %d vertices", then rebuilds the LODs) is not written
// yet.  This only reports what the 2001 one says when there is nothing to
// merge.
void CModelEditDlg::OnPieceMerge()
{
	IdleChanger idle(&m_cWinIdle, IDLE_DIALOG_DELAY);  	

	if (!m_pModel || !m_pModel->NumPieces())
	{
		MessageBox("No pieces are loaded.", "What's the big idea?", MB_OK|MB_ICONINFORMATION);
		return;
	}

	MessageBox("No pieces were found to merge.", "Try again", MB_OK|MB_ICONINFORMATION);
}


void CModelEditDlg::OnEndlabeleditNodeList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;

	// Update the node's name
	if ((pDispInfo->item.mask & LVIF_TEXT) != 0)
		m_pModel->GetNode(pDispInfo->item.iItem)->SetName(pDispInfo->item.pszText);
	
	SetChangesMade();

	*pResult = TRUE;
}

void CModelEditDlg::OnItemChangedNodeList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	static BOOL bInChange = FALSE;
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	ModelNode *pNode;
	DWORD iIndex;
	BOOL bSelected, bCheck;
	POSITION pos;
	int i;

	*pResult = 0;

	if (bInChange || m_bFillingNodeList || !m_pModel || !m_RenderWnd.GetSelectedNodesSize())
		return;

	iIndex = (DWORD)pNMListView->iItem;
	if (iIndex >= (DWORD)m_NodeList.GetItemCount() || iIndex >= (DWORD)m_RenderWnd.GetSelectedNodesSize())
		return;

	// Refresh the selection state array entry
	bSelected = m_NodeList.GetItemState((int)iIndex, LVIS_SELECTED) != 0;
	m_RenderWnd.m_SelectedNodes[iIndex] = bSelected;

	bCheck = m_NodeList.GetCheck((int)iIndex);
	pNode = m_pModel->GetNode(iIndex);

	// Has the check changed?
	if (bCheck ? !(pNode->m_Flags & MNODE_ROTATIONONLY) : (pNode->m_Flags & MNODE_ROTATIONONLY))
	{
		if (bSelected)
		{
			// Check all the selected nodes the same.
			pos = m_NodeList.GetFirstSelectedItemPosition();
			while (pos)
			{
				i = m_NodeList.GetNextSelectedItem(pos);
				if (bCheck)
					m_pModel->GetNode(i)->m_Flags |= MNODE_ROTATIONONLY;
				else
					m_pModel->GetNode(i)->m_Flags &= ~MNODE_ROTATIONONLY;

				bInChange = TRUE;
				m_NodeList.SetCheck(i, bCheck);
				bInChange = FALSE;
			}
		}
		else
		{
			if (bCheck)
				pNode->m_Flags |= MNODE_ROTATIONONLY;
			else
				pNode->m_Flags &= ~MNODE_ROTATIONONLY;
		}

		SetChangesMade();
	}
}

void CModelEditDlg::OnDestroy() 
{
	// Stop the idling thread
	m_cWinIdle.EndIdle();

	CDialog::OnDestroy();
}

// one of the tree controls has had its selection changed

// a new piece has been selected
void CModelEditDlg::OnItemChangedPieceList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	DWORD iIndex;

	if (m_pModel && m_RenderWnd.GetSelectedPiecesSize())
	{
		iIndex = (DWORD)pNMListView->iItem;
		if (iIndex < (DWORD)m_PieceList.GetItemCount() && iIndex < (DWORD)m_RenderWnd.GetSelectedPiecesSize())
			m_RenderWnd.m_SelectedPieces[iIndex] = m_PieceList.GetItemState((int)iIndex, LVIS_SELECTED) != 0;
	}

	*pResult = 0;
}

// one of the tree controls has had its selection edited

// a piece has had its name changed
void CModelEditDlg::OnEndLabelEditPieceList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;

	// Update the piece's name
	if ((pDispInfo->item.mask & LVIF_TEXT) != 0)
		m_pModel->GetPiece(pDispInfo->item.iItem)->SetName(pDispInfo->item.pszText);
	
	SetChangesMade();

	*pResult = TRUE;
}


// retrieve a list of the selected pieces (returns number of selected pieces)
// this does not iterate through children, the piece itself must be explicitly selected in order to be returned


// retrieve a list of the selected piece LODs (returns number of selected LODs)
// if firstIfParentOnly is true, then only the 0th lod will be returned if only the parent piece is selected
// otherwise all lods will be returned for the selected piece

BOOL CModelEditDlg::UpdateSelectedAnimList()
{
	DWORD i, index;

	
	// Get rid of them all if the model isn't there.
	if(!m_pModel)
	{
		m_SelectedAnims.Term();
		return TRUE;
	}

	// Get rid of invalid ones.
	for(i=0; i < m_SelectedAnims; i++)
	{
		if(m_SelectedAnims[i] >= m_pModel->NumAnims())
		{
			m_SelectedAnims.Remove(i);
			--i;
		}
	}

	// Add new selections and remove old ones.
	for(i=0; i < (DWORD)m_AnimList.GetItemCount(); i++)
	{
		if(m_AnimList.GetItemState((DWORD)i, LVIS_SELECTED) == 0)
		{
			index = m_SelectedAnims.FindElement(i);
			if(index != BAD_INDEX)
			{
				m_SelectedAnims.Remove(index);
			}
		}
		else
		{
			if(m_SelectedAnims.FindElement(i) == BAD_INDEX)
			{
				m_SelectedAnims.Append(i);
			}
		}
	}

	return TRUE;
}

void CModelEditDlg::OnSelChangeAnimList()
{
	DWORD i;
	MEAnimInfo *pInfo;
	
	m_bMovePieces = FALSE;

	if(!m_pModel)
		return;


	UpdateSelectedAnimList();

	
	// Activate/deactivate the animations.
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = &m_AnimInfos[i];

		if(i >= m_SelectedAnims.GetSize())
		{
			pInfo->Reset(NULL);
		}
	}

	
	// Activate any new ones and change the animation for any existing ones.
	for(i=0; i < m_SelectedAnims.GetSize(); i++)
	{
		if(i >= NUM_ANIM_INFOS)
			break;
	
		pInfo = &m_AnimInfos[i];
		if(!pInfo->m_Wnd.IsActive())
		{
			pInfo->m_Wnd.SetActive(TRUE);
			pInfo->m_Wnd.SetAnim(m_pModel->GetAnim(m_SelectedAnims[i]));
			trk_Init(&pInfo->m_Tracker, m_pModel, m_SelectedAnims[i]);
		}
		else if(m_SelectedAnims[i] != pInfo->m_Tracker.m_TimeRef.m_Cur.m_iAnim)
		{
			pInfo->m_Wnd.SetAnim(m_pModel->GetAnim(m_SelectedAnims[i]));
			trk_SetCurAnim(&pInfo->m_Tracker, m_SelectedAnims[i], pInfo->IsAnimPlaying());
		}

		pInfo->m_Tracker.m_TimeRef.m_Cur.m_iWeightSet = pInfo->CurAnim()->m_ModelEditWeightSet;
	}


	// Set button labels.
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = &m_AnimInfos[i];
		
		if(pInfo->m_Wnd.IsActive())
		{
			::EnableWindow(::GetDlgItem(m_hWnd, pInfo->m_AnimButtonID), TRUE);
			SetDlgItemText(pInfo->m_AnimButtonID, pInfo->CurAnim()->GetName());
		}
		else
		{
			::EnableWindow(::GetDlgItem(m_hWnd, pInfo->m_AnimButtonID), FALSE);
			SetDlgItemText(pInfo->m_AnimButtonID, "NONE");
		}
	}
}


void CModelEditDlg::OnBeginLabelEditAnimList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;

	*pResult = ((m_pModel->GetAnimInfo(pDispInfo->item.iItem))->GetAnimOwner() != m_pModel);
}

void CModelEditDlg::OnEndLabelEditAnimList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	LV_DISPINFO* pDispInfo = (LV_DISPINFO*)pNMHDR;

	if ((pDispInfo->item.mask & LVIF_TEXT) != 0)
		(m_pModel->GetAnim(pDispInfo->item.iItem))->SetName(pDispInfo->item.pszText);
	
	SetChangesMade();

	// talon: 2001 resizes the column for the new name.
	SetListColumnWidth(&m_AnimList, m_AnimList.GetStringWidth(pDispInfo->item.pszText), pDispInfo->item.iItem);

	*pResult = TRUE;
}


void CModelEditDlg::OnItemChangingAnimList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;

	if ((pNMListView->uOldState & 0x3000) == 0)
		*pResult = 0;
	// Make sure the checkbox state is correct.  (Note : I have no idea what constant value this mask associates with..)
	else if ((m_pModel->GetAnimInfo(pNMListView->iItem))->GetAnimOwner() == m_pModel)
		*pResult = (pNMListView->uNewState & 0x2000) == 0;
	else
		*pResult = (pNMListView->uNewState & 0x1000) == 0;
}

void CModelEditDlg::OnBeginDragAnimList(NMHDR* pNMHDR, LRESULT* pResult) 
{
	NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
	// TODO: Add your control notification handler code here
	
	*pResult = 0;
}

void CModelEditDlg::OnDropAnimList(int iItem)
{
	// Handle rearrangement of the animation list
	if (iItem != m_AnimList.GetDragIndex())
	{
		// Adjust for post-insertion instead of pre-insertion
		iItem = min(iItem, m_AnimList.GetItemCount() - 1);
		MoveAnims(iItem - m_AnimList.GetDragIndex());
	}
}

// ------------------------------------------------------------------------
// when the right button is clicked on one of our controls, see 
// get the menu for it.
// ------------------------------------------------------------------------
void CModelEditDlg::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	CMenu menu;
	
	if (pWnd->m_hWnd == m_AnimList.m_hWnd)
	{
		VERIFY(menu.LoadMenu(IDR_MENU_ANIMLIST));

		CMenu* pPopup = menu.GetSubMenu(0);
		ASSERT(pPopup != NULL);

		pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);	
	}
}


void CModelEditDlg::OnSortAnimName() 
{
	// Make sure this is valid...
	if ((!m_pModel) || (m_pModel->NumAnims() < 2))
		return;

	// Sort the animation list
	AnimInfo temp;
	DWORD i,j;

	for (i = 0; i < m_pModel->NumAnims() - 1; i++)
		for (j = i + 1; j < m_pModel->NumAnims(); j++)
		{
			if (stricmp(m_pModel->GetAnim(i)->GetName(), m_pModel->GetAnim(j)->GetName()) > 0)
			{
				temp = m_pModel->m_Anims[i];
				m_pModel->m_Anims[i] = m_pModel->m_Anims[j];
				m_pModel->m_Anims[j] = temp;
			}
		}

	SetChangesMade();

	// Re-display the animation list
	FillAnimList();
}

void CModelEditDlg::OnSortAnimRelation() 
{
	// Make sure this is valid...
	if ((!m_pModel) || (m_pModel->NumAnims() < 2))
		return;

	// Sort the animation list
	AnimInfo temp;
	DWORD i,j;

	for (i = 0; i < m_pModel->NumAnims() - 1; i++)
		for (j = i + 1; j < m_pModel->NumAnims(); j++)
		{
			int iDiffi = (int)(m_pModel->GetAnimInfo(i)->GetAnimOwner() - m_pModel);
			int iDiffj = (int)(m_pModel->GetAnimInfo(j)->GetAnimOwner() - m_pModel);
			if (iDiffi > iDiffj)
			{
				temp = m_pModel->m_Anims[i];
				m_pModel->m_Anims[i] = m_pModel->m_Anims[j];
				m_pModel->m_Anims[j] = temp;
			}
		}

	SetChangesMade();

	// Re-display the animation list
	FillAnimList();
}

void CModelEditDlg::OnDblClkSocketList() 
{
	ModelSocket *pSocket;
	int iSocket;
	EulerAngles angles;

	if (!m_pModel)
		return;

	// Get the selected socket
	iSocket = m_SocketList.GetCurSel();

	// This should never happen
	if ((iSocket < 0) || (iSocket >= (int)m_pModel->NumSockets()))
		return;

	pSocket = m_pModel->GetSocket(iSocket);
	
	// Get the attachment from the registry
	CString csAttachment("");

	CRegMgr regMgr;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Attachments", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = 256;
		regMgr.Get(pSocket->GetName(), csAttachment.GetBufferSetLength(dwSize), dwSize);
		csAttachment.ReleaseBuffer(-1);
	}

	CSocketEdit dlg;

	int dlgResult = IDOK;
	do
	{
		// Convert the rotation to euler angles
		angles = Eul_FromQuat(pSocket->m_Rot, EulOrdXYZs);

		// Set up the dialog
		dlg.m_Name = pSocket->GetName();
		dlg.m_PosX = pSocket->m_Pos.x;
		dlg.m_PosY = pSocket->m_Pos.y;
		dlg.m_PosZ = pSocket->m_Pos.z;
		dlg.m_RotX = angles.x * 180.0f / MATH_PI;
		dlg.m_RotY = angles.y * 180.0f / MATH_PI;
		dlg.m_RotZ = angles.z * 180.0f / MATH_PI;

		dlg.m_Attachment = csAttachment;

		// Edit the socket
		dlgResult = dlg.DoModal();
		if (dlgResult == IDCANCEL)
			return;

		// Change the position
		pSocket->m_Pos.x = dlg.m_PosX;
		pSocket->m_Pos.y = dlg.m_PosY;
		pSocket->m_Pos.z = dlg.m_PosZ;

		// Change the rotation
		angles.x = dlg.m_RotX * MATH_PI / 180.0f;
		angles.y = dlg.m_RotY * MATH_PI / 180.0f;
		angles.z = dlg.m_RotZ * MATH_PI / 180.0f;
		pSocket->m_Rot = Eul_ToQuat(angles);


		// Change the name
		if (dlg.m_Name.CompareNoCase(pSocket->GetName()) != 0)
		{
			pSocket->SetName(dlg.m_Name);
			FillSocketList(TRUE);
			// Make sure the attachment tries to change to the new value
			csAttachment = _T("");
		}

		// Change the attachment
		if (dlg.m_Attachment.CompareNoCase(csAttachment) != 0)
		{
			// Clear out the old attachment
			if (pSocket->m_pAttachment)
				delete pSocket->m_pAttachment;
			pSocket->m_pAttachment = NULL;

			if (dlg.m_Attachment.GetLength())
			{
				if( IsFilenameLTAExt( dlg.m_Attachment ) )
				{
					
					char *str = dlg.m_Attachment.LockBuffer();
					dlg.m_Attachment.UnlockBuffer();
					Model *pModel = load_LTA_Using_FeedBackDialog( str, false  );

					if( pModel == NULL )
					{
						MessageBox("Failed to load lta file","load error");
						dlg.m_Attachment = csAttachment ;
					
					}else // success !!
					{
						pSocket->m_pAttachment = pModel ;
					}
				}
				else // else we're loading abcs
				{
					// talon: 2001 loads the .abc.
					ModelLoadRequest request;
					if (request.m_pFile = streamsim_Open(dlg.m_Attachment, "rb"))
					{
						pSocket->m_pAttachment = new Model;
						pSocket->m_pAttachment->Load(&request);
						request.m_pFile->Release();
					}
					else
					{
						dlg.m_Attachment = csAttachment;
					}
				}
			
			}

			// Store the attachment in the registry if it didn't fail loading
			if (dlg.m_Attachment.CompareNoCase(csAttachment) != 0)
				regMgr.Set(pSocket->GetName(), (LPCTSTR)dlg.m_Attachment);

			// Save the string for the next go-round
			csAttachment = dlg.m_Attachment;
		}

		SetChangesMade();

	} while (dlgResult != IDOK);
}

float CModelEditDlg::CalcFOV()
{
	CRegMgr regMgr;
	CString csFOV;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Render", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = 256;
		csFOV = "90";
		regMgr.Get("FOV", csFOV.GetBufferSetLength(dwSize), dwSize);
		csFOV.ReleaseBuffer(-1);
		return max((float)atof(csFOV), 10.0f);
	}
	else
		return 90.0f;
}

void CModelEditDlg::SetFOV(float FOV)
{
	CRegMgr regMgr;
	CString csFOV;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Render", HKEY_CURRENT_USER))
	{
		csFOV.Format("%f", FOV);
		regMgr.Set("FOV", csFOV);
	}
}


MEAnimInfo* CModelEditDlg::GetAnimInfo(uint32 iAnimInfo)
{
	ASSERT(iAnimInfo < NUM_ANIM_INFOS);
	return &m_AnimInfos[iAnimInfo];
}


void CModelEditDlg::StopAllPlayback()
{
	uint32 i;

	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		m_AnimInfos[i].StopPlayback();
	}
}


void CModelEditDlg::ResetAnimInfos()
{
	uint32 i;

	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		GetAnimInfo(i)->Reset();
	}
}


void CModelEditDlg::DrawActiveAnimRects(BOOL bForce)
{
	DWORD dwActive[NUM_ANIM_INFOS];
	DWORD i;
	CDC *pDC;

	
	// Get the key state.
	GetPlaybackActiveFlags(dwActive, FALSE);

	// Redraw if necessary.	
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		if((dwActive[i] & PA_ACTIVE) != (m_LastAnimActiveFlags[i] & PA_ACTIVE) || 
			bForce)
		{
			pDC = GetDC();
				
				if(dwActive[i] & PA_ACTIVE)
					m_AnimInfos[i].m_ActiveFlagsWnd.SetColor(RGB(255,0,0));
				else
					m_AnimInfos[i].m_ActiveFlagsWnd.SetColor(RGB(0,0,0));
			
			ReleaseDC(pDC);
		}
	}
	
	// Remember the last state..
	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		m_LastAnimActiveFlags[i] = dwActive[i];
	}
}

// ------------------------------------------------------------------------
// IncKeyframe( bForward )
// increment the frame forward (or backwards...).
// ------------------------------------------------------------------------
void CModelEditDlg::IncKeyframe(BOOL bForward)
{
	DWORD dwActive[NUM_ANIM_INFOS];
	DWORD i, iKeyframe;
	MEAnimInfo *pInfo;

	// determine which anims are active & valid
	GetPlaybackActiveFlags(dwActive);


	for(i=0; i < NUM_ANIM_INFOS; i++)
	{
		pInfo = GetAnimInfo(i);

		// if the animation slot is active AND valid 
		if( dwActive[i] & (PA_ACTIVE | PA_VALID) )
		{
			if(bForward)
			{
				iKeyframe = (pInfo->m_Tracker.m_TimeRef.m_Cur.m_iFrame + 1) % pInfo->CurAnim()->NumKeyframes();
			}
			else
			{
				if(pInfo->m_Tracker.m_TimeRef.m_Cur.m_iFrame == 0)
				{
					if( pInfo->CurAnim()->NumKeyFrames() == 0 )
						iKeyframe = 0 ;
					else {
						iKeyframe = (int)(pInfo->CurAnim()->NumKeyframes() - 1);
					}
					//iKeyframe = pInfo->CurAnim()->NumKeyframes() == 0 ? 0 : (int)(pInfo->CurAnim()->NumKeyframes() - 1);
				}else
					iKeyframe = (int)(pInfo->m_Tracker.m_TimeRef.m_Cur.m_iFrame - 1);
			}

			SetCurrentPosition(i, (DWORD)iKeyframe, (DWORD)iKeyframe, 0.0f);
			pInfo->SetKeyframeWindowTime();
		}
	}
}


void CModelEditDlg::OnOK()
{
	// Forward the enter key to the two edit controls on the dialog 
	//	(Note: Gross hack because MFC uses the enter key for "OK" in dialog-based apps)
	if (GetFocus() == &m_EditFrameTime)
		SetEditFrameTime( );
	if (GetFocus() == &m_EditFrameString)
		SetEditFrameString( );
}


BOOL CModelEditDlg::OnCommand(WPARAM wParam, LPARAM lParam) 
{
	DWORD i;
	MEAnimInfo *pInfo;
	ModelAnim *pAnim;


	if(m_pModel)
	{
		for(i=0; i < NUM_ANIM_INFOS; i++)
		{
			pInfo = GetAnimInfo(i);

			if(LOWORD(wParam) == pInfo->m_AnimButtonID)
			{
				if(pInfo->m_Wnd.IsActive())
				{
					WeightSetSelectDlg dlg(m_pModel, pInfo->CurAnim()->m_ModelEditWeightSet);

					if(dlg.DoModal() == IDOK)
					{
						// Set it up..
						if(pAnim = pInfo->CurAnim())
						{
							if(pAnim->m_ModelEditWeightSet != dlg.m_SelectedSet)
							{
								pAnim->m_ModelEditWeightSet = dlg.m_SelectedSet;
								SetChangesMade();
							}
						}

						// Make it update the trackers..
						OnSelChangeAnimList();
					}
				}
				
				break;
			}
		}
	}

	return CDialog::OnCommand(wParam, lParam);
}


void CModelEditDlg::OnFrameStringChange()
{
	if(m_bInStringChange)
		return;

	TempVarSetter<BOOL> setter(&m_bInStringChange, TRUE);

	SetEditFrameString();
}


void CModelEditDlg::OnNewBgColor() 
{
	extern void SetGLMBackgroundColor( GLM_CONTEXT hContext, COLORREF win_color );

	// TODO: Add your command handler code here
		// TODO: Add your command handler code here
	CColorDialog colorPicker ;
	colorPicker.DoModal();
	COLORREF ms_color = colorPicker.GetColor();

	CRegMgr regMgr;
	if (regMgr.Init(szRegKeyCompany, szRegKeyApp, szRegKeyVer, "Render", HKEY_CURRENT_USER))
	{
		UINT32 dwSize = sizeof(float)*3;
		unsigned char *pColDecomp = (unsigned char*)&ms_color;
		float bgColor[3];
		// unitize
		bgColor[0] = pColDecomp[0]/256.0f;
		bgColor[1] = pColDecomp[1]/256.0f;
		bgColor[2] = pColDecomp[2]/256.0f;
		regMgr.Set("bgColor",bgColor , dwSize);

	}
	// set the bg color for m_RenderWnd->GLMContext.
	SetGLMBackgroundColor( m_RenderWnd.GetContext(), ms_color );
}


void CModelEditDlg::OnOptionsShoworiginalmodel() 
{
		m_RenderWnd.m_DrawStruct.m_bDrawOriginalModel = !m_RenderWnd.m_DrawStruct.m_bDrawOriginalModel ;
	UpdateMenuChecks();	
}


// talon: 2001's (0x4207c9): makes a keyframe between iPrev and iNext, at
// fPercent of the way.  KeyframeWnd's Create Keyframe and ImportKeyStrings
// call it.  The name is ours.
BOOL CreateKeyframe(ModelAnim *pAnim, int iPrev, int iNext, float fPercent)
{
	ModelAnim *pFrame;

	if (!pAnim->NumKeyframes())
		return FALSE;

	if (iPrev < 0 || iPrev > (int)pAnim->NumKeyframes())
		return FALSE;

	if (iNext < 0 || iNext > (int)pAnim->NumKeyframes())
		return FALSE;

	if (fPercent < 0.001f || fPercent > 0.999f)
		return FALSE;

	pFrame = pAnim->CopyKeyFrame(iPrev);
	if (!pFrame)
		return FALSE;

	if (!pAnim->PasteKeyFrame(pFrame, iNext))
	{
		delete pFrame;
		return FALSE;
	}

	delete pFrame;
	pAnim->InterpolateKeyFrame(iNext, iPrev, iNext+1, fPercent);
	return TRUE;
}
