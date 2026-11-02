// ------------------------------------------------------------------------
// lithtech (c) 2000
// ltaScene.cpp
// contains operations for translating a parse node tree into a lta scene.
// which is an in-mem representation of the lta ascii file.
// ------------------------------------------------------------------------

#include <assert.h>
#include <stdio.h>
#pragma warning (disable:4786)

#include "ltaScene.h"

using namespace std;

ostream *CLTATranslStatus::s_CurErrorLog = NULL ;
void (*CLTATranslStatus::s_fnLoadLogCallBack)( const char *) = NULL ;


///////////////////////////////////////////////////////////////////////////
/// NODE 
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
// Node Default constr
// ------------------------------------------------------------------------
Node::	Node() {
	shape=NULL ; // reference to shape, don't belong to node.
	index = 0 ; // ltmodel
	parent=NULL ;
	
		 mat.Identity();
	}

// ------------------------------------------------------------------------
// Node name constr
// ------------------------------------------------------------------------
Node::Node( string &name ){
	mat.Identity();
	shape=NULL ; // reference to shape, don't belong to node.
	index = 0 ; // ltmodel
	parent=NULL ;
	
	SetName(name);
}

// ------------------------------------------------------------------------
// deconstr
// ------------------------------------------------------------------------
Node::~Node()
{
	for( uint32 i = 0 ; i < children.size() ;i++ )
	{
		Node *child = children[i] ;
		delete child ;
		children[i] = NULL;
	}
	children.resize(0);
}

// finds the node with the name sname ...
Node *Node::FindName( const string & sname )
{
	if( sname == GetName() )
		return this ;
	else 
	{
		for( uint32 i = 0 ;i < children.size() ; i++ )
		{
			Node *node = children[i]->FindName(sname );
			if( node != NULL )
				return node ;
		}
	}
	return NULL ;
}

///////////////////////////////////////////////////////////////////////////
/// NODE 
///////////////////////////////////////////////////////////////////////////


// 0x4130a7
CLTA_ABC_PC_MAT::CLTA_ABC_PC_MAT()
	:m_specular_power(0), m_specular_scale(0), 
	m_texture_index(0), m_envtex_index(-1), CLTAMaterial( PC_MAT ) 
{
		m_diffuse[0] = m_diffuse[1] = m_diffuse[2] = 0.8f ;
		m_diffuse[3] = 1.0f ;
		m_ambient[0] = m_ambient[1] = m_ambient[2] = 0.0f ;
		m_emissive[0] = m_emissive[1] = m_emissive[2] = 0.0 ;
		m_specular[0] = m_specular[1] = m_specular[2] = 0.0 ;
		m_shininess = 0.0f ;
} 

Shape::	Shape() {}


CAnimSet::~CAnimSet() {
	// delete m_vAnims ...
	for( uint32 i = 0 ;i < m_vAnims.size() ;i++ )
	{
		delete m_vAnims[i] ;
	}
	m_vAnims.clear();
}

///////////////////////////////////////////////////////////////////////////
/*************************************************************************/
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
// KeyFrameTimeSlideToZero
// if the first key frame does not start at 0, slide down the rest of the 
// values.
// ------------------------------------------------------------------------
void KeyFrameTimeSlideToZero( CKeyFrame & kf )
{
	int num_times = kf.Size();
	
	if( num_times < 1 )
		return ;

	int base_time  = kf.GetTime( 0 ) ;

	if( base_time == 0 )
		return ;

	// shift the times values
	for( int i = 0 ;i < num_times ; i++ )
	{
		int new_time = kf.GetTime( i ) - base_time ;
		kf.SetTime( i , new_time );

	}
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
// Parse tree to Meta Format 
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
// ParseVector( ParseNode *vertex, float *array )
// reads an lta node in ( x y z ... ) format into a float array.
// array must be alloc'd & the same length as vrt->getNumElements()
// ------------------------------------------------------------------------
void
LTA::ParseVector( CParseNode *vrt, float * vert )
{
	for( int i = 0 ; i < (int)vrt->getNumElements() ; i++ )
	{
		vert[i] = (float)atof( vrt->getElement(i)->getValue());
	}
}

// ------------------------------------------------------------------------
// ParseShapeNode( shape, root )
// convert the parse node that's really a shape into a c++ -struct shape.
// pnroot = (shape name  
//             (parent name )
//             (geometry 
//				(mesh name ( vertex ( (v1) (v2) ... ) ) (tri-ifs < a b c>>)
//
// ------------------------------------------------------------------------
void
ParseShapeNode( Shape *shape, CParseNode *pnroot )
{
	// talon: 2001's (0x412923): the pc-mat material, not Build 69's render
	// style.
	CParseNode *tmp,*mesh ;
	int i ;
	// tmp = (shape name ... 
	shape->SetName( pnroot->getElement(1)->getValue()  );
	CLTATranslStatus::AddToLog( shape->GetName() );
	tmp = ShallowFindList(pnroot,"parent" );	
	// tmp = (parent name)
	if( tmp != NULL )
		shape->SetParentName(tmp->getElement(1)->getValue());


	// ------------------------------------------------
	tmp = ShallowFindList( pnroot , "geometry" );

	// mesh = ( mesh name (vertex ... ) (tri-fs ... ))
	mesh = ShallowFindList( tmp, "mesh");
	
	
	// if there is no mesh skip
	if( mesh != NULL )
	{
		// get the mesh name, if its there
		if( mesh->getElement(1)->isAtom() )
			shape->SetMeshName ( mesh->getElement(1)->getValue() );

		
		tmp = ShallowFindList( mesh , "vertex" );
		if(tmp )
		{
			// tmp = ( vertex ( (v1) (v2) .. )) 
			tmp = tmp->getElement(1);// tmp = ( (v2) (v2) .. )

			float vert[3] ;
			// load up the vertex
			for( i = 0 ;i < tmp->getNumElements() ; i++ )
			{
				
				LTA::ParseVector( tmp->getElement(i), vert );
				// shape->vertex[i] = new vert
				shape->GetMesh().vertex.push_back( 
						LTVector(vert[0],vert[1],vert[2]));
			}
		}

		tmp = ShallowFindList( mesh , "normals" );
		
		if(tmp )
		{
			// tmp = ( vertex ( (v1) (v2) .. )) 
			tmp = tmp->getElement(1);// tmp = ( (v2) (v2) .. )
			float vert[3] ;
			// load up the vertex
			for( i = 0 ;i < tmp->getNumElements() ; i++ )
			{	
				LTA::ParseVector( tmp->getElement(i), vert );
				shape->GetMesh().normals.push_back(
						LTVector(vert[0],vert[1],vert[2]));	
			}

			tmp = ShallowFindList( mesh, "nrm-fs") ;
			if(tmp){
				tmp = tmp->getElement(1) ; // get the list 
				for( i = 0 ;i < tmp->getNumElements() ; i++ )
				{
					int val = atoi(tmp->getElement(i)->getValue());
					//shape->uv_fs.push_back( val );
					shape->GetMesh().tri_NormSet.push_back( val );
				}
			}

		}

		tmp = ShallowFindList( mesh, "tri-fs");
		if(tmp)
		{
			// tmp = ( 1 2 3... )
			tmp = tmp->getElement(1) ; // get the index list
			for( i =  0 ; i < tmp->getNumElements() ; i++ )
			{
				int val = atoi( tmp->getElement(i)->getValue() ) ;
				//shape->triangles.push_back( val );
				shape->GetMesh().tri_FaceSet.push_back( val ) ;
			}
		}

		// get the uv coords 
		tmp = ShallowFindList( mesh, "uvs" );
		if( tmp )
		{
			float vert[2];
			tmp = tmp->getElement(1) ; // get the param
			for( i = 0 ; i < tmp->getNumElements() ; i++ )
			{
				LTA::ParseVector( tmp->getElement(i), vert );
				shape->GetMesh().tcoords.push_back( LTVector( vert[0], vert[1], 0 ));
			}
		

			tmp = ShallowFindList( mesh, "tex-fs") ;
			if(tmp){
				tmp = tmp->getElement(1) ; // get the list 
				for( i = 0 ;i < tmp->getNumElements() ; i++ )
				{
					int val = atoi(tmp->getElement(i)->getValue());
					//shape->uv_fs.push_back( val );
					shape->GetMesh().tri_UVSet.push_back( val );
				}
			}
			else 
			{
				CLTATranslStatus::AddToLog("WARNING : " );
				CLTATranslStatus::AddToLog(shape->GetName());
				CLTATranslStatus::AddToLog(" has no texture coords/index\r\n");
				// *** should report that there was no texture face set
			}
		}

		tmp = ShallowFindList( mesh , "colors" );
		if(tmp )
		{
			// tmp = ( vertex ( (v1) (v2) .. )) 
			tmp = tmp->getElement(1);// tmp = ( (v2) (v2) .. )

			float vert[3] ;
			// load up the vertex
			for( i = 0 ;i < tmp->getNumElements() ; i++ )
			{
				
				LTA::ParseVector( tmp->getElement(i), vert );
				// shape->vertex[i] = new vert
				shape->GetMesh().colors.push_back( 
						LTVector(vert[0],vert[1],vert[2]));
			}
		}


	}// if there's a mesh node 

	// set up the appearance node.
	CLTA_ABC_PC_MAT *mat = NULL ;
  	tmp = ShallowFindList( pnroot , "appearance" );
	if( tmp != NULL && (tmp = tmp->getElement(1)) != NULL )
	{
		// we have a mat node
		if( strcmp( tmp->getElement(0)->getValue(),  "material") == 0 || 
			strcmp( tmp->getElement(0)->getValue() , "pc-mat") == 0) 
		{
			mat = new CLTA_ABC_PC_MAT ;
			shape->SetAppearance( mat );

			CParseNode *mat_attrib ;

			mat_attrib = ShallowFindList( tmp, "specular-power");
			if( mat_attrib )
				mat->m_specular_power = (float)atof( mat_attrib->getElement(1)->getValue() );

			mat_attrib = ShallowFindList( tmp, "specular-scale");
			if( mat_attrib )
				mat->m_specular_scale = (float)atof( mat_attrib->getElement(1)->getValue() );

			mat_attrib = ShallowFindList( tmp, "texture-index");
			if( mat_attrib )
				mat->m_texture_index = atoi( mat_attrib->getElement(1)->getValue() );

			mat_attrib = ShallowFindList( tmp, "envtex-index");
			if( mat_attrib )
				mat->m_envtex_index = atoi( mat_attrib->getElement(1)->getValue() );

			mat_attrib = ShallowFindList( tmp, "diffuse");
			if( mat_attrib )
			{
				char buf[1024];
				LTA::ParseVector( mat_attrib->getElement(1), mat->m_diffuse );
				// talon: formatted and never used (0x412fee)
				sprintf( buf, "diffuse %f %f %f %f\r\n", 
						 mat->m_diffuse[0], mat->m_diffuse[1], 
						 mat->m_diffuse[2], mat->m_diffuse[3] );
			}

			mat_attrib = ShallowFindList( tmp, "ambient");
			if( mat_attrib )
				LTA::ParseVector( mat_attrib->getElement(1), mat->m_ambient );

			mat_attrib = ShallowFindList( tmp, "emissive");
			if( mat_attrib )
				LTA::ParseVector( mat_attrib->getElement(1), mat->m_emissive );

			mat_attrib = ShallowFindList( tmp, "specular");
			if( mat_attrib )
				LTA::ParseVector( mat_attrib->getElement(1), mat->m_specular );

			mat_attrib = ShallowFindList( tmp, "shininess");
			if( mat_attrib )
				mat->m_shininess = (float)atof( mat_attrib->getElement(1)->getValue() );
		}
		else
		{
			CLTATranslStatus status ;
			status << tmp->getElement(0)->getValue() ;
			status << " is an unrecognized material for shape " ;
			status << shape->GetName().c_str() ;
			status << "\r\n" ;
			status.OnError( NULL, 0 );
		}
	}
}

// ------------------------------------------------------------------------
// ShapeFromPN
// ------------------------------------------------------------------------
void 
LTA::ShapeFromPN( MetaModel & metaModel, CParseNode *pnroot )
{
	Shape *shape = new Shape ;
	ParseShapeNode( shape, pnroot );
	metaModel.HookShapeToParent( shape );
	metaModel.shapes.push_back( shape );
}


// ------------------------------------------------------------------------
static int
GetTransformFromPN( Node *node, CParseNode *pnroot )
{
	CParseNode *children=NULL, *matrix ;

	// set up node
	node->mat.Identity();

	// check if the list is a transform 
	if( strcmp( pnroot->getElement(0)->getValue() , "transform" ) == 0)
	{
		// get the transform's name if there is one
		if( pnroot->getNumElements() > 1 && pnroot->getElement(1)->isAtom() )
		{
			node->SetName( pnroot->getElement(1)->getValue() );	
		}

		// get the matrix from this transform
		matrix = ShallowFindList( pnroot, "matrix" );

		// plow through matrix list to get the basis
		if( matrix != NULL && matrix->isList())
		{
			matrix = matrix->getElement(1) ;
			// matrix = ( (...) (...) (...) (...) ) 

			LTMatrix mat;
		
			for( int i = 0 ; i < 4 ; i ++ )
			{
				LTA::ParseVector( matrix->getElement(i),mat.m[i]);
			}
		
			node->mat  = mat ;
		}
		// get transform's children
		children = ShallowFindList( pnroot, "children");
	}
	// this node is a 'null' node 
	else  	if(strcmp( pnroot->getElement(0)->getValue() , "children" ) == 0 )
	{
		node->SetName("null");
		children = pnroot ;
	}
	// we have a child shape
	else if( strcmp( pnroot->getElement(0)->getValue() , "shape" ) == 0 )
	{
		CLTATranslStatus status;
		status.m_msg = "Shapes in hier not tested ... " ;
		status.OnError("Parsing Hierarchy",0);
	
		Shape *shape = new Shape ;
		ParseShapeNode( shape, pnroot );
		node->AddShape( shape );
	
	}

	// if children 
	if( children != NULL && children->getNumElements() > 0)
	{
		// pop the children list to the list of transforms
		children = children->getElement(1) ;// ( (t ...) (t ...) (t ...) )
		int index = node->index +1;
		for( int i = 0 ;i < (int)children->getNumElements() ; i++ )
		{
			Node *chld = new Node ;
			chld->parent = node ;
			chld->index = index ;
			
			if(GetTransformFromPN(chld, children->getElement(i) ))
			{
				node->children.push_back( chld );
				index++;
			}
			else {
				delete chld ;
			}	
		}
	}
	return 1;
}



// ------------------------------------------------------------------------
// KeyFrameFromPN
// KeyFrame from a parsenode 
// keyframe format : the name is optional.
//	(keyframe name 
//	         (times ( val val val )) 
//	         (value ( val val val )))
// ------------------------------------------------------------------------
static 
void KeyFrameFromPN( CParseNode *kf , CKeyFrame & KF, CLTATranslStatus & status  )
{
	// check that pn is a keyframe 
	if( kf->getElement(0)->isAtom() )
	{
		string name = kf->getElement(0)->getValue();
		if( name != "keyframe" )
		{
			status = status.FAIL;
			status.m_msg = "parse node is not a keyframe node " ;
			return ;
		}else {
			// get name if there's one ..
			if( kf->getElement(1)->isAtom()  )
			{
				KF.SetName( kf->getElement(1)->getValue() );
			}
		}

		CParseNode *times = FindList( kf , "times" );
		CParseNode *value = FindList( kf , "values");

		if( times  ) 
		{		
			// go through the times list
			// the second element the times node is the value list
			times = times->getElement(1) ; 
			

			for( int i = 0 ; i < (int)times->getNumElements() ; i++ )
			{
				int ival = atoi(times->getElement(i)->getValue()) ;
			 
				KF.Add( ival, i  );
			}
		}

		// fix the time range so it starts at zero.
		KeyFrameTimeSlideToZero( KF );

		// parse (value ( ... ) )
		if( value )
		{
			value = value->getElement(1) ;
	
			// cut out of func here if value is null ... 
			if( value == NULL ) 
				return ;

			for( int i = 0 ; i < (int)value->getNumElements() ; i++ )
			{
				
				string str_val ;
				
				// if there's an associated value...
				if( value->getElement(i) != NULL && value->getElement(i)->getValue() != NULL )
				{
					const char *cval = value->getElement(i)->getValue();
					
					if( value && value->getElement(i)->isString() && strlen(cval) > 2 )
						str_val = value->getElement(i)->getValue() ;
				}
			 
				KF.AddValue( i , str_val );

			}// for the items in the value node 
		}// value node is there
	}// if parse nod eis a kf
}

// ------------------------------------------------------------------------
// AddAnimNodesToAnimSet
// returns 1 if function really changed/added data anim
// little thing to remember: data will not get filtered out here, if a file
// that has more animation channels than connections : (anim "foo" (parent "bif" )
// and there's no bif, well it gets read in anyway, let the consumer of the metaModel 
// deal with extra, unused data.
// ------------------------------------------------------------------------
static
int AddAnimNodesToAnimSet(  CParseNode *anim, 
							CAnimSet  *pCurAnimSet,
						    MetaModel & metaModel, 			   
						    CLTATranslStatus & status )
{
	CParseNode *keyF, *frames , *parent;
	CAnim *pAnim ;
	CAnimSet *pAnimSet = pCurAnimSet;
	string animName ;
	int    bHasFrameData=0 ; // t/f if there has been data read in

	// get the animation name 
	if( anim->getElement(1)->isAtom () )
	{
		string name ;
		animName = anim->getElement(1)->getValue();
	}

	// get the attribs
	keyF =  ShallowFindList( anim, "keyframe");
	frames= ShallowFindList( anim, "frames" );
	parent= ShallowFindList( anim, "parent" );

	if( frames == NULL )
	{
		status = status.FAIL ;
		status.m_msg = "no frames in this anim node " ;
		return bHasFrameData;
	}
	
	// get the frame attribs' data which is a list with its first element is the 
	// type 
	frames = frames->getElement(1) ;// (frames (type ... )) => (type ... )
	if( frames != NULL )
	{
		assert( frames->getElement(0) != NULL );
		string type = frames->getElement(0)->getValue();
	
		if( type == "vertex" )
		{
			CVtxAnim *pVtxAnim = new CVtxAnim ;
			int i ;
			Shape *shape = NULL ;
			
			// find the anim target 
			if( parent != NULL )
			{
				string parent_name = parent->getElement(1)->getValue();

				shape = metaModel.FindShape( parent_name );
				if ( shape == NULL )
				{
					// no shape error 
					//status = status.FAIL ;
					status.m_msg = " this vertex animation has no target .. " ;
				//	delete pVtxAnim ;
				//	return bHasFrameData;
				}
				pVtxAnim-> SetTarget( shape );

			}else {
				// error no target
			}
			
			
			// fish out the vertex animations
			CParseNode *tmp;
			// tmp = a list of list of vertex
			tmp = frames->getElement(1);// tmp = ( (v2) (v2) .. )
			int tmpval = tmp->getNumElements();
			for( int nLists = 0 ; nLists < (int)tmp->getNumElements() ; nLists++ )
			{
				CParseNode *vecList = tmp->getElement(nLists) ;
				
				vector<LTVector> vVecs ;
				float vert[3] ;
				int tmpval2 = vecList->getNumElements();
				for( i = 0 ;i < (int)vecList->getNumElements() ; i++ )
				{
					
					LTA::ParseVector( vecList->getElement(i), vert );
					vVecs.push_back(LTVector(vert[0],vert[1],vert[2]));	
				}
				
				pVtxAnim->AppendVertexList( vVecs );
			}

			if (tmp->getNumElements() > 0 ) 
				bHasFrameData = 1;

			pAnim = pVtxAnim ;
		}
		else if( type == "posquat" ) 
		{
			CPosQuatAnim *pPQAnim = new CPosQuatAnim ;
			Node *node = NULL ;

			if( parent != NULL )
			{
				string parent_name = parent->getElement(1)->getValue();
				Node *node = metaModel.FindNode( parent_name );
				if ( node == NULL )
				{
					// no shape error 
					//status = status.FAIL ;
					status.m_msg = parent_name ;
					status.m_msg += " " ;
					status.m_msg += " this POS/QUAT animation has no target .. " ;
			//		delete pPQAnim ;
			//		return bHasFrameData;
				}
				pPQAnim->SetTarget( node );
			}
		
			// read in pos-quats
			float pos[3];
			float quat[4];
			CParseNode *tmp;
			// tmp = a list of list of vertex
			tmp = frames->getElement(1);// tmp = ( (v2) (v2) .. )
			int tmpval = tmp->getNumElements();
			for( int nLists = 0 ; nLists < (int)tmp->getNumElements() ; nLists++ )
			{
				CParseNode *posQuatLst = tmp->getElement(nLists) ;
			
				int tmpval2 = posQuatLst->getNumElements();
				
				LTA::ParseVector( posQuatLst->getElement(0), pos );
				LTA::ParseVector( posQuatLst->getElement(1), quat);
	
				pPQAnim->Append( pos, quat );
			}

			bHasFrameData = tmp->getNumElements() > 0 ;
			pAnim = pPQAnim ;			
			
		} // pos quat anim
	} // has frame s

	// get keyframe from anim node
	// if there is no KF node in lta, get it from the anim set
	if( keyF != NULL )
	{
		CKeyFrame KeyFrame ;
		CLTATranslStatus kfstatus ;
		KeyFrameFromPN( keyF, KeyFrame , kfstatus );
		if( kfstatus != kfstatus.OK ){
			kfstatus.OnError("AddAnimNodeToAnimSet",true );
			status = kfstatus ;
			return 0;
		}

		pAnim->SetKeyFrameCopy( KeyFrame );
	}
	else 
	{
		pAnim->ShareKeyFrame( pAnimSet->GetKeyFrame()) ;
		// that's ok use anim sets' kf
	}

	// finalize this animatin & append it to set.
	pAnim->SetName( animName );

	// if there is no data, don't add the animation to the set.
	if( bHasFrameData )
		pAnimSet->AddAnim( pAnim );
	else 
		delete pAnim ;

	return bHasFrameData ;
}


// --------------------------------------------
// AnimSetFromAnimSetPN( meta-model, parse-tree = (animset ...), status 
// --------------------------------------------
void LTA::AnimSetFromPN( MetaModel & metaModel,
							CParseNode *animsetPN,
							CLTATranslStatus & status )
{
		CAnimSet *pAnimSet = new CAnimSet ;
		CParseNode *KFNode, *AnimList ;
		if( pAnimSet == NULL )
		{
			status = status.OUT_OF_MEM ;
			status.m_msg = " OUT OF MEMORY!!! ALLOC CAnimSet ";
			return ;
		}
		// get the anim set name 
		// if no name then anim + i 
		string name ;
		if( animsetPN->getElement(1)->isAtom() )
		{
			name = animsetPN->getElement(1)->getValue() ;
			CLTATranslStatus::AddToLog( name.c_str() );
			
		}else 
		{
			char buf[34];
			itoa( 0 , buf, 10 );
			name = "base" ;
			name += buf ;
		}

		pAnimSet->SetName( name );

		KFNode = ShallowFindList( animsetPN, "keyframe" );

		if( KFNode != NULL )
		{
			CKeyFrame KeyFrame ;
			CLTATranslStatus kfstatus ;
			// KFNode = (keyframe (keyframe ... )  we want elem 2
			KFNode = KFNode->getElement(1) ;
			if( KFNode )
				KeyFrameFromPN( KFNode, KeyFrame , kfstatus );
			if( kfstatus != kfstatus.OK ){
				//MessageBox(NULL, kfstatus.m_msg.c_str(), "AnimSetFromPN", MB_OK);
				kfstatus.OnError("AnimSetFromPN");
				status = kfstatus ;
				return ;
			}
			pAnimSet->SetKeyFrame( KeyFrame );
		}

		// AnimList => (anims ( ... ) )
		AnimList = ShallowFindList( animsetPN, "anims" );
		int bAddAnim = 0 ; // return value from AddAnimNodesToAnimSet
		if( AnimList ){
			//AnimList => ( (anim ... ) (anim ... ) )
			AnimList = AnimList->getElement(1) ;

 			for( int i = 0 ; i < (int)AnimList->getNumElements() ; i++ )
			{
				bAddAnim = 1;
				AddAnimNodesToAnimSet( AnimList->getElement(i), pAnimSet, metaModel, status );
			}
		}

		//if ( status != status.OK )
		//{
			//status.OnError("comming back from AddAnimNodeToAnimSet");
			//	return ;//
		//}

		if( bAddAnim ) 
			metaModel.AppendAnimSet( pAnimSet );
		else delete pAnimSet ;
	
}
// ------------------------------------------------------------------------
// HierarchyFromPN( 
// get the hierarchy from the pn.
// one caveat :
// if the hierarchy has more than one child, we create a child an call it
// the root node. If the hier has more than one child, that means that 
// the modeler has not collected all the nodes under one root. This should
// be ok.
/*
1. (h name (childs (  (a) (b) ))) 
2. (h name (childs ( (a (childs ( (c) (d) ))))))
*/
// ------------------------------------------------------------------------
 
void LTA::HierarchyFromPN(	MetaModel & metaModel,
							CParseNode *pnroot,
							CLTATranslStatus & status )

{
	// build hier
	// get heir.
	// get children list from there for every child call GetTransformFromPN
	CParseNode *lst ;
	lst = FindList( pnroot, "hierarchy");
	if( lst == NULL )
	{
		status.m_status = status.FAIL ; 
		status.m_msg = "this model has no hierarchy!! " ;
		status.OnError(" processing Hierarchy ", LT_FALSE );
		return ;
	}

	string hierName ;
	// lst = (hier name (children (...)))
	if( lst->getElement(1)->isAtom() )
	{
		hierName = lst->getElement(1)->getValue();
	}else {
		hierName = "node_root";
	}
	
	CLTATranslStatus::AddToLog( hierName );

	lst = FindList( lst, "children");
	if( lst == NULL )
	{
		status.m_status = status.FAIL ;
		status.m_msg    = "Hier has no children " ;
		status.OnError(" processing Hierarchy ", LT_FALSE );
		// error no children
		return ;
	}
	
	lst = lst->getElement(1) ; // ( children ( ... ) )

	// The hierarchy node is not a node in itself, its the 
	// model.
	// if there is more than one child, make a rootnode put children as node's childs
	// else if one child then one child is root
	if( lst->getNumElements() > 0 )
	{
		int index =0;
		Node * rootN = new Node() ;
		rootN->index = index;
		rootN->SetName("root");
		metaModel.rootNode = rootN ;
		//status.errLog() << " root has : " << lst->getNumElements() << " num children " << endl;
		if( lst->getNumElements() == 1 ){
			GetTransformFromPN( rootN, lst->getElement(0) );
			
		}else
		{
			for( int i = 0 ;i < (int)lst->getNumElements() ; i++ )
			{
				Node *newNode = new Node ;
				newNode->index = index ;
				rootN->children.push_back(newNode);
				GetTransformFromPN( newNode ,  lst->getElement(i) );
				index++;
			}
		}
	}
}

// ------------------------------------------------------------------------
// 
// ------------------------------------------------------------------------

// ------------------------------------------------------------------------
// Arbitrary value to indicate in a pair that animation should NOT be merged.
// ------------------------------------------------------------------------
const int32 kShouldNotBeMerged = -1;

int LTA::ShouldMerge( CClassifiedAnimLst & CurLst, int index )
{
	return CurLst[index].second != kShouldNotBeMerged ;
}


// ------------------------------------------------------------------------
// CollectAndClassifyAnims( metaModel, result-list )
// check the animation sets in the MetaModel for name matches. 
// a name match is an indication that a vertex animset and a pos/quat animset
// should be merged. The criteria is an animation named nameVA and name both have
// the same root name. The VA postfix indicates that it is an vertex animation.
// ------------------------------------------------------------------------
int LTA::CollectAndClassifyAnims( MetaModel & metaModel, CClassifiedAnimLst & ClassifiedAnimLst )
{
	CAnimSet *cur_anim, *next_anim ;
	string search_name ;
	int32 i ;

	if( metaModel.NumAnimSets() >= 2 )
	{
		for( i = 0 ;i < metaModel.NumAnimSets() ; i++ )
		{
			cur_anim = metaModel.GetAnimSet(i);
			int type = cur_anim->GetAnimType();
			
			if( type == CAnim::VERTEX )
			{
				string name = cur_anim->GetName();
				search_name = name.substr(0,name.size()-2);
				next_anim = metaModel.GetAnimSet(i+1);
				if( next_anim != NULL )
				if( search_name == next_anim->GetName() ) 
				{
						ClassifiedAnimLst.push_back( pair<int,int>( i , i+1 )  );
						i++; // skip the next_anim next time around the loop
				}
			}else
			{
				ClassifiedAnimLst.push_back( pair<int,int>( i, kShouldNotBeMerged ) );
			}
		}
	}
	else // we have only one or no animations, if 1 classify single anim as a non merge.
	{
		if( metaModel.NumAnimSets() == 1 )
		{
			ClassifiedAnimLst.push_back( pair< int, int > ( 0, kShouldNotBeMerged ) ) ;
		}
	}

	return ClassifiedAnimLst.size();
}

