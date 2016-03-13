#include "stdafx.h"
#include "znsxt.h"
#include "CableInfoDlg.h"
#include "XhlCableInfoDlg.h"
#include "znutility.h"
#include "PolylineUtil.h"
#include "ConvertUtil.h"
#include "EntPropertyDlg.h"
#include "BlockUtil.h"

#include "..\blkuserdll\ViewExtents.h"
#include "BoxNodeEnt.h"

//////////////////////////////////////////////////////////////////////////
using namespace std;
//为了排序连接到盒子的线缆（按逆时针和HF相交）
struct ConLineInfo
{
	long nHandle ;
	double dAngle ;
	bool operator >(const ConLineInfo& info) const ;
	bool operator <(const ConLineInfo& info) const ;
};
bool ConLineInfo::operator >(const ConLineInfo& info) const
{
	return dAngle>info.dAngle;
}
bool ConLineInfo::operator <(const ConLineInfo& info) const
{
	return dAngle<info.dAngle;
}
//////////////////////////////////////////////////////////////////////////

CZnSxt::CZnSxt(void)
: m_pDb(NULL)
, m_bFillPolarErro(false)
, m_iCurDljlType(-1)
, m_iColorIndexCur(256)
, m_strCurNameIntoXhl(_T(""))
, m_iCurJob(0)
, m_i4Or2Line(0)
, m_iDsNumXhj(0)
, m_iQkNum(0)
, m_iDcDsNum(0)
, m_dRowHeightToXhl(10)
, m_dHeightOfDistText(2.2)
, m_dHeightOfDistText_DT(3)
, m_dMaxLenPmt(5000)
, m_bNetWithSn(TRUE)
{	
	m_AdoDb.Open(g_strAdoCon) ;
	m_RecordSet.SetConnection(&m_AdoDb) ;
    num=3;
	
	CBlkUtility::QueryValueReg(_T("ZnOption"), _T("TwoOrFourLine"), m_i4Or2Line) ;
	CBlkUtility::QueryValueReg(_T("ZnOption"), _T("XhjDsNum"), m_iDsNumXhj) ;
	CBlkUtility::QueryValueReg(_T("ZnOption"), _T("DcQkNum"), m_iQkNum) ;
	CBlkUtility::QueryValueReg(_T("ZnOption"), _T("DcDsNum"), m_iDcDsNum) ;
}

CZnSxt::~CZnSxt(void)
{
	m_RecordSet.Close() ;
	m_AdoDb.Close() ;
}

int CZnSxt::Test(void)
{
	_ApplicationPtr pExcel = NULL ;
	WorkbooksPtr pWorkBooks = NULL ;
	_WorkbookPtr pWorkBook = NULL ;
	SheetsPtr pWorksheets = NULL ; 
	RangePtr pRange = NULL ;
	_WorksheetPtr pSheet = NULL ;

	try
	{
		if (bFirst)
		{
			::AfxOleInit() ;
			bFirst = FALSE ;
		}
		HRESULT hr = pExcel.CreateInstance ( _T("Excel.Application") ) ;
		if(FAILED(hr))
		{
			AfxMessageBox( _T("初始化Excel失败,可能是没有安装Excel") ) ;
			//return ;
		}
		//使excel程序显示可见
		//pExcel->PutVisible(0,VARIANT_TRUE) ;
		//得到所有的工作簿
		pWorkBooks = pExcel->GetWorkbooks() ;
		//添加一个新的工作簿，返回结果为刚添加的工作簿的指针
		pWorkBook = pWorkBooks->Add() ;

		VARIANT vt_index ;
		vt_index.vt = VT_I2 ;
		vt_index.iVal = 1 ;		

		//得到当前工作簿的所有的sheet
		pWorksheets = pWorkBook->GetSheets() ;
		//定位于第一个sheet
		pSheet = pWorksheets->GetItem(vt_index) ;

		CADORecordset cSet(&m_AdoDb) ;

		CString strSql ;
		strSql.Format(_T("select top 35 * from define_dev_use_cablecore order by id")) ;
		if (cSet.Open(strSql))
		{
			int iRow = 0 ;
			while(!cSet.IsEOF())
			{
				CString strCellName, strCellContent ;
				long iData = 0 ;
				CString strData ;
				iRow++ ;
				for (int iCol=0; iCol<12; iCol++)
				{
					strCellName.Format(_T("%c%d"), _T('A')+iCol, iRow) ;
					iData = 0 ;
					strData = _T("") ;
					if (iCol>1&&iCol<7)
					{
						cSet.GetFieldValue(iCol, strData) ;
						strCellContent = strData ;
					}
					else
					{
						cSet.GetFieldValue(iCol, iData) ;
						strCellContent.Format(_T("%d"), iData) ;
					}
					pRange = NULL ;				
					pRange = pSheet->GetRange((_variant_t)strCellName, vtMissing) ;
					pRange->FormulaR1C1 = (_variant_t) strCellContent ;
				}


				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}

		//保存excel文件
		VARIANT varOptional ;
		VariantInit(&varOptional);
		varOptional.vt		= VT_ERROR;
		varOptional.scode	= (long)DISP_E_PARAMNOTFOUND;		

		CString strSaveFileName  = _T("D:\\1.xls") ;
		pWorkBook->SaveAs( (_variant_t) strSaveFileName, COleVariant(varOptional), COleVariant(varOptional), 
			COleVariant(varOptional), COleVariant(varOptional), COleVariant(varOptional), xlExclusive, 
			COleVariant(varOptional), COleVariant(varOptional), COleVariant(varOptional), COleVariant(varOptional)) ;
		//////////////////////////////////////////////////////////////////////////
		pWorkBook->Close( ) ;
		pWorkBook = NULL ;
		pExcel->Quit() ;
		pExcel = NULL ;
		//end double for loop
		//////////////////////////////////////////////////////////////////////////
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);  
		pWorkBooks->Close() ;
		pWorkBooks = NULL ;
		pExcel->Quit() ;
		pExcel = NULL ;		
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!"));		
		pWorkBooks->Close() ;
		pWorkBooks = NULL ;
		pExcel->Quit() ;
		pExcel = NULL ;		
	}
	return 0;

}

//查询和pEntSrc相交的多段线，若是定位相交的线，则返回多段线的段数（base 0）,若是反位相交的，返回多段线的顶点序号（base 0）
//iMode:默认-1表示反位相交情况，iMode=100，表示信号机绝缘节相交情况，iIndexOfVertex需要返回相交的Vertex序号（base 0）;其他表示定位相交，iIndexOfVertex需要返回相交的段数（base 0）
int CZnSxt::GetIntersectPLine(AcDbEntity *pEntSrc, UINT &nHandleIntersectEnt, UINT &nIndexOfVertex, AcGePoint3d &pt_3d_intersect, int &iDwIntersectMoveToVertex, int iMode)
{
	int iRet = 0 ;

	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			//只查询相交的多段线
			if (!pEnt->isEqualTo(pEntSrc)&&pEnt->isKindOf(AcDbPolyline::desc()))
			{
				long nHandle = m_mysys.GetHandle(pEnt) ;
// 				if (nHandle==289)
// 				{
// 					int iiii = 0 ;
// 				}
				AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;
				AcGePoint3dArray arrayPt3d ;
				if(Acad::eOk==pEntSrc->intersectWith(pEnt, AcDb::kOnBothOperands, arrayPt3d) )
				{
					int iPtNum = arrayPt3d.length() ;
					//正常情况应该只有一个交点
					if (iPtNum==1)
					{
						nHandleIntersectEnt = m_mysys.GetHandle(pEnt) ;
						pt_3d_intersect = arrayPt3d.at(0) ;
						iRet++ ;

						int iPtOnPolyRet = m_mysys.IsPointOnPolyLine(pPolyLine, pt_3d_intersect, nIndexOfVertex) ;
						if (iMode==0) //定位
						{	
							//确定绘制双线图的三线交点（其实是两线交点，定位那根线是折点分成的2段）
							//判断定位那个交点离定位线折点有没有小于等于2的，若有就将三线交点设在折点，否则就是定位交点pt_3d_intersectDW
							AcGePoint3d pt_3d_vertex1(0, 0, 0), pt_3d_vertex2(0, 0, 0) ;
							pPolyLine->getPointAt(nIndexOfVertex, pt_3d_vertex1) ;
							pPolyLine->getPointAt(nIndexOfVertex+1, pt_3d_vertex2) ;
							if (pt_3d_intersect.distanceTo(pt_3d_vertex1)<=2.3)
							{
								pt_3d_intersect = pt_3d_vertex1 ;
								iDwIntersectMoveToVertex = nIndexOfVertex ;
							}
							else if (pt_3d_intersect.distanceTo(pt_3d_vertex2)<=2.3)
							{
								pt_3d_intersect = pt_3d_vertex2 ;
								iDwIntersectMoveToVertex = nIndexOfVertex+1 ;
							}
							//20140430-1627,here
							//交点不在折点则需要给该多段线增加一个折点（交点位置）
// 							else 
// 							{
// 								AcGePoint2d pt_2d_intersect(pt_3d_intersect.x, pt_3d_intersect.y) ;
// 								pPolyLine->upgradeOpen() ;
// 								pPolyLine->addVertexAt(nIndexOfVertex+1, pt_2d_intersect) ;
// 								pPolyLine->downgradeOpen() ;
// 							}

							//acutPrintf(_T("\n定位iIndexOfSeg: %d"), nIndexOfVertex) ;				
						}
						else if (iMode==-1)//反位
						{
							AcGePoint3d pt_3d_vertex1(0, 0, 0), pt_3d_vertex2(0, 0, 0) ;
							pPolyLine->getPointAt(nIndexOfVertex, pt_3d_vertex1) ;
							pPolyLine->getPointAt(nIndexOfVertex+1, pt_3d_vertex2) ;
							if (pt_3d_intersect.distanceTo(pt_3d_vertex1)>pt_3d_intersect.distanceTo(pt_3d_vertex2))
							{
								nIndexOfVertex = nIndexOfVertex+1 ;
							}
							//acutPrintf(_T("\n反位iIndexOfvertex: %d"), nIndexOfVertex) ;		
						}
						else if (iMode==100)//信号机、绝缘节
						{
						}

						CString strPt ;
						strPt.Format(_T("%d"), iPtNum) ;
						for (int iPtIndex=0; iPtIndex<iPtNum; iPtIndex++)
						{
							strPt.AppendFormat(_T("(%.3f-%.3f)"), arrayPt3d.at(iPtIndex).x, arrayPt3d.at(iPtIndex).y) ;
						}
						//acutPrintf(_T("\n一个交点:%d, 交点：%s"), nHandleIntersectEnt, strPt) ;
					}
					else if(iPtNum>1)
					{
						CString strPt ;
						strPt.Format(_T("%d"), iPtNum) ;
						for (int iPtIndex=0; iPtIndex<iPtNum; iPtIndex++)
						{
							strPt.AppendFormat(_T("(%.3f-%.3f)"), arrayPt3d.at(iPtIndex).x, arrayPt3d.at(iPtIndex).y) ;
						}
						//acutPrintf(_T("\n不止一个交点:%d, 交点：%s"), nHandleIntersectEnt, strPt) ;
					}
				}
			}

			pEnt->close() ;
		}		
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return iRet ;
}

//查询和pEntSrc(信号机或绝缘节)相交的多段线，返回值：1,1个交点在顶点,顶点所在线在当前信号机或者绝缘节的左侧；5,1个交点在顶点,顶点所在线在当前信号机或者绝缘节的右侧
int CZnSxt::GetIntersectPLine(AcDbEntity *pEntSrc, CLongArray &nArrayHandleIntersect, CLongArray &nArrayIndexOfVertex, AcGePoint3dArray &pt3dArrayIntersect)
{
	int iRet = 0 ;

	int iPtOnPLinePre = -1 ; //记录上一次交点在多段线上的情况：1000，在多段线非顶点位置，0~1000，在多段线顶点位置。若上次为1000，第二次为任何大于-1的值都是有问题的；若上次为0~1000，则第二次必须也为0~1000才行

	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	int iIndexEnt = 0 ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			//只查询相交的多段线
			if (!pEnt->isEqualTo(pEntSrc)&&pEnt->isKindOf(AcDbPolyline::desc()))
			{
				AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;
				AcDb::Visibility bIsVisible  = pPolyLine->visibility() ;
				double dLenOfPline = m_mysys.GetLineLen(pPolyLine) ;
				if (bIsVisible==AcDb::kInvisible||m_mysys.IsOnOffLayer(pEnt)||dLenOfPline<1)
				{
					pEnt->close() ;
					continue;
				}
				iIndexEnt++ ;

				AcGePoint3dArray arrayPt3d ;
				Acad::ErrorStatus es = pEntSrc->intersectWith(pEnt, AcDb::kOnBothOperands, arrayPt3d);
				int iPtNum = arrayPt3d.length() ;

				if(es==Acad::eOk && iPtNum>0)
				{
					UINT nIndexOfVertex = 0 ;
					UINT nHandleIntersectEnt = 0 ;
					nHandleIntersectEnt = m_mysys.GetHandle(pEnt) ;
					AcGePoint3d pt_3d_intersect(0,0,0) ;
					pt_3d_intersect = arrayPt3d.at(0) ;

					//正常情况应该只有一个交点
					if (iPtNum==1)
					{
						int iPtOnPLine = m_mysys.IsPointOnPolyLine(pPolyLine, pt_3d_intersect, nIndexOfVertex) ;
						int iNumOfVertex = pPolyLine->numVerts() ;
						if (iPtOnPLinePre>-1&&iPtOnPLinePre<1000) //前一个交点是顶点
						{
							if (iPtOnPLine>-1&&iPtOnPLine<1000)//当前交点也是顶点
							{
								nArrayHandleIntersect.Add(nHandleIntersectEnt) ;
								nArrayIndexOfVertex.Add(iPtOnPLine) ;
								pt3dArrayIntersect.append(pt_3d_intersect) ;
								if (iPtOnPLine==0)
								{
									iRet =5*10 + iRet ; //顶点所在线在当前信号机或者绝缘节的右侧
								}
								else if (iPtOnPLine==iNumOfVertex-1)
								{
									iRet = 1*10 +iRet ; //顶点所在线在当前信号机或者绝缘节的左侧
								}
								else
								{
									iRet = 0 ; //erro
								}
							}
							else
							{
								acutPrintf(_T("\n前交点是顶点，当前交点不是。交点为（%.3f, %.3f）,handle为%d，第%d段"), pt_3d_intersect.x, pt_3d_intersect.y, nHandleIntersectEnt, nIndexOfVertex) ;
								iRet = 2*10 +iRet;
							}
						}
						else if (iPtOnPLinePre==1000)//前一个交点不是顶点
						{
							acutPrintf(_T("\n前交点不是顶点，现在有出现一个交线，请检查图面。交点为（%.3f, %.3f）,handle为%d，第%d段"), pt_3d_intersect.x, pt_3d_intersect.y, nHandleIntersectEnt, nIndexOfVertex) ;
							iRet = 0 ;
						}
						else if (iPtOnPLinePre==-1)
						{
							nArrayHandleIntersect.Add(nHandleIntersectEnt) ;
							if (iPtOnPLine>-1&&iPtOnPLine<1000)//当前交点是顶点
							{
								nArrayIndexOfVertex.Add(iPtOnPLine) ;
								if (iPtOnPLine==0)
								{
									iRet = 5 ; //顶点所在线在当前信号机或者绝缘节的右侧
								}
								else if (iPtOnPLine==iNumOfVertex-1)
								{
									iRet = 1 ; //顶点所在线在当前信号机或者绝缘节的左侧
								}
								else
								{
									iRet = 0 ; //错误
								}
							}
							else //有交点，但交点不在顶点，那肯定在直线上了（即信号机/绝缘节位于直线上online）
							{
								nArrayIndexOfVertex.Add(nIndexOfVertex) ;
								iRet = 2 ;
							}							
							pt3dArrayIntersect.append(pt_3d_intersect) ;							
						}
						iPtOnPLinePre = iPtOnPLine ;	
						//acutPrintf(_T("\n%d:cur pline handle:%d, iRet:%d"), iIndexEnt, m_mysys.GetHandle(pEnt), iRet) ;

					}
					else if(iPtNum>1)
					{
						CString strPt ;
						strPt.Format(_T("%d"), iPtNum) ;
						for (int iPtIndex=0; iPtIndex<iPtNum; iPtIndex++)
						{
							strPt.AppendFormat(_T("(%.3f-%.3f)"), arrayPt3d.at(iPtIndex).x, arrayPt3d.at(iPtIndex).y) ;
						}
						acutPrintf(_T("\n不止一个交点:%d, 交点：%s"), nHandleIntersectEnt, strPt) ;
					}
				}
			}

			pEnt->close() ;
		}		
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return iRet ;
}

// 前期工作：确定道岔中心点，可能需要移动岔后连线（连在DC块屁股上的线）的及其上的实体（循环过程）
//DC定位直线的，中心点在插入点，反位的在多段线折点（顶点vertex）
int CZnSxt::PreDoGetDcCenter()
{
	AcDbDatabase *pDbCur = acdbHostApplicationServices()->workingDatabase() ;	
	m_pDb = pDbCur ;
	m_mysys.m_pDb = pDbCur ;

	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;

		//遍历出所有DC_00块
		CArray<long, long> nArrayHandleDC ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type=0")) ;		
		int nNumDc = m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nArrayHandleDC) ;
		for (int i=0; i<nArrayHandleDC.GetCount(); i++)
		{
			long nHandleDC  = nArrayHandleDC.GetAt(i), nHandleDW  = 0, nHandleFW  = 0;

			strSql.Format(_T("select * from sxt_ent where sxt_ent_type=0 and sxt_ent_handle=%d"), nHandleDC) ;
			AcGePoint2d pt_2d_dwcon(0,0) ; //DW连接点
			//m_RecordSet.GetRecordValue(strSql,_T("sxt_ent_handle"), nHandleDC) ;
			m_RecordSet.GetRecordValue(strSql,_T("sxt_ent_online_handle"), nHandleDW) ;
			m_RecordSet.GetRecordValue(strSql,_T("sxt_ent_online_intersectx1"), pt_2d_dwcon.x) ;
			m_RecordSet.GetRecordValue(strSql,_T("sxt_ent_online_intersecty1"), pt_2d_dwcon.y) ;
			m_RecordSet.GetRecordValue(strSql,_T("sxt_ent_fwline_handle"), nHandleFW) ;
			long iFwVertex = 0 ; //道岔块反位连线的vertex base 0（不是终点就是起点）
			m_RecordSet.GetRecordValue(strSql,_T("sxt_ent_fwline_vertex"), iFwVertex) ;
			AcGePoint2d pt_2d_fwcon(0,0) ; //FW连接点
			strSql.Format(_T("select * from sxt_line where  sxt_line_handle=%d and sxt_line_vertex=%d"), nHandleFW, iFwVertex) ;				
			m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x1"), pt_2d_fwcon.x	) ;
			m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y1"), pt_2d_fwcon.y	) ;
			AcGePoint2d pt_2d_rotate(0,0) ; //旋转基点，即FW连接点所在段的另一点
			strSql.Format(_T("select * from sxt_line where  sxt_line_handle=%d and sxt_line_vertex=%d"), nHandleFW, iFwVertex==0?(iFwVertex+1):(iFwVertex-1)) ;				
			m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x1"), pt_2d_rotate.x) ;
			m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y1"), pt_2d_rotate.y) ;

			double dAngleToMove = 0.000 ;
			AcGeVector2d vecOld, vecNew ;
			vecOld = pt_2d_rotate-pt_2d_fwcon ;
			vecNew = pt_2d_rotate-pt_2d_dwcon ;
			dAngleToMove = vecNew.angle()-vecOld.angle() ;

			//先将当前fw线的连接点移动到dwcon点,仅更新数据库值				
			strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex=%d"),  nHandleFW, iFwVertex) ;
			m_RecordSet.SetRecordValue(strSql, _T("sxt_line_vertex_x1"), pt_2d_dwcon.x) ;
			m_RecordSet.SetRecordValue(strSql, _T("sxt_line_vertex_y1"), pt_2d_dwcon.y) ;

			this->SearchToMoveEnt(nHandleFW, iFwVertex, pt_2d_rotate, dAngleToMove) ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in PreDoGetDcCenter"));
	}

	return 0;
}

// 取得和DC块定/反位方向连接的线
BOOL CZnSxt::GetDcConLineDFW(const AcDbBlockReference *pRef, CLongArray &nArrayHandleIntersectLine, CLongArray &nArrayIndexOfVertex, AcGePoint3dArray &pt3dArrayIntersect, int &iDwIntersectMoveToVertex)
{
	BOOL bRet = FALSE ;

	AcGePoint3d pt_3d_ref = pRef->position() ;
	AcGeScale3d scale_3d = pRef->scaleFactors()  ;
	if (pRef->normal().z<0)
	{
		scale_3d.sx = -scale_3d.sx	;
	}
	double dAngle = pRef->rotation() ;

	//以DC块基点为源，确定8个点，起点为黑疙瘩反位点，第7点为黑疙瘩中心,第8点为基点下移一点和3、4点同一线上的点
	//先算其无旋转时的值，再旋转dAangle度
	double dPlus = 0.05 ; //计算点时，适当外扩一点，防止没开捕捉绘制的图
	AcGePoint2dArray pt2dArrayDC ;
	for (int i=0; i<8; i++)
	{
		AcGePoint2d pt_2d_tmp(0,0) ;
		pt2dArrayDC.append(pt_2d_tmp) ;
	}
	pt2dArrayDC[0].x = pt2dArrayDC[3].x = pt_3d_ref.x-(1+dPlus)*(scale_3d.sx<0?-1:1) ;
	pt2dArrayDC[0].y = pt2dArrayDC[1].y = pt2dArrayDC[4].y = pt_3d_ref.y+(1+dPlus) ;
	pt2dArrayDC[1].x = pt2dArrayDC[2].x = pt_3d_ref.x+1*(scale_3d.sx<0?-1:1) ;
	pt2dArrayDC[2].y = pt2dArrayDC[3].y = pt2dArrayDC[5].y = pt_3d_ref.y-dPlus ;
	pt2dArrayDC[4].x = pt2dArrayDC[5].x = pt_3d_ref.x+(2+dPlus)*(scale_3d.sx<0?-1:1) ;
	pt2dArrayDC[6].x = pt_3d_ref.x ;
	pt2dArrayDC[6].y = ((pt_3d_ref.y+(1+dPlus)) +(pt_3d_ref.y-dPlus))/2 ;
	pt2dArrayDC[7].x = pt_3d_ref.x ;
	pt2dArrayDC[7].y = pt_3d_ref.y-dPlus ;
	for(int i=0; i<8; i++)
	{
		AcGeMatrix2d matrix; 
		matrix.setToRotation(dAngle, pt_3d_ref.convert2d(AcGePlane::kXYPlane)); 
		pt2dArrayDC[i].transformBy(matrix) ; 				
	}

	//acutPrintf(_T("\n%d\n%s号道岔:position(%.3f,%.3f)"), iDcIndex, strDcName, pt_3d_ref.x, pt_3d_ref.y) ; 

	//定位方向的线
	AcGePoint2dArray pt2dArrayLine ; //测试用，绘制辅助线
	AcGePoint2d pt_2d_start(0, 0), pt_2d_end(0,0) ;	
	AcDbPolyline *polyLineTmp1 = new AcDbPolyline(2) ;
	AcGeVector2d vec1 = pt2dArrayDC[6]-pt2dArrayDC[7] ;
	vec1/=2 ; //vec1长度为0.5，需要减半
	pt_2d_start.x =pt2dArrayDC[7].x ;
	pt_2d_start.y =pt2dArrayDC[7].y ;
	pt_2d_start-=vec1 ;
	pt2dArrayLine.append(pt_2d_start) ;
	polyLineTmp1->addVertexAt(0, pt_2d_start) ;
	pt_2d_end.x =(pt2dArrayDC[6].x+pt2dArrayDC[7].x)/2;
	pt_2d_end.y =(pt2dArrayDC[6].y+pt2dArrayDC[7].y)/2 ;	
	pt2dArrayLine.append(pt_2d_end) ;
	polyLineTmp1->addVertexAt(1, pt_2d_end) ;
	UINT nHandleIntersectDW ;
	AcGePoint3d pt_3d_intersectDW(0,0,0) ;
	UINT nIndexOfVertexDW = 0 ;
	int iNumConLineDW = GetIntersectPLine(polyLineTmp1, nHandleIntersectDW, nIndexOfVertexDW, pt_3d_intersectDW, iDwIntersectMoveToVertex,0) ;
	nArrayHandleIntersectLine.Add(nHandleIntersectDW) ;
	nArrayIndexOfVertex.Add(nIndexOfVertexDW) ;
	pt3dArrayIntersect.append(pt_3d_intersectDW) ;
	delete polyLineTmp1 ;
	polyLineTmp1 = NULL ;
	//m_mysys.AddAcDbPolyLine(pt2dArrayLine, 0, 3) ;

	//反位方向的线
	pt2dArrayLine.removeSubArray(0, pt2dArrayLine.length()-1) ;
	AcGePoint2d pt_2d_tmp(0,0) ;
	AcDbPolyline *polyLineTmp2 = new AcDbPolyline(3) ;
	pt_2d_tmp.x = (pt2dArrayDC[0].x+pt2dArrayDC[1].x)/2 ;
	pt_2d_tmp.y = (pt2dArrayDC[0].y+pt2dArrayDC[1].y)/2 ;	
	pt2dArrayLine.append(pt_2d_tmp) ;
	polyLineTmp2->addVertexAt(0, pt_2d_tmp) ;
	pt_2d_tmp.x =pt2dArrayDC[0].x ;
	pt_2d_tmp.y =pt2dArrayDC[0].y ;
	pt2dArrayLine.append(pt_2d_tmp) ;
	polyLineTmp2->addVertexAt(1, pt_2d_tmp) ;
	pt_2d_tmp.x = (pt2dArrayDC[0].x+pt2dArrayDC[3].x)/2 ;
	pt_2d_tmp.y = (pt2dArrayDC[0].y+pt2dArrayDC[3].y)/2 ;
	pt2dArrayLine.append(pt_2d_tmp) ;
	polyLineTmp2->addVertexAt(2, pt_2d_tmp) ;
	UINT nHandleIntersectFW ;
	AcGePoint3d pt_3d_intersectFW(0,0,0) ;
	UINT nIndexOfVertexFW = 0 ;
// 	CString strMsg ;
// 	for (int iii = 0; iii<polyLineTmp2->numVerts(); iii++)
// 	{
// 		AcGePoint2d pt_2d_iii(0,0) ;
// 		polyLineTmp2->getPointAt(iii, pt_2d_iii) ;
// 		strMsg.AppendFormat(_T("%.3f-%.3f,"), pt_2d_iii.x, pt_2d_iii.y ) ;
// 	}
// 	acutPrintf(_T("\n%s"), strMsg) ;
	int iNumConLineFW = GetIntersectPLine(polyLineTmp2, nHandleIntersectFW, nIndexOfVertexFW,  pt_3d_intersectFW, iDwIntersectMoveToVertex,-1) ;
	nArrayHandleIntersectLine.Add(nHandleIntersectFW) ;
	nArrayIndexOfVertex.Add(nIndexOfVertexFW) ;
	pt3dArrayIntersect.append(pt_3d_intersectFW) ;
	delete polyLineTmp2 ;
	polyLineTmp2 = NULL ;
	//mysys.AddAcDbPolyLine(pt2dArrayLine, 0, 3) ;

	if (iNumConLineFW==1&&iNumConLineDW==1)
	{
		bRet = TRUE ;
	}
	else
	{
		acutPrintf(_T("\n查找定反位线时不都为1，%.3f %.3f"), pt_3d_ref.x, pt_3d_ref.y) ;
	}

	return bRet ;
}

// 查找一条FW连线移动引起的所有移动，对于反位顶点nIndexOfVertex，若为起点，对应的该线段数就是第0段，若大于0，则对应段数为nIndexOfVertex-1
void CZnSxt::SearchToMoveEnt( UINT nHandleLineFW,  UINT nIndexOfVertex,  const AcGePoint2d &pt_2d_rotate, const double dRotateAngle)
{
	//查询在nHandleLineFW的第nIndexOfVertex段线上的实体
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_online_handle=%d and sxt_ent_online_vertex=%d"), nHandleLineFW, nIndexOfVertex==0?0:nIndexOfVertex-1) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long iEntType = 0 ;
				long iHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_type"), iEntType) ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), iHandle) ;
				AcGePoint2d pt_2d_ent(0,0), pt_2d_intersect(0,0) ;
				cSet.GetFieldValue(_T("sxt_ent_x1"), pt_2d_ent.x) ;
				cSet.GetFieldValue(_T("sxt_ent_y1"), pt_2d_ent.y) ;
				cSet.GetFieldValue(_T("sxt_ent_online_intersectx1"), pt_2d_intersect.x) ;
				cSet.GetFieldValue(_T("sxt_ent_online_intersecty1"), pt_2d_intersect.y) ;

				AcGeMatrix2d matrix; 
				matrix.setToRotation(dRotateAngle, pt_2d_rotate); 
				pt_2d_ent.transformBy(matrix) ; 	
				pt_2d_intersect.transformBy(matrix) ; 	

				cSet.Edit() ;
				cSet.SetFieldValue(_T("sxt_ent_x1"), pt_2d_ent.x) ;
				cSet.SetFieldValue(_T("sxt_ent_y1"), pt_2d_ent.y) ;
				cSet.SetFieldValue(_T("sxt_ent_online_intersectx1"), pt_2d_intersect.x) ;
				cSet.SetFieldValue(_T("sxt_ent_online_intersecty1"), pt_2d_intersect.y) ;
				cSet.Update() ;

				if (iEntType==0) //道岔
				{
					long iHandleLineFWTmp = 0 ;
					long iIndexOfVertexTmp = 0 ;
					cSet.GetFieldValue(_T("sxt_ent_fwline_handle"), iHandleLineFWTmp) ;
					cSet.GetFieldValue(_T("sxt_ent_fwline_vertex"), iIndexOfVertexTmp) ;

					//fw连接线
					strSql.Format(_T("select * from sxt_line where  sxt_line_handle=%d and sxt_line_vertex=%d"), iHandleLineFWTmp, iIndexOfVertexTmp) ;				
					m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x1"), pt_2d_ent.x	) ;
					m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y1"), pt_2d_ent.y	) ;
					pt_2d_ent.transformBy(matrix) ; 	
					
					m_RecordSet.SetRecordValue(strSql, _T("sxt_line_vertex_x1"), pt_2d_ent.x) ;
					m_RecordSet.SetRecordValue(strSql, _T("sxt_line_vertex_y1"), pt_2d_ent.y) ;

					this->SearchToMoveEnt(iHandleLineFWTmp, iIndexOfVertexTmp, pt_2d_rotate, dRotateAngle) ;
				}

				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in SearchToMoveEnt"));
	}
}

// 读入所有实体到数据库
int CZnSxt::ReadAllEntToDB()
{
	int iRet = 0 ;
	m_AdoDb.ClearTable(_T("sxt_ent")) ;
	m_AdoDb.ClearTable(_T("sxt_line")) ;

	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbObjectIdArray objArray ;
	AcDbEntity * pEnt = NULL ;
	CString strSql ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;	
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			UINT nHandle = m_mysys.GetHandle(pEnt) ;

			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;
				AcGePoint3d pt_3d_position = pRef->position() ;
				//将块统一为X比例和旋转角变化（X、Y都置-1，等同于旋转180度，X置-1并且旋转180度等同于Y置-1，Y置-1并且旋转180度等同于X置-1）
				AcGeScale3d scale_3d = pRef->scaleFactors()  ;		
				if (scale_3d.sy<0)
				{
					pRef->upgradeOpen() ;
					scale_3d.set(-scale_3d.sx, -scale_3d.sy, scale_3d.sz) ;
					pRef->setScaleFactors(scale_3d) ;
					pRef->setRotation(pRef->rotation()-PI) ;
					pRef->downgradeOpen() ;
				}	

				CString strName ;
				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;
				CStringArray strArrayBlkName ;
				CBlkUtility::DivideString(strBlkName, _T("_"), strArrayBlkName) ;
				CString strBlkType = _T("") ;
				if (strArrayBlkName.GetCount()>0)
				{
					strBlkType = strArrayBlkName.GetAt(0) ;
				}

				if (strBlkType==_T("DC"))
				{
					m_mysys.GetAttValue(pRef, _T("BIANHAO"), strName) ;
					//acutPrintf(_T("\n道岔编号%s"), strName) ;
					//和DC块定/反位连接的线信息
					CLongArray nArrayHandleIntersectLine, nArrayIndexOfVertex ;
					int iDwIntersectMoveToVertex = -1 ;
					AcGePoint3dArray pt3dArrayIntersect ;
					if(this->GetDcConLineDFW(pRef, nArrayHandleIntersectLine, nArrayIndexOfVertex, pt3dArrayIntersect, iDwIntersectMoveToVertex) )
					{
						strSql.Format(_T("insert into sxt_ent(sxt_ent_handle,  sxt_ent_block, sxt_ent_x, sxt_ent_y, sxt_ent_movetovertex, sxt_ent_name, sxt_ent_type,  sxt_ent_online_handle,  sxt_ent_online_vertex,  sxt_ent_online_intersectx,  sxt_ent_online_intersecty,  sxt_ent_fwline_handle,  sxt_ent_fwline_vertex,  sxt_ent_x1, sxt_ent_y1, sxt_ent_online_intersectx1,  sxt_ent_online_intersecty1)  \
							values(%d, '%s', %.10f, %.10f, %d, '%s', %d, %d, %d, %.10f, %.10f, %d, %d, %.10f, %.10f, %.10f, %.10f) "), nHandle, strBlkName,pt_3d_position.x, pt_3d_position.y, iDwIntersectMoveToVertex, strName, 0, nArrayHandleIntersectLine.GetAt(0), nArrayIndexOfVertex.GetAt(0), pt3dArrayIntersect.at(0).x, pt3dArrayIntersect.at(0).y, nArrayHandleIntersectLine.GetAt(1), nArrayIndexOfVertex.GetAt(1), pt_3d_position.x, pt_3d_position.y, pt3dArrayIntersect.at(0).x, pt3dArrayIntersect.at(0).y) ;
						m_AdoDb.Execute(strSql) ;
					}					
				}
				else if (strBlkType==_T("XHJ")||strBlkType==_T("JYJ"))
				{
					int iEntType = 100 ; //xhj
					if (strBlkType==_T("JYJ"))
					{
						iEntType = 200 ;
						if (strBlkName.Left(6)==_T("JYJ_JX"))
						{
							iEntType=250 ; //为实现极性切割增加的绝缘节
						}
					}

					m_mysys.GetAttValue(pRef, _T("XHJNAME"), strName) ;
					UINT nHandleIntersectLine, nIndexOfVertex ;
					AcGePoint3d pt_3d_intersect ;
					if(this->GetXhjJyjConLine(pRef, nHandleIntersectLine, nIndexOfVertex, pt_3d_intersect) )
					{
						strSql.Format(_T("insert into sxt_ent(sxt_ent_handle,  sxt_ent_block, sxt_ent_x, sxt_ent_y, sxt_ent_name, sxt_ent_type,  sxt_ent_online_handle,  sxt_ent_online_vertex,  sxt_ent_online_intersectx,  sxt_ent_online_intersecty,  sxt_ent_fwline_handle,  sxt_ent_fwline_vertex,  sxt_ent_x1, sxt_ent_y1, sxt_ent_online_intersectx1,  sxt_ent_online_intersecty1)  \
										 values(%d, '%s', %.10f, %.10f, '%s', %d, %d, %d, %.10f, %.10f, %d, %d, %.10f, %.10f, %.10f, %.10f) "), nHandle, strBlkName, pt_3d_position.x, pt_3d_position.y,strName, iEntType, nHandleIntersectLine, nIndexOfVertex, pt_3d_intersect.x, pt_3d_intersect.y, 0, 0, pt_3d_position.x, pt_3d_position.y, pt_3d_intersect.x, pt_3d_intersect.y) ;
						m_AdoDb.Execute(strSql) ;
					}	
				}
			}

			pEnt->close() ;
		}	
	}
	//再遍历一遍，将轨道线读入
	for (pBlkTblRecIter->start(); !pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				long nHandleLine = m_mysys.GetHandle(pEnt) ;
				strSql.Format(_T("select * from sxt_ent where sxt_ent_online_handle=%d or sxt_ent_fwline_handle=%d or sxt_ent_conline_left=%d or sxt_ent_conline_right=%d"), nHandleLine, nHandleLine, nHandleLine, nHandleLine ) ;
				int iRetSum = m_RecordSet.GetRecordCount(strSql) ;
				if (iRetSum>0)
				{
					AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;	
					int iNumVert = pPolyLine->numVerts() ;
					for (int i=0; i<iNumVert; i++)
					{
						AcGePoint3d pt_3d_vertex(0,0,0) ;
						pPolyLine->getPointAt(i, pt_3d_vertex) ;
						strSql.Format(_T("insert into sxt_line(sxt_line_handle, sxt_line_vertex,  sxt_line_vertex_x, sxt_line_vertex_y, sxt_line_vertex1,  sxt_line_vertex_x1, sxt_line_vertex_y1 )  \
										 values(%d, %d, %.10f, %.10f, %d, %.10f, %.10f) "), nHandleLine, i,  pt_3d_vertex.x, pt_3d_vertex.y, i,  pt_3d_vertex.x, pt_3d_vertex.y) ;
						m_AdoDb.Execute(strSql) ;
					}
				}
			}
			pEnt->close() ;
		}
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return iRet ;
}

// 取得信号机、绝缘节所在的线的信息
BOOL CZnSxt::GetXhjJyjConLine(const AcDbBlockReference *pRef, UINT &nHandleIntersectLine, UINT &nIndexOfVertex, AcGePoint3d &pt_3d_intersect)
{
	BOOL bRet = FALSE ;

	AcGePoint3d pt_3d_ref = pRef->position() ;
	AcGeScale3d scale_3d = pRef->scaleFactors()  ;
	double dAngle = pRef->rotation() ;

	//以块基点为源，确定2个点
	//先算其无旋转时的值，再旋转dAangle度
	double dPlus = 0.05 ; //计算点时，适当外扩一点，防止没开捕捉绘制的图
	AcGePoint2dArray pt2dArray ;
	for (int i=0; i<2; i++)
	{
		AcGePoint2d pt_2d_tmp(0,0) ;
		pt2dArray.append(pt_2d_tmp) ;
	}
	pt2dArray[0].x = pt2dArray[1].x = pt_3d_ref.x ;
	pt2dArray[0].y = pt_3d_ref.y+(1+dPlus) ;
	pt2dArray[1].y = pt_3d_ref.y-(1+dPlus) ;

	for(int i=0; i<2; i++)
	{
		AcGeMatrix2d matrix; 
		matrix.setToRotation(dAngle, pt_3d_ref.convert2d(AcGePlane::kXYPlane)); 
		pt2dArray[i].transformBy(matrix) ; 				
	}

	AcDbPolyline *polyLineTmp = new AcDbPolyline(2) ;
	polyLineTmp->addVertexAt(0, pt2dArray[0]) ;
	polyLineTmp->addVertexAt(1, pt2dArray[1]) ;
	int iDwIntersectMoveToVertex = -1 ;
	int iNumConLine = GetIntersectPLine(polyLineTmp, nHandleIntersectLine, nIndexOfVertex, pt_3d_intersect, iDwIntersectMoveToVertex, 100) ;
	delete polyLineTmp ;
	polyLineTmp = NULL ;

	if (iNumConLine==1)
	{
		bRet = TRUE ;
	}

	return bRet ;
}

// 取得信号机所在的线的信息(可能为两段)
int CZnSxt::GetXhjJyjConLine(const AcDbBlockReference *pRef, CLongArray &nArrayHandleIntersect, CLongArray &nArrayIndexOfVertex, AcGePoint3dArray &pt3dArrayIntersect)
{
	int iRet = -1 ;

	AcGeTol tol ;
	tol.setEqualPoint(0.05) ;

	AcGePoint3d pt_3d_ref = pRef->position() ;

	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		BOOL bValidData = FALSE ; //本次数据是否有效

		if (iRet==0)
		{
			break;
		}
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;
			
				//只查询显示的多段线
				AcDb::Visibility bIsVisible  = pPolyLine->visibility() ;
				double dLenOfPline = m_mysys.GetLineLen(pPolyLine) ;
				if (bIsVisible==AcDb::kInvisible||m_mysys.IsOnOffLayer(pEnt)||dLenOfPline<1)
				{
					pEnt->close() ;
					continue;
				}
				long nHandleCur = m_mysys.GetHandle(pEnt) ;
				AcGePoint3d pt_3d_intersect ; //交点
				UINT nIndexOfVertex = 0 ;
				int iPtOnPLine = m_mysys.IsPointOnPolyLine(pPolyLine, pt_3d_ref, nIndexOfVertex,0.05) ;
				if (iPtOnPLine==1000)
				{
					if (iRet==-1)
					{
						iRet = 2 ;
						pPolyLine->getClosestPointTo(pt_3d_ref, pt_3d_intersect) ;
						bValidData = TRUE ;
					}
					else
					{
						iRet = 0 ;
					}
				}
				else if (iPtOnPLine>-1&&iPtOnPLine<1000)
				{
					if (iPtOnPLine==0) //起点
					{
						pPolyLine->getStartPoint(pt_3d_intersect) ;
						if (iRet==-1)
						{
							iRet = 5 ;
							bValidData = TRUE ;
						}
						else if (iRet==1)
						{
							iRet = 5*10+1 ;
							bValidData = TRUE ;
						}
						else
						{
							iRet = 0 ;
						}
					}
					else //终点
					{
						pPolyLine->getEndPoint(pt_3d_intersect) ;
						if (iRet==-1)
						{
							iRet = 1 ;
							bValidData = TRUE ;
						}
						else if (iRet==5)
						{
							iRet = 1*10+5 ;
							bValidData = TRUE ;
						}
						else
						{
							iRet = 0 ;
						}
					}
				}
				if (bValidData)
				{
					nArrayHandleIntersect.Add(nHandleCur) ;
					nArrayIndexOfVertex.Add(nIndexOfVertex) ;
					pt3dArrayIntersect.append(pt_3d_intersect) ;
				}
			}

			pEnt->close() ;
		}		
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;	
	

	return iRet ;
}

int CZnSxt::DrawGuiDao(void)
{
	AcDbDatabase *pDb = acdbHostApplicationServices()->workingDatabase() ;	
	CUserSystem mysys ;
	mysys.m_pDb = pDb ;
	mysys.Set_Layer(_T(""), _T("sxt_guidao")) ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select distinct sxt_line_handle from sxt_line")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long  iHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_line_handle"), iHandle) ;

				AcGePoint2dArray pt2dArray ;
				strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d order by sxt_line_vertex1"), iHandle) ;
				CADORecordset cSetVertex(&m_AdoDb) ;
				if (cSetVertex.Open(strSql))
				{
					if (!cSetVertex.IsBOF())
					{
						cSetVertex.MoveFirst() ;
					}
					while (!cSetVertex.IsEOF())
					{
						double dVertexX = 0.000, dVertexY = 0.000 ;
						cSetVertex.GetFieldValue(_T("sxt_line_vertex_x1"), dVertexX) ;
						cSetVertex.GetFieldValue(_T("sxt_line_vertex_y1"), dVertexY) ;
						AcGePoint2d pt_2d_tmp(dVertexX, dVertexY) ;
						pt2dArray.append(pt_2d_tmp) ;

						cSetVertex.MoveNext() ;
					}
					cSetVertex.Close() ;
				}
				
				mysys.AddAcDbPolyLine(pt2dArray, 0) ;

				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in DrawGuiDao"));
	}

	mysys.Set_Layer(_T(""), _T("0")) ;

	return 0;
}

// 绘制双线图
int CZnSxt::DrawSxt(double dScaleX, double dScaleY, const AcGePoint3d & pt_3d_orign, double dLenDcPlus, double dDistBetweenSX)
{
	AcDbObjectId objIdOldLayer = m_pDb->clayer() ;
	m_mysys.Set_Layer(_T(""), _T("sxt")) ;
	AcGeTol tol  ; 
	tol.setEqualVector(0.00001) ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;

		//begin 根据指定基点、缩放比值，计算实体（信号机、道岔、轨道线）坐标值
		CString strTables[2] ={ _T("sxt_ent"), _T("sxt_line")} ;
		for (int iTbIndex=0; iTbIndex<2; iTbIndex++)
		{
			strSql.Format(_T("select * from %s"), strTables[iTbIndex]) ;
			CString strField1[2], strField2[2], strField3[2], strField4[2] ;

			for (int i=0; i<2; i++)
			{
				if (iTbIndex==0)
				{
					strField1[i].Format(_T("%s_x%d"), strTables[iTbIndex], i+1) ;
					strField2[i].Format(_T("%s_y%d"), strTables[iTbIndex], i+1) ;
					strField3[i].Format(_T("%s_online_intersectx%d"), strTables[iTbIndex], i+1) ;
					strField4[i].Format(_T("%s_online_intersecty%d"), strTables[iTbIndex], i+1) ;
				}
				else if (iTbIndex==1)
				{
					strField1[i].Format(_T("%s_vertex_x%d"), strTables[iTbIndex], i+1) ;
					strField2[i].Format(_T("%s_vertex_y%d"), strTables[iTbIndex], i+1) ;
				}
			}

			if(cSet.Open(strSql))
			{
				while(!cSet.IsEOF())
				{
					double dEntX = 0.000, dEntY = 0.000, dEntX1 = 0.000, dEntY1 = 0.000;		

					cSet.GetFieldValue(strField1[0], dEntX) ;
					cSet.GetFieldValue(strField2[0], dEntY) ;
					dEntX = pt_3d_orign.x +dEntX*dScaleX ;
					dEntY = pt_3d_orign.y +dEntY*dScaleY ;

					if (iTbIndex==0)
					{
						cSet.GetFieldValue(strField3[0], dEntX1) ;
						cSet.GetFieldValue(strField4[0], dEntY1) ;
						dEntX1 = pt_3d_orign.x +dEntX1*dScaleX ;
						dEntY1 = pt_3d_orign.y +dEntY1*dScaleY ;
					}
					cSet.Edit() ;
					cSet.SetFieldValue(strField1[1], dEntX) ;
					cSet.SetFieldValue(strField2[1], dEntY) ;
					if (iTbIndex==0)
					{
						cSet.SetFieldValue(strField3[1], dEntX1) ;
						cSet.SetFieldValue(strField4[1], dEntY1) ;
					}
					cSet.Update() ;

					cSet.MoveNext() ;
				}

				cSet.Close() ;
			}
		}
		//end 根据指定基点、缩放比值，计算实体（信号机、道岔、轨道线）坐标值

		//begin 绘制线
		strSql.Format(_T("select distinct sxt_line_handle from sxt_line")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long  iHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_line_handle"), iHandle) ;
				//begin 读取该线信息for绘制轨道名称
				CString strGdName ;
				int iGdWriteLeftOrRight = -1 ; //轨道名称写在线左侧0还是右侧1
				AcDbEntity* pEntLine = NULL ;
				if (m_mysys.OpenAcDbEntity(iHandle, pEntLine, AcDb::kForRead))
				{
					if(m_mysys.GetXdata(pEntLine, _T("GUIDAONAME"), strGdName))
					{
						if (!strGdName.IsEmpty())
						{
							CString strLeftForS=_T("N"), strRightForS = _T("N") ;
							m_mysys.GetXdata(pEntLine, _T("LEFT_FORS"), strLeftForS) ;
							m_mysys.GetXdata(pEntLine, _T("RIGHT_FORS"), strRightForS) ;
							if (strLeftForS==_T("S"))
							{
								iGdWriteLeftOrRight = 0 ;
							}
							else if (strRightForS==_T("S"))
							{
								iGdWriteLeftOrRight = 1 ;
							}
							else if (strLeftForS==_T("F"))
							{
								iGdWriteLeftOrRight = 0 ;
							}
							else if (strRightForS==_T("F"))
							{
								iGdWriteLeftOrRight = 1 ;
							}
						}
					}
					pEntLine->close() ;
				}
				//end 读取该线信息for绘制轨道名称

				//确定该多段线的起点和终点是否都是DC的反位连接端

				AcGePoint2dArray pt2dArrayUp, pt2dArrayDown ;
				AcArray <double> dDrawOptionUps,dDrawOptionDowns ;
				//VertexInfo=A:0,B:0,C:0;A:1,B:0,C:0;A:2,B:1,C:-1;A:3,B:2,C:1;A:4,B:3,C:2
				//ABC后面数字表示：A，双线中线的顶点序号；B，原平面图线加上极性切割绝缘节后形成的顶点序号；C，原平面图线不考虑极性绝缘节的顶点序号
				CString strXdataUp, strXdataDown ; 
				CString strXdataUpForZZJ, strXdataDownForZZJ ; //记录形式（针对的是道岔FW线）：端头连接的道岔handle-道岔FW朝向（1，朝上；0，朝下）-FW端线的顶点vertex（起点或终点）-道岔所在DW线-道岔所在DW线vertex
				strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d order by sxt_line_vertex1"), iHandle) ;
				CADORecordset cSetVertex(&m_AdoDb) ;
				if (cSetVertex.Open(strSql))
				{
					int iNumVertex = cSetVertex.GetRecordCount() ;
					if (!cSetVertex.IsBOF())
					{
						cSetVertex.MoveFirst() ;
					}
					int iIndexVertex = 0 ;
					int iDrawOptionPre = 2222 ; //前一顶点的DrawOption
					while (!cSetVertex.IsEOF())
					{
						//acutPrintf(_T("\nhandle:%d,vetex:%d"), iHandle, iIndexVertex) ;
						AcGePoint2d pt_2d_up(0,0), pt_2d_down(0,0) ; //双线的上下2个点
						AcGePoint2d pt_2d_cur(0,0), pt_2d_pre(0,0), pt_2d_back(0,0) ; 

						int iIndexVertexOld = iIndexVertex ; //原顶点序号（未加入切割点）
						cSetVertex.GetFieldValue(_T("sxt_line_vertex"), iIndexVertexOld) ;
						
						cSetVertex.GetFieldValue(_T("sxt_line_vertex_x2"), pt_2d_cur.x) ;
						cSetVertex.GetFieldValue(_T("sxt_line_vertex_y2"), pt_2d_cur.y) ;

						//begin 线宽
						long nHandleVertex = 0 ; //该点对应的绝缘节或者道岔
						int iDrawOptionCur = -1 ; //当前顶点的DrawOption
						cSetVertex.GetFieldValue(_T("sxt_line_vertex_handle"), nHandleVertex) ;

						double dWidthUp = 0, dWidthDown = 0 ;

						int iPolar = 0 ;
						cSetVertex.GetFieldValue(_T("sxt_line_polar"), iPolar) ;
						if (iIndexVertex==(iNumVertex-1)) //最后一段极性取前一个顶点的值
						{
							cSetVertex.MovePrevious() ;
							cSetVertex.GetFieldValue(_T("sxt_line_polar"), iPolar) ;
							cSetVertex.MoveNext() ;
						}
						if (iPolar==-1) //上粗下细
						{
							dWidthUp = 0.5 ;
							dWidthDown = 0 ;							
						}
						else if (iPolar==1)
						{
							dWidthUp = 0 ;
							dWidthDown = 0.5 ;				
						}
						//end 线宽

						if (iIndexVertex!=0)//不是第一个点
						{
							cSetVertex.MovePrevious() ;
							cSetVertex.GetFieldValue(_T("sxt_line_vertex_x2"), pt_2d_pre.x) ;
							cSetVertex.GetFieldValue(_T("sxt_line_vertex_y2"), pt_2d_pre.y) ;
							cSetVertex.MoveNext() ;
						}
						if (iIndexVertex!=(iNumVertex-1)) //不是最后一个点
						{
							cSetVertex.MoveNext() ;
							cSetVertex.GetFieldValue(_T("sxt_line_vertex_x2"), pt_2d_back.x) ;
							cSetVertex.GetFieldValue(_T("sxt_line_vertex_y2"), pt_2d_back.y) ;
							cSetVertex.MovePrevious() ;
						}

						if (iIndexVertex==0)
						{
							AcGeLineSeg2d lineSeg2dSrc(pt_2d_cur, pt_2d_back) ;
							AcGeLineSeg2d lineSeg2dParallelUp, lineSeg2dParallelDown ;
							m_mysys.GetParallelLine(lineSeg2dSrc, lineSeg2dParallelUp, 2.5 ) ;
							m_mysys.GetParallelLine(lineSeg2dSrc, lineSeg2dParallelDown, 2.5, FALSE) ;
							pt_2d_up = lineSeg2dParallelUp.startPoint() ;
							pt_2d_down = lineSeg2dParallelDown.startPoint() ;

							//begin 绘制轨道名称
							if (iGdWriteLeftOrRight==0)
							{
								AcGePoint2d pt_2d_gdname = lineSeg2dSrc.midPoint() ;
								AcGePoint3d pt_3dg_gdname(pt_2d_gdname.x, pt_2d_gdname.y, 0) ;
								AcGeVector2d vecLine = lineSeg2dSrc.direction() ;
								AcDbObjectId objIdGdName ;
								m_mysys.AddText(objIdGdName, strGdName, pt_3dg_gdname, pt_3dg_gdname, AcDb::kTextCenter, AcDb::kTextVertMid, 2.5, 0.8, vecLine.angle(), 4)  ;	
								m_mysys.SetXdata(objIdGdName, _T("TYPE"), (long)101) ; //双线图中轨道名称文本
								m_mysys.SetXdata(objIdGdName, _T("BELONG_ENT"), iHandle) ;
							}
							//end 绘制轨道名称

							//和该FW端相交的DW轨道线的信息
							AcGePoint2d pt_2d_start_dw, pt_2d_end_dw ;	
							strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d and sxt_ent_fwline_vertex=%d"), iHandle, iIndexVertex) ;
							long iHandleDW = 0 ;
							int iLineVertexDW = 0 ;							
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_handle"), iHandleDW) ;
							long nHandleDc = 0 ; //以该线为反位线的道岔记录下来，为双线图绘制尖轨芯轨用
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleDc) ;
							CString strDcName ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strDcName) ;
							int iUpOrDownDc = 0 ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_upordown"), iUpOrDownDc) ;
							if (iHandleDW!=0) //还有不是和DC反位相连的线，比如轨道起始
							{
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_vertex"), iLineVertexDW) ;
								strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex=%d"), iHandleDW, iLineVertexDW) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x2"), pt_2d_start_dw.x) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y2"),  pt_2d_start_dw.y) ;
								strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex=%d"), iHandleDW, iLineVertexDW+1) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x2"), pt_2d_end_dw.x) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y2"),  pt_2d_end_dw.y) ;
								//2条平行线，一条距定位2.5,，另一条是距定位线1.5
								AcGeLineSeg2d lineSeg2dDwSrc(pt_2d_start_dw, pt_2d_end_dw), lineSeg2dDw25, lineSeg2dDw15 ;

								//begin 尖轨、芯轨牵引数量
								//int iJgNum = 3, iXgNum = 2 ; 
								double dAngleDc = 0 ; //道岔块旋转角度
								AcDbEntity* pEntDc = NULL ;
								if (m_mysys.OpenAcDbEntity(nHandleDc, pEntDc, AcDb::kForRead))
								{
									AcDbBlockReference* pRefDc = AcDbBlockReference::cast(pEntDc) ;
									dAngleDc = pRefDc->rotation() ;
									pEntDc->close() ;
								}
								//end 尖轨、芯轨牵引数量

								AcGePoint2d pt_2d_ext ;
								AcGeVector2d vecDw = pt_2d_end_dw-pt_2d_start_dw ;
								//用于判断道岔垂直方向，不能简单根据*** int iDcDirectV = ((pt_2d_back.y-pt_2d_cur.y)>0.5?0:1) ; //道岔垂直方向（FW向上0、向下1）***
								//根据以下算法：道岔旋转角，90~270度之间，向下	
								int iDcDirectV =0 ; //道岔垂直方向（FW向上0、向下1）
								if (dAngleDc>PI/2&&dAngleDc<(PI*3/2))
								{
									iDcDirectV = 1 ;
								}
								if (iDcDirectV==0) //反位端朝上
								{
									//反位端相交的所在多段线及段序号
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw25, 2.5, TRUE) ;
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw15, 1.5, FALSE) ;

									lineSeg2dParallelUp.intersectWith(lineSeg2dDw25, pt_2d_up) ;
									lineSeg2dParallelDown.intersectWith(lineSeg2dDw15, pt_2d_down) ;

									//Dc反位线和定位线平行的一段
									pt_2d_ext =pt_2d_down ;
									pt_2d_ext.x -=dLenDcPlus ;
									pt_2d_ext.rotateBy(vecDw.angle(), pt_2d_down) ;								
									pt2dArrayDown.append(pt_2d_ext) ;		
									dDrawOptionDowns.append(dWidthDown) ;
									strXdataDown.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayDown.length()-1, iIndexVertex, iIndexVertexOld) ;
									strXdataDownForZZJ.AppendFormat(_T("%d-1-%d-%d-%d;"), nHandleDc, iIndexVertex, iHandleDW, iLineVertexDW) ;	
								}
								else //反位端朝下，（反位端在水平线上，视同反位朝下）
								{
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw25, 2.5, FALSE) ;
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw15, 1.5, TRUE) ;

									lineSeg2dParallelUp.intersectWith(lineSeg2dDw15, pt_2d_up) ;
									lineSeg2dParallelDown.intersectWith(lineSeg2dDw25, pt_2d_down) ;

									//Dc反位线和定位线平行的一段
									pt_2d_ext = pt_2d_up ;
									pt_2d_ext.x -=dLenDcPlus ;
									pt_2d_ext.rotateBy(vecDw.angle(), pt_2d_up) ;
									pt2dArrayUp.append(pt_2d_ext) ;
									dDrawOptionUps.append(dWidthUp) ;
									strXdataUp.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayUp.length()-1, iIndexVertex, iIndexVertexOld) ;
									strXdataUpForZZJ.AppendFormat(_T("%d-0-%d-%d-%d;"), nHandleDc, iIndexVertex, iHandleDW, iLineVertexDW) ;
								}

								//begin draw zzj
								int iDcDirectH =0 ; //道岔FW水平朝向，0朝右，1，朝左
								AcGeLineSeg2d line2d(pt_2d_ext,iDcDirectV==0?pt_2d_down:pt_2d_up) ;									
								AcGePoint2d pt_2d_extcen =line2d.midPoint() ;
								AcGePoint2d pt_2d_extcen_mid = pt_2d_extcen ;
								pt_2d_extcen_mid.y+=(iDcDirectV==0?1.5:-1.5) ;
								pt_2d_extcen_mid.rotateBy(vecDw.angle(), pt_2d_extcen) ;

								//映射到双线的一边
								AcGePoint2d pt_2d_extcen_jg = pt_2d_extcen_mid ;
								pt_2d_extcen_jg.y +=(iUpOrDownDc==0?-2.5:2.5); 
								pt_2d_extcen_jg.rotateBy(vecDw.angle(), pt_2d_extcen) ;
								
								//道岔名称
								AcGePoint2d pt_2d_dcname = pt_2d_extcen_jg ;
								pt_2d_dcname.y+=(iUpOrDownDc==0?-2:2); 
								pt_2d_dcname.rotateBy(vecDw.angle(), pt_2d_extcen_jg) ;
								AcGePoint3d pt_3d_dcname(pt_2d_dcname.x, pt_2d_dcname.y,0) ;
								m_mysys.AddText(strDcName, pt_3d_dcname, pt_3d_dcname, AcDb::kTextCenter, AcDb::kTextVertMid, 2.5, 0.8, vecDw.angle(), 3) ;
								//end draw zzj
							}

							pt2dArrayUp.append(pt_2d_up) ;
							dDrawOptionUps.append(dWidthUp) ;
							strXdataUp.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayUp.length()-1, iIndexVertex, iIndexVertexOld) ;
							pt2dArrayDown.append(pt_2d_down) ;
							dDrawOptionDowns.append(dWidthDown) ;
							strXdataDown.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayDown.length()-1, iIndexVertex, iIndexVertexOld) ;
						}
						else if (iIndexVertex==(iNumVertex-1))
						{
							AcGeLineSeg2d lineSeg2dSrc(pt_2d_pre, pt_2d_cur) ;
							AcGeLineSeg2d lineSeg2dParallelUp, lineSeg2dParallelDown ;
							m_mysys.GetParallelLine(lineSeg2dSrc, lineSeg2dParallelUp, 2.5 ) ;
							m_mysys.GetParallelLine(lineSeg2dSrc, lineSeg2dParallelDown, 2.5, FALSE) ;
							pt_2d_up = lineSeg2dParallelUp.endPoint() ;
							pt_2d_down = lineSeg2dParallelDown.endPoint() ;

							//begin 绘制轨道名称
							if (iGdWriteLeftOrRight==1)
							{
								AcGePoint2d pt_2d_gdname = lineSeg2dSrc.midPoint() ;
								AcGePoint3d pt_3dg_gdname(pt_2d_gdname.x, pt_2d_gdname.y, 0) ;
								AcGeVector2d vecLine = lineSeg2dSrc.direction() ;
								AcDbObjectId objIdGdName ;
								m_mysys.AddText(objIdGdName, strGdName, pt_3dg_gdname, pt_3dg_gdname, AcDb::kTextCenter, AcDb::kTextVertMid, 2.5, 0.8, vecLine.angle(), 4)  ;	
								m_mysys.SetXdata(objIdGdName, _T("TYPE"), (long)101) ; //双线图中轨道名称文本
								m_mysys.SetXdata(objIdGdName, _T("BELONG_ENT"), iHandle) ;
							}
							//end 绘制轨道名称

							int iIndexVertexOld = -1 ;
							cSetVertex.GetFieldValue(_T("sxt_line_vertex"), iIndexVertexOld) ;

							//和该FW端相交的DW轨道线的信息
							AcGePoint2d pt_2d_start_dw, pt_2d_end_dw ;	
							strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d and sxt_ent_fwline_vertex=%d"), iHandle, iIndexVertexOld) ;
							long iHandleDW = 0 ;
							int iLineVertexDW = 0 ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_handle"), iHandleDW) ;
							long nHandleDc = 0 ; //以该线为反位线的道岔记录下来，为双线图绘制尖轨芯轨用
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleDc) ;
							CString strDcName ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strDcName) ;
							int iUpOrDownDc = 0 ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_upordown"), iUpOrDownDc) ;

							if (iHandleDW!=0) //还有不是和DC反位相连的线，比如轨道终点
							{
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_vertex"), iLineVertexDW) ;
								strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex=%d"), iHandleDW, iLineVertexDW) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x2"), pt_2d_start_dw.x) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y2"),  pt_2d_start_dw.y) ;
								strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex=%d"), iHandleDW, iLineVertexDW+1) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x2"), pt_2d_end_dw.x) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y2"),  pt_2d_end_dw.y) ;
								//2条平行线，一条距定位2.5,，另一条是距定位线1.5
								AcGeLineSeg2d lineSeg2dDwSrc(pt_2d_start_dw, pt_2d_end_dw), lineSeg2dDw25, lineSeg2dDw15 ;

								//begin 尖轨、芯轨牵引数量
								double dAngleDc = 0 ; //道岔块旋转角度
								AcDbEntity* pEntDc = NULL ;
								if (m_mysys.OpenAcDbEntity(nHandleDc, pEntDc, AcDb::kForRead))
								{
									AcDbBlockReference* pRefDc = AcDbBlockReference::cast(pEntDc) ;
									dAngleDc = pRefDc->rotation() ;
									pEntDc->close() ;
								}
								//end 尖轨、芯轨牵引数量

								AcGePoint2d pt_2d_ext ;
								AcGeVector2d vecDw = pt_2d_end_dw-pt_2d_start_dw ;
								//用于判断道岔垂直方向，不能简单根据*** int iDcDirectV = ((pt_2d_pre.y-pt_2d_cur.y)>0.5?0:1) ; //道岔垂直方向（FW向上0、向下1）***
								//根据以下算法：道岔旋转角，90~270度之间，向下								
								int iDcDirectV =0 ; //道岔垂直方向（FW向上0、向下1）
								if (dAngleDc>PI/2&&dAngleDc<(PI*3/2))
								{
									iDcDirectV = 1 ;
								}
								if (iDcDirectV==0) //反位端朝上,允许一定误差，这个0.5可再议
								{
									//反位端相交的所在多段线及段序号
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw25, 2.5, TRUE) ;
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw15, 1.5, FALSE) ;

									lineSeg2dParallelUp.intersectWith(lineSeg2dDw25, pt_2d_up) ;
									lineSeg2dParallelDown.intersectWith(lineSeg2dDw15, pt_2d_down) ;

									pt2dArrayUp.append(pt_2d_up) ;
									dDrawOptionUps.append(dWidthUp) ;
									strXdataUp.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayUp.length()-1, iIndexVertex, iIndexVertexOld) ;
									pt2dArrayDown.append(pt_2d_down) ;
									dDrawOptionDowns.append(dWidthDown) ;
									strXdataDown.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayDown.length()-1, iIndexVertex, iIndexVertexOld) ;

									//Dc反位线和定位线平行的一段
									pt_2d_ext =pt_2d_down ;
									pt_2d_ext.x +=dLenDcPlus ;
									//AcGeVector2d vecDw = pt_2d_end_dw-pt_2d_start_dw ;
									pt_2d_ext.rotateBy(vecDw.angle(), pt_2d_down) ;								
									pt2dArrayDown.append(pt_2d_ext) ;	
									dDrawOptionDowns.append(dWidthDown) ;
									strXdataDown.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayDown.length()-1, iIndexVertex, iIndexVertexOld) ;
									strXdataDownForZZJ.AppendFormat(_T("%d-1-%d-%d-%d;"), nHandleDc, iIndexVertex, iHandleDW, iLineVertexDW) ;
								}
								else //反位端朝下，（反位端在水平线上，视同反位朝下）
								{
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw25, 2.5, FALSE) ;
									m_mysys.GetParallelLine(lineSeg2dDwSrc, lineSeg2dDw15, 1.5, TRUE) ;

									lineSeg2dParallelUp.intersectWith(lineSeg2dDw15, pt_2d_up) ;
									lineSeg2dParallelDown.intersectWith(lineSeg2dDw25, pt_2d_down) ;

									pt2dArrayUp.append(pt_2d_up) ;
									dDrawOptionUps.append(dWidthUp) ;
									strXdataUp.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayUp.length()-1, iIndexVertex, iIndexVertexOld) ;
									pt2dArrayDown.append(pt_2d_down) ;
									dDrawOptionDowns.append(dWidthDown) ;
									strXdataDown.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayDown.length()-1, iIndexVertex, iIndexVertexOld) ;

									//Dc反位线和定位线平行的一段
									pt_2d_ext = pt_2d_up ;
									pt_2d_ext.x +=dLenDcPlus ;
									//AcGeVector2d vecDw = pt_2d_end_dw-pt_2d_start_dw ;
									pt_2d_ext.rotateBy(vecDw.angle(), pt_2d_up) ;
									pt2dArrayUp.append(pt_2d_ext) ;
									dDrawOptionUps.append(dWidthUp) ;
									strXdataUp.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayUp.length()-1, iIndexVertex, iIndexVertexOld) ;
									strXdataUpForZZJ.AppendFormat(_T("%d-0-%d-%d-%d;"), nHandleDc, iIndexVertex, iHandleDW, iLineVertexDW) ;
								}

								//begin draw zzj
								int iDcDirectH = 1 ; //道岔FW水平朝向，0朝右，1，朝左
								AcGeLineSeg2d line2d(pt_2d_ext,iDcDirectV==0?pt_2d_down:pt_2d_up) ;									
								AcGePoint2d pt_2d_extcen =line2d.midPoint() ;

								AcGePoint2d pt_2d_extcen_mid = pt_2d_extcen ;
								pt_2d_extcen_mid.y+=(iDcDirectV==0?1.5:-1.5) ;
								pt_2d_extcen_mid.rotateBy(vecDw.angle(), pt_2d_extcen) ;

								//映射到双线的一边
								AcGePoint2d pt_2d_extcen_jg = pt_2d_extcen_mid ;
								pt_2d_extcen_jg.y +=(iUpOrDownDc==0?-2.5:2.5); 
								pt_2d_extcen_jg.rotateBy(vecDw.angle(), pt_2d_extcen) ;

								//道岔名称
								AcGePoint2d pt_2d_dcname = pt_2d_extcen_jg ;
								pt_2d_dcname.y+=(iUpOrDownDc==0?-2:2); 
								pt_2d_dcname.rotateBy(vecDw.angle(), pt_2d_extcen_jg) ;
								AcGePoint3d pt_3d_dcname(pt_2d_dcname.x, pt_2d_dcname.y,0) ;
								m_mysys.AddText(strDcName, pt_3d_dcname, pt_3d_dcname, AcDb::kTextCenter, AcDb::kTextVertMid, 2.5, 0.8, vecDw.angle(), 3) ;
								//end draw zzj
							}
							else
							{
								pt2dArrayUp.append(pt_2d_up) ;
								dDrawOptionUps.append(dWidthUp) ;
								strXdataUp.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayUp.length()-1, iIndexVertex, iIndexVertexOld) ;
								pt2dArrayDown.append(pt_2d_down) ;
								dDrawOptionDowns.append(dWidthDown) ;
								strXdataDown.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayDown.length()-1, iIndexVertex, iIndexVertexOld) ;
							}
						}
						else
						{
							AcGeLineSeg2d lineSeg2dSrc1(pt_2d_pre, pt_2d_cur), lineSeg2dSrc2(pt_2d_cur, pt_2d_back) ;	
							//acutPrintf(_T("\nlinesrc1(%.3f,%.3f)(%.3f,%.3f) linesrc2(%.3f,%.3f)(%.3f,%.3f)"), lineSeg2dSrc1.startPoint().x,lineSeg2dSrc1.startPoint().y,lineSeg2dSrc1.endPoint().x,lineSeg2dSrc1.endPoint().y,lineSeg2dSrc2.startPoint().x,lineSeg2dSrc2.startPoint().y, lineSeg2dSrc2.endPoint().x,lineSeg2dSrc2.endPoint().y );

							AcGeLineSeg2d lineSeg2dParallelUp1, lineSeg2dParallelDown1, lineSeg2dParallelUp2, lineSeg2dParallelDown2 ;
							m_mysys.GetParallelLine(lineSeg2dSrc1, lineSeg2dParallelUp1, 2.5 ) ;
							m_mysys.GetParallelLine(lineSeg2dSrc1, lineSeg2dParallelDown1, 2.5, FALSE) ;
							m_mysys.GetParallelLine(lineSeg2dSrc2, lineSeg2dParallelUp2, 2.5 ) ;
							m_mysys.GetParallelLine(lineSeg2dSrc2, lineSeg2dParallelDown2, 2.5, FALSE) ;				

							//lineSeg2dParallelUp1.intersectWith(lineSeg2dParallelUp2, pt_2d_up) ;
							//lineSeg2dParallelDown1.intersectWith(lineSeg2dParallelDown2, pt_2d_down) ;
							m_mysys.IntersectWith(&lineSeg2dParallelUp1, &lineSeg2dParallelUp2, pt_2d_up, AcDb::kExtendBoth) ;
							m_mysys.IntersectWith(&lineSeg2dParallelDown1, &lineSeg2dParallelDown2, pt_2d_down, AcDb::kExtendBoth) ;

							if (lineSeg2dSrc1.isParallelTo(lineSeg2dSrc2, tol))
							{
								pt_2d_up = lineSeg2dParallelUp1.endPoint() ;
								pt_2d_down = lineSeg2dParallelDown1.endPoint() ;
							}
							//acutPrintf(_T("\nintersect-up(%.3f,%.3f)intersect-down(%.3f,%.3f)"), pt_2d_up.x,pt_2d_up.y,pt_2d_down.x,pt_2d_down.y );

							pt2dArrayUp.append(pt_2d_up) ;
							dDrawOptionUps.append(dWidthUp) ;
							strXdataUp.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayUp.length()-1, iIndexVertex, iIndexVertexOld) ;
							pt2dArrayDown.append(pt_2d_down) ;
							dDrawOptionDowns.append(dWidthDown) ;
							strXdataDown.AppendFormat(_T("A:%d,B:%d,C:%d;"), pt2dArrayDown.length()-1, iIndexVertex, iIndexVertexOld) ;
						}

						cSetVertex.MoveNext() ;
						iIndexVertex++ ;
						iDrawOptionPre = iDrawOptionCur ;
					}
					cSetVertex.Close() ;
				}

				AcDbObjectId objIdUp = m_mysys.AddAcDbPolyLine(pt2dArrayUp, dDrawOptionUps, 7) ;				
				AcDbObjectId objIdDown = m_mysys.AddAcDbPolyLine(pt2dArrayDown, dDrawOptionDowns, 7) ;

				//set xdata
				strXdataUp.TrimRight(_T(";")) ;
				strXdataDown.TrimRight(_T(";")) ;
				strXdataUpForZZJ.TrimRight(_T(";")) ;
				strXdataDownForZZJ.TrimRight(_T(";")) ;
				AcDbEntity * pEnt = NULL ;
				if (m_mysys.OpenAcDbEntity(objIdUp, pEnt))
				{
					m_mysys.SetXdata(pEnt, _T("CloneFrom"), iHandle) ;
					m_mysys.SetXdata(pEnt, _T("UPDOWN"), 1) ;
					m_mysys.SetXdata(pEnt, _T("VertexInfo"), strXdataUp) ;
					if (!strXdataUpForZZJ.IsEmpty())
					{
						m_mysys.SetXdata(pEnt, _T("ForZZJ"), strXdataUpForZZJ) ;
					}
					pEnt->close() ;
				}
				if (m_mysys.OpenAcDbEntity(objIdDown, pEnt))
				{
					m_mysys.SetXdata(pEnt, _T("CloneFrom"), iHandle) ;
					m_mysys.SetXdata(pEnt, _T("UPDOWN"), 0) ;
					m_mysys.SetXdata(pEnt, _T("VertexInfo"), strXdataDown) ;
					if (!strXdataDownForZZJ.IsEmpty())
					{
						m_mysys.SetXdata(pEnt, _T("ForZZJ"), strXdataDownForZZJ) ;
					}
					pEnt->close() ;
				}

				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
		//end 绘制线

		//begin 信号机
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type=100")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				
				double dEntX = 0.000, dEntY = 0.000 ;				
				cSet.GetFieldValue(_T("sxt_ent_x2"), dEntX) ;
				cSet.GetFieldValue(_T("sxt_ent_y2"), dEntY) ;
				CString strBlkName ; 
				cSet.GetFieldValue(_T("sxt_ent_block"), strBlkName) ;
				AcDbEntity * pEnt = NULL ;
				if (m_mysys.OpenAcDbEntity(nHandle, pEnt))
				{
					AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;
					double dAngle = pRef->rotation() ;
					AcGePoint3d pt_3d_old = pRef->position() ;
					AcDbBlockReference *pRefNew = AcDbBlockReference::cast(pRef->clone()) ;
					m_mysys.SetXdata(pRefNew, _T("CloneFrom"), nHandle) ;
					AcGePoint3d pt_3d_new(dEntX, dEntY, 0) ;
					AcGeMatrix3d matrix ;
					AcGeVector3d vec = pt_3d_new - pt_3d_old ;
					matrix.setToTranslation(vec) ;
					pRefNew->transformBy(matrix) ; 
					//begin 在双线图中，信号机基点不能在中线上，要至少上/下移2.5
					AcGePoint3d pt_3d_new2(pt_3d_new) ;
					int iMarkLoc = strBlkName.Find(_T("_Z_")) ;//正向信号机（Z 立于左侧）, 逆向信号机(Y 立于右侧)
					//角度在90度~270度之间是倒置了
					if (dAngle>PI/2&&dAngle<PI*3/2)
					{
						pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?2.5:-2.5) ;
					}
					else
					{
						pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?-2.5:2.5) ;
					}
					pt_3d_new2.rotateBy(PI/2+dAngle, AcGeVector3d::kZAxis, pt_3d_new) ;
					vec = pt_3d_new2 - pt_3d_new ;
					matrix.setToTranslation(vec) ;
					pRefNew->transformBy(matrix) ; 
					//end 在双线图中，信号机基点不能在中线上，要至少上/下移2.5
					pRefNew->setLayer(_T("sxt")) ;
					m_mysys.AddEntityToDb(pRefNew) ;
					pRefNew->close() ;

					pEnt->close() ;
				}

				cSet.MoveNext() ;
			}

			cSet.Close() ;
		}
		//end 信号机

		//begin 绝缘节
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=2")) ; //整除为2的
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				int iEntType = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_type"), iEntType) ;

				//计算双线中线绝缘节位置时，应以平面图绝缘节和多段线交点作为源数据为宜
				double dEntX = 0.000, dEntY = 0.000 ;				
				cSet.GetFieldValue(_T("sxt_ent_online_intersectx2"), dEntX) ;
				cSet.GetFieldValue(_T("sxt_ent_online_intersecty2"), dEntY) ;
				
				///begin 绝缘节所在线段信息	
				AcGeVector2d vecOnLine = GetAngleOfJyjOnLine(nHandle) ;
				double dAngleOnLine = 0 ;
				dAngleOnLine = vecOnLine.angle() ;
				//end  绝缘节所在线段信息

				//begin 复制平面图中的绝缘节（2份）并经过移动转换到双线中
				AcDbEntity * pEnt = NULL ;
				if (m_mysys.OpenAcDbEntity(nHandle, pEnt))
				{
					AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;
					double dAngle = pRef->rotation() ;
					AcGePoint3d pt_3d_old = pRef->position() ;
					AcDbBlockReference *pRefNew1 = AcDbBlockReference::cast(pRef->clone()) ;
					AcDbBlockReference *pRefNew2 = AcDbBlockReference::cast(pRef->clone()) ;
					AcGePoint3d pt_3d_new(dEntX, dEntY, 0) ;//双线中间线和绝缘节交点位置
					AcGeMatrix3d matrix ;
					AcGeVector3d vec = pt_3d_new - pt_3d_old ;
					matrix.setToTranslation(vec) ;					
					pRefNew1->transformBy(matrix) ; 
					pRefNew2->transformBy(matrix) ; 
					//begin 在双线图中，绝缘节基点不能在中线上，上/下移2.5
					AcGePoint3d pt_3d_new1(pt_3d_new), pt_3d_new2(pt_3d_new) ;
					pt_3d_new1.x = pt_3d_new.x+2.5 ;
					pt_3d_new2.x = pt_3d_new.x-2.5 ;
					pt_3d_new1.rotateBy(PI/2+dAngleOnLine, AcGeVector3d::kZAxis, pt_3d_new) ;
					pt_3d_new2.rotateBy(PI/2+dAngleOnLine, AcGeVector3d::kZAxis, pt_3d_new) ;
					vec = pt_3d_new1 - pt_3d_new ;
					matrix.setToTranslation(vec) ;
					pRefNew1->transformBy(matrix) ; 
					vec = pt_3d_new2 - pt_3d_new ;
					matrix.setToTranslation(vec) ;
					pRefNew2->transformBy(matrix) ; 
					//end 在双线图中，绝缘节基点不能在中线上，上/下移2.5
					pRefNew1->setColorIndex(1) ;
					pRefNew1->setLayer(_T("sxt")) ;
					m_mysys.AddEntityToDb(pRefNew1) ;
					m_mysys.SetXdata(pRefNew1, _T("CloneFrom"), nHandle) ;
					m_mysys.SetXdata(pRefNew1, _T("UPDOWN"), 1) ;
					pRefNew1->close() ;
					pRefNew2->setColorIndex(3) ;
					pRefNew2->setLayer(_T("sxt")) ;
					m_mysys.AddEntityToDb(pRefNew2) ;
					m_mysys.SetXdata(pRefNew2, _T("CloneFrom"), nHandle) ;
					m_mysys.SetXdata(pRefNew2, _T("UPDOWN"), 0) ;
					pRefNew2->close() ;
					pEnt->close() ;
				}
				//end 复制平面图中的绝缘节（2份）并经过移动转换到双线中

				cSet.MoveNext() ;
			}

			cSet.Close() ;
		}
		//end 绝缘节		

		//begin 绘制跳线
		this->DrawJumpLine() ;		
		//end 绘制跳线
		
		//begin 绘制 信号楼(20150605：双线不绘制警冲标）
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type=310")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;

				double dEntX = 0.000, dEntY = 0.000 ;				
				cSet.GetFieldValue(_T("sxt_ent_x2"), dEntX) ;
				cSet.GetFieldValue(_T("sxt_ent_y2"), dEntY) ;
				CString strBlkName ; 
				cSet.GetFieldValue(_T("sxt_ent_block"), strBlkName) ;
				AcDbEntity * pEnt = NULL ;
				if (m_mysys.OpenAcDbEntity(nHandle, pEnt))
				{
					AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;
					double dAngle = pRef->rotation() ;
					AcGePoint3d pt_3d_old = pRef->position() ;
					AcDbBlockReference *pRefNew = AcDbBlockReference::cast(pRef->clone()) ;
					AcGePoint3d pt_3d_new(dEntX, dEntY, 0) ;
					AcGeMatrix3d matrix ;
					AcGeVector3d vec = pt_3d_new - pt_3d_old ;
					matrix.setToTranslation(vec) ;
					pRefNew->transformBy(matrix) ; 
					pRefNew->setLayer(_T("sxt")) ;
					m_mysys.AddEntityToDb(pRefNew) ;
					pRefNew->close() ;

					pEnt->close() ;
				}

				cSet.MoveNext() ;
			}

			cSet.Close() ;
		}
		//end 绘制警冲标 信号楼

		//begin 绘制距离信号楼距离
		AcGePoint3d pt_3d_min_dwg(0,0,0), pt_3d_max_dwg(0,0,0) ; //先取得当前图纸的范围,以便确定绘制距信号楼距离的坐标
		m_mysys.GetCurExtents(pt_3d_min_dwg, pt_3d_max_dwg) ;
		AcGePoint3d pt_3d_insert_toxhldist(pt_3d_min_dwg.x-20,pt_3d_max_dwg.y+100,0) ;
		double dRowHeight = 10 ;
		double dY1 = pt_3d_insert_toxhldist.y ;
		double dY2 = pt_3d_insert_toxhldist.y-dRowHeight ;
		double dY3 = pt_3d_insert_toxhldist.y-dRowHeight*2 ;
		double dY4 = pt_3d_insert_toxhldist.y-dRowHeight*3 ;
		double dY5 = pt_3d_insert_toxhldist.y-dRowHeight*4 ;
		for (int i=0; i<2; i++) //循环处理道岔、信号机
		{
			strSql.Format(_T("select distinct sxt_ent_toxhldist from sxt_ent where sxt_ent_type\\100=%d and sxt_ent_toxhldist<>0 order by sxt_ent_toxhldist"), i) ; //整除为0,dc;为1,xhj
			if (cSet.Open(strSql))
			{
				while(!cSet.IsEOF())
				{
					int iDist = 0 ;
					cSet.GetFieldValue(_T("sxt_ent_toxhldist"), iDist) ;
					strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=%d and sxt_ent_toxhldist=%d"), i, iDist) ;
					CStringArray strArrayName ;
					int iNum = m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strArrayName) ;
					double dCenter_X = 0 ; //信号机或者道岔在双线图中的插入点，作为绘制距离文本的x坐标中心点,多个设备同一x坐标的，名称按此点左右散开
					m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_x2"), dCenter_X) ;
					//距离数值文本
					AcGePoint3d pt_3d_insert_text(dCenter_X, dY5+dRowHeight*3/2+i*dRowHeight*2, 0) ;
					CString strDist ;
					strDist.Format(_T("%d"), abs(iDist)) ;
					m_mysys.AddText(strDist, pt_3d_insert_text, pt_3d_insert_text, AcDb::kTextCenter, AcDb::kTextVertMid, 2.2, 0.8, PI/2) ;
					//信号机名称或道岔编号
					pt_3d_insert_text.y-=dRowHeight ;
					for (int j=0; j<iNum; j++)
					{
						pt_3d_insert_text.x = dCenter_X-iNum*2.4/2+2.4/2+j*2.4 ;
						m_mysys.AddText(strArrayName.GetAt(j), pt_3d_insert_text, pt_3d_insert_text, AcDb::kTextCenter, AcDb::kTextVertMid, 2.2, 0.8, PI/2) ;
					}

					cSet.MoveNext() ;
				}

				cSet.Close() ;
			}
		}
		//插入表头、绘制行线5根
		AcDbBlockReference* pRef = NULL ;
		CString strBlkName = _T("PMT_TOXHL_HEADER") ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_insert_toxhldist))
		{
			pRef->close() ;
			pRef = NULL ;
		}
		for (int i=0; i<5; i++)
		{
			AcGePoint3d pt_3d_start(pt_3d_insert_toxhldist.x+20, pt_3d_insert_toxhldist.y-i*dRowHeight, 0) ;
			AcGePoint3d pt_3d_end(pt_3d_max_dwg.x, pt_3d_insert_toxhldist.y-i*dRowHeight, 0) ;
			m_mysys.AddAcDbPolyLine(pt_3d_start, pt_3d_end, 0.5) ;
		}
		//end 绘制距离信号楼距离
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in Drawsxt"));
	}

	m_pDb->setClayer(objIdOldLayer) ;

	return 0;
}

// 读取各绝缘节、道岔前后关系到relation表
void CZnSxt::FillRelationDB(void)
{
	m_AdoDb.ClearTable(_T("sxt_relation")) ;

	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;

		//查出所有的前后关系(绝缘节、道岔)
		strSql.Format(_T("select distinct sxt_line_handle from sxt_line")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long  nHandleLine = 0 ;
				cSet.GetFieldValue(_T("sxt_line_handle"), nHandleLine) ;
				//begin 查找iHandle多线上所有的道岔、绝缘节
				//1、先查询起始vertex是否连着道岔反位端
				int iNumEntFirst = 0 ;
				strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d and sxt_ent_fwline_vertex=0"), nHandleLine) ;
				long nHandleEntPre = 0, nEntTypePre =  0, nDirectPre = 0, nDfwPre = 0 ;
				iNumEntFirst = m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleEntPre) ;
				if (iNumEntFirst==1)
				{
					int iDwIntersectMoveToVertex = -1 ;
					m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_movetovertex"), iDwIntersectMoveToVertex) ;
					if (iDwIntersectMoveToVertex==-1)
					{
						nDirectPre = 1 ;
					}
					nDfwPre = 1 ;
				}
				else
				{
					//1.1对于分段pmt新增，线段左侧连接的绝缘节
					strSql.Format(_T("select * from sxt_ent where sxt_ent_conline_right=%d and sxt_ent_type\\100=2"), nHandleLine) ;
					iNumEntFirst = m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleEntPre) ;
					m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_type"), nEntTypePre) ;
				}

				//2、再查询在这条线上的绝缘节和道岔（定位相交）
				int iNumEntOnLine = 0 ;
				strSql.Format(_T("select * from sxt_ent where sxt_ent_online_handle=%d and sxt_ent_type<>100 order by sxt_ent_x"), nHandleLine) ;
				CADORecordset cSetTmp(&m_AdoDb) ;
				if (cSetTmp.Open(strSql))
				{
					iNumEntOnLine = cSetTmp.GetRecordCount() ;
					if (iNumEntOnLine>0)
					{
						cSetTmp.MoveFirst() ;
						int iIndexEntOnLine = 0 ;
						while (!cSetTmp.IsEOF())
						{
							long nHandleEntCur = 0, nEntTypeCur =  0, nDirectCur = 0, nDfwCur = 0 ;
							cSetTmp.GetFieldValue(_T("sxt_ent_handle"), nHandleEntCur) ;
							cSetTmp.GetFieldValue(_T("sxt_ent_type"), nEntTypeCur) ;
							int iDwIntersectMoveToVertex = -1 ;
							cSetTmp.GetFieldValue(_T("sxt_ent_movetovertex"), iDwIntersectMoveToVertex) ;
							if (iDwIntersectMoveToVertex!=-1)
							{
								nDirectCur= 1 ;
							}							

							if (nHandleEntPre!=0)
							{
								strSql.Format(_T("insert into sxt_relation (sxt_relation_from_handle, sxt_relation_from_type, sxt_relation_to_handle, sxt_relation_to_type, sxt_relation_direct, sxt_relation_dfw) values(%d, %d, %d, %d, %d, %d)"), nHandleEntPre, nEntTypePre, nHandleEntCur, nEntTypeCur, nDirectPre, nDfwPre) ;
								m_AdoDb.Execute(strSql) ;
							}

							nHandleEntPre = nHandleEntCur ;
							nEntTypePre = nEntTypeCur ;
							nDirectPre = nDirectCur ;
							nDfwPre = nDfwCur ;

							cSetTmp.MoveNext() ;
							iIndexEntOnLine++ ;
						}
					}
					cSetTmp.Close() ;
				}

				//3、最后查询终点vertex是否连着道岔反位端
				int iNumEntLast = 0 ;
				strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d and sxt_ent_fwline_vertex=(select max(sxt_line_vertex)  from sxt_line where sxt_line_handle=%d)"), nHandleLine, nHandleLine) ;
				long nHandleEntLast = 0, nEntTypeLast =  0, nDirectLast = 0, nDfwLast = 0 ;
				iNumEntLast = m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleEntLast) ;
				if (iNumEntLast==1)
				{
					int iDwIntersectMoveToVertex = -1 ;
					m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_movetovertex"), iDwIntersectMoveToVertex) ;
					if (iDwIntersectMoveToVertex==-1)
					{
						nDirectLast = 1 ;
					}
					nDfwLast = 1 ;

					if (nHandleEntPre!=0)
					{
						strSql.Format(_T("insert into sxt_relation (sxt_relation_from_handle, sxt_relation_from_type, sxt_relation_to_handle, sxt_relation_to_type, sxt_relation_direct, sxt_relation_dfw) values(%d, %d, %d, %d, %d, %d)"), nHandleEntPre, nEntTypePre, nHandleEntLast, nEntTypeLast, nDirectLast, nDfwLast) ;
						m_AdoDb.Execute(strSql) ;
					}
				}
				else
				{
					//3.1对于分段pmt新增，线段右侧连接的绝缘节
					strSql.Format(_T("select * from sxt_ent where sxt_ent_conline_left=%d and sxt_ent_type\\100=2"), nHandleLine) ;
					iNumEntLast = m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleEntLast) ;
					m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_type"), nEntTypeLast) ;
					if (iNumEntLast==1)
					{
						if (nHandleEntPre!=0)
						{
							strSql.Format(_T("insert into sxt_relation (sxt_relation_from_handle, sxt_relation_from_type, sxt_relation_to_handle, sxt_relation_to_type, sxt_relation_direct, sxt_relation_dfw) values(%d, %d, %d, %d, %d, %d)"), nHandleEntPre, nEntTypePre, nHandleEntLast, nEntTypeLast, nDirectLast, nDfwLast) ;
							m_AdoDb.Execute(strSql) ;
						}
					}
				}
				//end 查找iHandle多线上所有的道岔、绝缘节

				cSet.MoveNext() ;
			}
		}

		//计算各关系所属轨道区段编号(此处轨道区段不是轨道电路，而是极性概念上的区段，其小于等于轨道电路区段)
		strSql.Format(_T("select * from sxt_relation where sxt_relation_from_type\\100=2 and not (sxt_relation_to_type=0 and sxt_relation_dfw=1)")) ;
		if (cSet.Open(strSql))
		{
			int iIndexGuiDao = 0 ;
			while (!cSet.IsEOF())
			{
				long nHandleFrom = 0 ;
				cSet.GetFieldValue(_T("sxt_relation_from_handle"), nHandleFrom) ;
				this->FindOneGuiDao(nHandleFrom, iIndexGuiDao) ;
				iIndexGuiDao++ ;

				cSet.MoveNext() ;
			}

			cSet.Close();
		}

		//计算极性
		strSql.Format(_T("select distinct sxt_relation_polarsn from sxt_relation order by sxt_relation_polarsn")) ;
		if (cSet.IsOpen())
		{
			cSet.Close() ;
		}
		if (cSet.Open(strSql))
		{
			//计算下一个极性区段
			while (!cSet.IsEOF())
			{
				int iPolarSn = 0 ;
				cSet.GetFieldValue(_T("sxt_relation_polarsn"), iPolarSn) ;
				strSql.Format(_T("select * from sxt_relation where sxt_relation_polarsn=%d"), iPolarSn) ;
				CADORecordset cSetSection(&m_AdoDb) ;
				if (cSetSection.Open(strSql))
				{
					if (cSetSection.GetRecordCount()>0)
					{
						cSetSection.MoveFirst() ;
						while (!cSetSection.IsEOF())
						{
							int nId = 0 ;
							cSetSection.GetFieldValue(_T("id"), nId) ;
							int iTypeTo = -1 ;
							cSetSection.GetFieldValue(_T("sxt_relation_to_type"), iTypeTo) ;
							if (iTypeTo/100==2) //结束是绝缘节的紧邻下个轨道
							{
								long nHandleTo = 0 ;
								cSetSection.GetFieldValue(_T("sxt_relation_to_handle"), nHandleTo) ;
								strSql.Format(_T("update sxt_relation a, (select top 1 sxt_relation_polarsn from sxt_relation where sxt_relation_from_handle=%d) as b set a.sxt_relation_nextpolar=b.sxt_relation_polarsn where a.id=%d"), nHandleTo, nId) ;
								m_AdoDb.Execute(strSql) ;
							}
							cSetSection.MoveNext() ;
						}
					}
					cSetSection.Close() ;
				}
				cSet.MoveNext() ;
			}

			cSet.Close();
		}

		//计算极性
		//先给一个起始区段设定极性0
		int iPolarFirst = -1 ;
		strSql.Format(_T("select top 1 * from sxt_relation where sxt_relation_from_handle in(select sxt_ent_handle from sxt_ent where sxt_ent_type>=200 and sxt_ent_type<220 and sxt_ent_handle not in(select distinct sxt_relation_to_handle from sxt_relation))")) ;
		if (cSet.Open(strSql))
		{
			if (cSet.GetRecordCount()>0)
			{
				cSet.MoveFirst() ;

				cSet.GetFieldValue(_T("sxt_relation_polarsn"), iPolarFirst) ;

				cSet.Edit() ;
				cSet.SetFieldValue(_T("sxt_relation_drawoption"), 0) ;
				cSet.Update() ;
			}
			cSet.Close() ;
		}
		//以iBelongGuiDaoFirst为起点往后顺序设置极性
		this->FillPolar(iPolarFirst, 0) ;
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in FillRelationDB"));
	}
}

// 查找由一个绝缘节起始的一条轨道包含的区段
void CZnSxt::FindOneGuiDao(long nHandleFrom, const int iIndexGuiDao, int iForword)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;

		if (iForword==0)
		{
			strSql.Format(_T("select *  from sxt_relation where sxt_relation_from_handle=%d and sxt_relation_done<>1"), nHandleFrom) ;
			if (cSet.Open(strSql))
			{
				if (cSet.GetRecordCount()>0)
				{
					cSet.MoveFirst() ;
					while (!cSet.IsEOF())
					{
						cSet.Edit() ;
						cSet.SetFieldValue(_T("sxt_relation_polarsn"), iIndexGuiDao) ;
						cSet.SetFieldValue(_T("sxt_relation_done"), 1) ;
						cSet.Update() ;

						int iTypeTo = -1 ;
						cSet.GetFieldValue(_T("sxt_relation_to_type"), iTypeTo) ;
						if (iTypeTo==0)//dc
						{
							long  nHandleTo = 0 ;
							cSet.GetFieldValue(_T("sxt_relation_to_handle"), nHandleTo) ;
							FindOneGuiDao(nHandleTo, iIndexGuiDao) ;
						}

						int iTypeFrom = -1 ;
						cSet.GetFieldValue(_T("sxt_relation_from_type"), iTypeFrom) ;
						if (iTypeFrom==0)
						{
							FindOneGuiDao(nHandleFrom, iIndexGuiDao,-1) ;
						}

						cSet.MoveNext() ;
					}
				}

				cSet.Close() ;
			}
		}
		else if (iForword==-1)
		{
			strSql.Format(_T("select *  from sxt_relation where sxt_relation_to_handle=%d and sxt_relation_done<>1"), nHandleFrom) ;
			if (cSet.Open(strSql))
			{
				if (cSet.GetRecordCount()>0)
				{
					cSet.MoveFirst() ;
					while (!cSet.IsEOF())
					{
						cSet.Edit() ;
						cSet.SetFieldValue(_T("sxt_relation_polarsn"), iIndexGuiDao) ;
						cSet.SetFieldValue(_T("sxt_relation_done"), 1) ;
						cSet.Update() ;

						int iTypeFrom = -1 ;
						cSet.GetFieldValue(_T("sxt_relation_from_type"), iTypeFrom) ;
						if (iTypeFrom==0)//dc
						{
							long  nHandleFrom = 0 ;
							cSet.GetFieldValue(_T("sxt_relation_from_handle"), nHandleFrom) ;
							FindOneGuiDao(nHandleFrom, iIndexGuiDao, -1) ;
						}
						cSet.MoveNext() ;
					}
				}
				cSet.Close() ;
			}
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in FindOneGuiDao"));
	}
}

// 顺序设置极性，循环过程
void CZnSxt::FillPolar(int iPolarSn, int iDrawOption)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_relation where sxt_relation_polarsn=%d and sxt_relation_nextpolar<>-1"), iPolarSn) ;
		if (cSet.Open(strSql))
		{
			if (cSet.GetRecordCount()>0)
			{
				cSet.MoveFirst() ;
				while (!cSet.IsEOF())
				{
					int iPolarNext = -1 ;
					cSet.GetFieldValue(_T("sxt_relation_nextpolar"), iPolarNext) ;
					int iDrawOptionNext = (iDrawOption+1)%2 ;
					//查看iPolarNext的区段是否已设置了DrawOption，并且是否和要设置的值iDrawOptionNext冲突
					int iDrawOptionNextTmp = -1 ;
					strSql.Format(_T("select * from sxt_relation where sxt_relation_polarsn=%d"), iPolarNext) ;
					m_RecordSet.GetRecordValue(strSql, _T("sxt_relation_drawoption"), iDrawOptionNextTmp) ;
					if (iDrawOptionNextTmp==-1)
					{
						strSql.Format(_T("update sxt_relation set sxt_relation_drawoption=%d where sxt_relation_polarsn=%d"), iDrawOptionNext, iPolarNext) ;
						m_AdoDb.Execute(strSql) ;
                        FillPolar(iPolarNext, iDrawOptionNext) ;					
					}
					else if (iDrawOptionNextTmp!=iDrawOptionNext)
					{
						CString strMsg ;
						strMsg.Format(_T("冲突 Cur belong: %d，drawop:%d， next belong: %d，drawop:%d"), iPolarSn, iDrawOption, iPolarNext, iDrawOptionNextTmp) ;
						AfxMessageBox(strMsg) ;
					}

					cSet.MoveNext() ;
				}
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in FillPolar"));
	}
}

// 调整图纸
void CZnSxt::AdjustDwg(void)
{
	AcDbDatabase *pDb = acdbHostApplicationServices()->workingDatabase() ;	
	CUserSystem mysys ;
	mysys.m_pDb = pDb ;

	acedInitGet(RSG_NONULL, _T("All Sel")) ;
	TCHAR chInput[10]  ;
	int iRet = acedGetKword(_T("\n[全图(All)/选择(Sel)]<全图>:"), chInput) ;
	if (RTNORM==iRet)
	{		
		if (_tcscmp(chInput, _T("All"))==0)
		{
			AcDbBlockTable * pBlkTbl = NULL ;
			pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

			AcDbBlockTableRecord * pBlkTblRec = NULL ;
			pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
			pBlkTbl->close() ;
			AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
			pBlkTblRec->newIterator( pBlkTblRecIter );
			pBlkTblRec->close();

			AcDbObjectIdArray objArray ;
			AcDbEntity * pEnt = NULL ;
			for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
			{
				//遍历并打开实体 ;	
				if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
				{
					if (pEnt->isKindOf(AcDbBlockReference::desc()))
					{
						AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;
						CString strBlkName ;
						mysys.GetBlockName(pRef, strBlkName) ;
						int iBlkType = 0 ;
						if (strBlkName.Left(2)==_T("DC"))
						{
							iBlkType = 0 ;
						}
						else if (strBlkName.Left(3)==_T("XHJ"))
						{
							iBlkType = 1 ;
						}
						this->AdjustBlock(pRef, iBlkType) ;
					}

					pEnt->close() ;
				}	
			}
			delete pBlkTblRecIter ;
			pBlkTblRecIter = NULL ;
		}
		else if (_tcscmp(chInput, _T("Sel"))==0)
		{
			struct resbuf *pRb ;
			pRb = acutBuildList(RTDXF0,_T("INSERT"),0) ;	

			long iLen = 0 ;
			ads_name sum_name ;
			if (Acad::eNormal==acedSSGet(NULL, NULL, NULL, pRb, sum_name))
			{	
				acedSSLength(sum_name, &iLen) ;
				for (int i=0; i<iLen; i++)
				{
					ads_name en ;
					acedSSName(sum_name, i, en) ;
					AcDbEntity *pEnt = NULL ;
					AcDbObjectId ent_id ;
					acdbGetObjectId(ent_id, en) ;
					if(Acad::eOk == acdbOpenAcDbEntity(pEnt,ent_id, AcDb::kForWrite))
					{
						AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;
						CString strBlkName ;
						mysys.GetBlockName(pRef, strBlkName) ;
						int iBlkType = 0 ;
						if (strBlkName.Left(2)==_T("DC"))
						{
							iBlkType = 0 ;
						}
						else if (strBlkName.Left(3)==_T("XHJ"))
						{
							iBlkType = 1 ;
						}
						this->AdjustBlock(pRef, iBlkType) ;

						pEnt->close() ;
					}
				}
			}
		}	
	}
}

// 调整道岔、信号机块的属性，使其符合美观
void CZnSxt::AdjustBlock(AcDbBlockReference*& pRef, int iType)
{
	//将DC_00统一为X比例和旋转角变化（X、Y都置-1，等同于旋转180度，X置-1并且旋转180度等同于Y置-1，Y置-1并且旋转180度等同于X置-1）
	AcGeScale3d scale3d = pRef->scaleFactors()  ;	
	double dSX = scale3d.sx ;
	double dSY = scale3d.sy;

	if (dSY<0)
	{
		pRef->upgradeOpen() ;
		scale3d.set(-dSX, -dSY, scale3d.sz) ;
		pRef->setScaleFactors(scale3d) ;
		pRef->setRotation(pRef->rotation()-PI) ;
		pRef->downgradeOpen() ;
	}	

	double dAngleBlk = pRef->rotation() ;
	//得到块的属性
	AcDbObjectIterator * pObjIter ;
	pObjIter = pRef->attributeIterator() ;
	for(pObjIter->start(); !pObjIter->done(); pObjIter->step()) 
	{
		AcDbObjectId attId ;
		attId = pObjIter->objectId() ;
		AcDbAttribute * pAttrib ;
		if(Acad::eOk==pRef->openAttribute(pAttrib, attId, AcDb::kForWrite, Adesk::kFalse) )
		{
			bool b1 =  pAttrib->isMirroredInX() ;
			bool b2 =  pAttrib->isMirroredInY() ;
			if (iType==0)
			{
				if (dSX==-1)
				{
					if (dAngleBlk>(PI/2) && dAngleBlk<(PI*3/2))
					{
						pAttrib->mirrorInY(!b2) ;
						pAttrib->setVerticalMode(AcDb::kTextTop) ;
					}
					else
					{
						pAttrib->mirrorInX(!b1) ;
					}
				}
				else
				{
					if (dAngleBlk>(PI/2) && dAngleBlk<(PI*3/2))
					{
						pAttrib->setRotation(pAttrib->rotation()-PI) ;
						pAttrib->setVerticalMode(AcDb::kTextTop) ;
					}
				}
			}
			else
			{
				if (dAngleBlk>(PI/2) && dAngleBlk<(PI*3/2))
				{
					pAttrib->setRotation(pAttrib->rotation()-PI) ;
					pAttrib->setHorizontalMode(AcDb::kTextLeft) ;
					pAttrib->setVerticalMode(AcDb::kTextTop) ;
				}
			}
			pAttrib->close() ;	
		}						
	}
}

// 将平面图转换为由一段一段区段组成的平面图
void CZnSxt::ConvertPMT(void)
{
	AcDbDatabase *pDb = acdbHostApplicationServices()->workingDatabase() ;
	//AcDbDatabase *pDbNew = new AcDbDatabase ;    
	this->SetAcDbDB(pDb) ;
	//m_mysys.Wblock(pDb, pDbNew) ;
	m_mysys.DelAllXdata() ; //删除图上所有xdata

	//this->SetAcDbDB(pDbNew) ;
	this->ReadAllEntToDB() ;

	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select distinct sxt_line_handle from sxt_line") ) ;
		if (cSet.Open(strSql))
		{
			if (cSet.GetRecordCount()>0)
			{
				cSet.MoveFirst() ;
				while (!cSet.IsEOF())
				{
					long nLineHandle = 0 ;
					cSet.GetFieldValue(_T("sxt_line_handle"), nLineHandle) ;

					AcGePoint2dArray pt2dArrayLine ; //每条多段线的（每个区段）顶点
					AcArray<double> dPlineWidths ; //每段多段线的宽
					strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d order by sxt_line_vertex"), nLineHandle) ;
					CADORecordset cSetVertex(&m_AdoDb) ;
					if (cSetVertex.Open(strSql))
					{
						int iVertexNum = cSetVertex.GetRecordCount() ;
						if (iVertexNum>0)
						{
							cSetVertex.MoveFirst() ;
							double dStartWidth = 0, dEndWidth = 0 ; //该段线宽
							while (!cSetVertex.IsEOF())
							{
								int iVertextIndex = 0 ;
								cSetVertex.GetFieldValue(_T("sxt_line_vertex"), iVertextIndex) ;
								//取得该段线顶点线宽
								m_mysys.GetPlineWidthAt(nLineHandle, iVertextIndex, dStartWidth, dEndWidth) ;
								//顶点
								double dVertexX  , dVertexY ; 
								cSetVertex.GetFieldValue(_T("sxt_line_vertex_x"), dVertexX) ;
								cSetVertex.GetFieldValue(_T("sxt_line_vertex_y"), dVertexY) ;
								pt2dArrayLine.append(AcGePoint2d(dVertexX, dVertexY)) ;
								dPlineWidths.append(dStartWidth) ;
								
								if (iVertextIndex!=iVertexNum-1)
								{
									//该段线上的实体（绝缘节）
									strSql.Format(_T("select * from sxt_ent where sxt_ent_online_handle=%d and sxt_ent_online_vertex=%d and sxt_ent_type/100=2 order by sxt_ent_x"), nLineHandle, iVertextIndex, nLineHandle) ;
									CADORecordset cSetJyjOnLine(&m_AdoDb) ;
									if (cSetJyjOnLine.Open(strSql))
									{
										if (cSetJyjOnLine.GetRecordCount()>0)
										{
											cSetJyjOnLine.MoveFirst() ;
											while (!cSetJyjOnLine.IsEOF())
											{
												cSetJyjOnLine.GetFieldValue(_T("sxt_ent_online_intersectx"), dVertexX) ;
												cSetJyjOnLine.GetFieldValue(_T("sxt_ent_online_intersecty"), dVertexY) ;
												pt2dArrayLine.append(AcGePoint2d(dVertexX, dVertexY)) ;
												dPlineWidths.append(dStartWidth) ;
												
												//遇到一个绝缘节，既是一个区段的终点，也是下一个区段的起点
												//作为终点，需要绘制这一阶段的线
												AcDbObjectId objIdClayer = m_pDb->clayer() ;
												m_mysys.Set_Layer(_T(""), _T("convert_pmt_guidao")) ;
												AcDbObjectId objIdPline = m_mysys.AddAcDbPolyLine(pt2dArrayLine, dPlineWidths) ;
												m_mysys.SetXdata(objIdPline,_T("ISGUIDAO"), 1) ;
												m_pDb->setClayer(objIdClayer) ;
												//作为起点，要初始化pt2dArrayLine，并将起点加入点集合(保留点集合最后一个点即可)，一种例外：这个绝缘节刚好在多段线顶点，这时不要保留此起点
												//begin 此处不会儿到EOF
												cSetVertex.MoveNext() ;
												double dTmpX = 0, dTmpY = 0 ;
												cSetVertex.GetFieldValue(_T("sxt_line_vertex_x"), dTmpX) ;
												cSetVertex.GetFieldValue(_T("sxt_line_vertex_y"), dTmpY) ;
												if (fabs(dTmpX-dVertexX)<0.01 &&fabs(dTmpY-dVertexY)<0.01) //clear
												{
													pt2dArrayLine.removeSubArray(0, pt2dArrayLine.length()-1) ;
													dPlineWidths.removeSubArray(0, dPlineWidths.length()-1) ;
												}
												else //left one
												{
													pt2dArrayLine.removeSubArray(0, pt2dArrayLine.length()-2) ;
													dPlineWidths.removeSubArray(0, dPlineWidths.length()-2) ;
												}
												cSetVertex.MovePrevious() ;
												//end
												
												cSetJyjOnLine.MoveNext() ;
											}
										}
										cSetJyjOnLine.Close() ;
									}
								}
								else 
								{
									AcDbObjectId objIdClayer = m_pDb->clayer() ;
									m_mysys.Set_Layer(_T(""), _T("convert_pmt_guidao")) ;
									AcDbObjectId objIdPline = m_mysys.AddAcDbPolyLine(pt2dArrayLine, dPlineWidths) ;
									m_mysys.SetXdata(objIdPline,_T("ISGUIDAO"), 1) ;
									m_pDb->setClayer(objIdClayer) ;
								}
								cSetVertex.MoveNext() ;
							}
						}
						cSetVertex.Close() ;
					}
					cSet.MoveNext() ;
				}

				cSet.MoveFirst() ;
				while (!cSet.IsEOF())
				{
					long nLineHandle = 0 ;
					cSet.GetFieldValue(_T("sxt_line_handle"), nLineHandle) ;
					AcDbEntity *pEnt = NULL ;
					if (m_mysys.OpenAcDbEntity(nLineHandle, pEnt))
					{
						pEnt->erase() ;
						pEnt->close() ;
					}

					cSet.MoveNext() ;
				}
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in ConvertPMT"));
	}

// 	AcApDocManager *pDocMgr = acDocManager ;
// 	AcApDocument *pDocCur = pDocMgr->curDocument() ;
// 	CString strFileNameCur = pDocCur->fileName() ;
// 	CString strFileNameNew ;
// 	strFileNameNew.Format(_T("%s(程序转换图).dwg"), strFileNameCur.Left(strFileNameCur.GetLength()-4)) ;
// 	pDbNew->saveAs(strFileNameNew) ;
// 	delete pDbNew ;
// 	pDbNew = NULL ;
// 	TCHAR * chFileNameNew = NULL ;
// 	chFileNameNew = (LPTSTR)(LPCTSTR)strFileNameNew ;
// 	pDocMgr->executeInApplicationContext(CUserSystem::OpenDocument,(void *)chFileNameNew) ;

}

void CZnSxt::SetAcDbDB(AcDbDatabase * pDb)
{
	m_pDb = pDb ;
	m_mysys.m_pDb = pDb ;
}

// 找出所有的轨道（根据绝缘节划分的，不是根据送受电的轨道电路，不带DG1之类的）
void CZnSxt::FindAllGuiDao(void)
{
	this->ReadAllEntToDB2() ;

	//先清理既有
	this->ClearShowInfo(_T("PMT_SHOWINFO_GUIDAONAME")) ; //删除显示信息
	m_mysys.WriteToRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_GUIDAONAME"), 0) ;//当前是显示，下次运行显示隐藏轨道就是要隐藏了。

	//删除扩展信息
	CString strAppNames[6] = {_T("GUIDAOID"), _T("IS1FNJ"), _T("ISMAINSEC"),  _T("LEFT_FORS"), _T("RIGHT_FORS"), _T("GUIDAONAME")} ;
	CStringArray strArrayAppName ;
	for(int i=0; i<6; i++)
	{
		strArrayAppName.Add(strAppNames[i]) ;
	}
	this->DeleteXdata(strArrayAppName) ;

	AcDbObjectId objIdClayer = m_pDb->clayer() ;
	m_mysys.Set_Layer(_T(""), _T("PMT_SHOWINFO_GUIDAONAME")) ;

	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select distinct sxt_line_handle from sxt_line") ) ;
		if (cSet.Open(strSql))
		{
			if (cSet.GetRecordCount()>0)
			{	
				CLongArray nArrayLineDone ; //已经处理过的线
				while (!cSet.IsEOF())
				{
					long nLineHandle = 0 ;
					cSet.GetFieldValue(_T("sxt_line_handle"), nLineHandle) ;
					if (nLineHandle==386098)
					{
						int iii = 0 ;
					}
                    
					//acutPrintf(_T("\n%d"), nLineHandle) ;
					strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d"), nLineHandle) ;
					if (CBlkUtility::FindLongInArray(nArrayLineDone, nLineHandle)!=-1||m_RecordSet.GetRecordCount(strSql)>0)//已经处理过的线跳过//如果该线和道岔反位相连，亦跳过
					{
						cSet.MoveNext() ;
						continue;
					}

					CLongArray nArrayHandleXHJ, nArrayHandleDC, nArrayLineDoneCur, nArrayLineCurType ;
					this->GetGuiDaoInfo(nLineHandle, nArrayHandleXHJ, nArrayHandleDC, nArrayLineDoneCur, nArrayLineCurType) ;	
					nArrayLineDone.Append(nArrayLineDoneCur) ;

					int iNumDC = nArrayHandleDC.GetCount() ;//当前轨道电路包含几个道岔
					int iNumXHJ = nArrayHandleXHJ.GetCount() ; //当前轨道电路连接有几架信号机
					int iNumLine = nArrayLineDoneCur.GetCount() ; //当前轨道电路由几段组成
					long nHandleLineMain = 0 ; //写轨道电路名称的line
					CString strGuiDaoName ; //轨道电路名称（此处是主轨道，类似3DG1之类的都是附属在主轨道上的）
					if (iNumDC>0) //道岔区段
					{
						if (iNumDC==1) //example：3DG
						{
							CString strNameDC;		
							strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nArrayHandleDC.GetAt(0)) ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strNameDC) ;
												
							strGuiDaoName.Format(_T("%sDG"), strNameDC) ;
							strSql.Format(_T("select sxt_ent_online_handle from sxt_ent where sxt_ent_handle=%d"), nArrayHandleDC.GetAt(0)) ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_handle"), nHandleLineMain) ;							
						}
						else  //example:1-3DG
						{
							int iMinDC = 0, iMaxDC = 0 ;
							this->GetMaxMinDC(nArrayHandleDC, iMaxDC, iMinDC) ;
							strGuiDaoName.Format(_T("%d-%dDG"), iMinDC, iMaxDC) ;
							CString strLineDoneCur ;
							CBlkUtility::ConvertArrayToString(nArrayLineDoneCur, _T(","),  strLineDoneCur) ;
							strSql.Format(_T("select top 1 sxt_ent_online_handle from sxt_ent where sxt_ent_online_handle in (%s)"),  strLineDoneCur) ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_handle"), nHandleLineMain) ;
						}
					}
					else //无岔区段
					{
						if (iNumXHJ==1) //一般是一个调车信号机和牵出线绝缘节组成的类似D8G或者股道分割4G1 4G2
						{
							strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nArrayHandleXHJ.GetAt(0)) ;
							CString strBlockName ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_block"), strBlockName) ;
							CString strXHJName ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strXHJName) ;
							CStringArray strArrayBlockName ;
							CBlkUtility::DivideString(strBlockName, _T("_"), strArrayBlockName) ;
							if (strArrayBlockName.GetCount()>1)
							{
								CString strXHJType ;
								strXHJType = strArrayBlockName.GetAt(1) ;
								if (strXHJType==_T("LF"))
								{
									int iGuDaoNum = 0 ;
									if (strXHJName.Left(1)==_T("S"))
									{
										iGuDaoNum = 1 ;										
									}
									else
									{
										iGuDaoNum =2 ;
									}
									strGuiDaoName.Format(_T("%dG%d"), strXHJName.Right(strXHJName.GetLength()-1), iGuDaoNum ) ;
								}
								else if (strXHJType==_T("DC"))
								{
									strGuiDaoName.Format(_T("%sG"), strXHJName) ;
								}
							}
						}
						else if (iNumXHJ==2)
						{
							CString strXHJTypes[2] = {_T(""), _T("")} ;
							CString strXHJNames[2] = {_T(""), _T("")} ;
							for (int i=0; i<2; i++)
							{
								strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nArrayHandleXHJ.GetAt(i)) ;
								CString strBlockName ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_block"), strBlockName) ;
								CStringArray strArrayBlockName ;
								CBlkUtility::DivideString(strBlockName, _T("_"), strArrayBlockName) ;
								strXHJTypes[i] = strArrayBlockName.GetAt(1) ;
								
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strXHJNames[i]) ;
							}
							if (strXHJTypes[0]==_T("LF") && strXHJTypes[1]==_T("LF"))	//若2侧都是发车信号机，则是股道
							{
								strGuiDaoName.Format(_T("%sG"), strXHJNames[0].Right(strXHJNames[0].GetLength()-1) ) ;
							}
							else if (strXHJTypes[0]==_T("YG")||strXHJTypes[1]==_T("YG")) //预告区段
							{
								strGuiDaoName.Format(_T("%sG"), strXHJTypes[0]==_T("YG")?strXHJNames[0]:strXHJNames[1] ) ;
							}
							else if ((strXHJTypes[0]==_T("LJ")&&strXHJTypes[1]==_T("DC")) || (strXHJTypes[1]==_T("LJ")&&strXHJTypes[0]==_T("DC")) )//IAG IIBG
							{
								CString strAOrBG = _T("B") ;
								if (strXHJNames[0].Left(1)==_T("X")||strXHJNames[1].Left(1)==_T("X"))
								{
									strAOrBG = _T("A") ;
								}
								//查找水平坐标上的出站信号机，得出股道编号
								strSql.Format(_T("select * from sxt_ent where abs(sxt_ent_y-(select sxt_ent_y from sxt_ent where sxt_ent_handle=%d))<1 and left(sxt_ent_block,6)='XHJ_LF'"), nArrayHandleXHJ.GetAt(0)) ;
								CString strCZXHJ ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strCZXHJ) ;
								if (!strCZXHJ.IsEmpty())
								{
									strGuiDaoName.Format(_T("%s%sG"), strCZXHJ.Right(strCZXHJ.GetLength()-1), strAOrBG) ;
								}								
							}
							else //31/33WG
							{
								CString strLeftDC = _T("XXX"),  strRightDC = _T("XXX") ;
								strSql.Format(_T("select sxt_ent_name from sxt_ent where sxt_ent_online_handle=(select top 1 sxt_ent_conline_left from sxt_ent where sxt_ent_conline_right=%d) or sxt_ent_fwline_handle=(select top 1 sxt_ent_conline_left from sxt_ent where sxt_ent_conline_right=%d)"), nArrayLineDoneCur.GetAt(0), nArrayLineDoneCur.GetAt(0)) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strLeftDC) ;
								strSql.Format(_T("select sxt_ent_name from sxt_ent where sxt_ent_online_handle=(select top 1 sxt_ent_conline_right from sxt_ent where sxt_ent_conline_left=%d) or sxt_ent_fwline_handle=(select top 1 sxt_ent_conline_right from sxt_ent where sxt_ent_conline_left=%d) "), nArrayLineDoneCur.GetAt(0), nArrayLineDoneCur.GetAt(0)) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strRightDC) ;
								int iLeftDC = _tstoi(strLeftDC) ;
								int iRightDc = _tstoi(strRightDC) ;
								strGuiDaoName.Format(_T("%d/%dWG"), min(iLeftDC, iRightDc), max(iLeftDC, iRightDc) ) ;
							}
						}

						nHandleLineMain = nArrayLineDoneCur.GetAt(0) ;
					}

					//绘制轨道名称
					if (nHandleLineMain!=0 && !strGuiDaoName.IsEmpty())
					{
						long nMaxGuiDaoId = this->GetMaxGuiDaoId() ;
						int iGuiDaoNameIndex = 0 ; //一送多受区段编号
						int iNumLineNotCross = iNumLine ;
						for (int i=0; i<iNumLine; i++)
						{	
							if (nArrayLineCurType.GetAt(i)==1)
							{
								iNumLineNotCross-- ;
							}
						}

						for (int i=0; i<iNumLine; i++)
						{	
							long nHandleLineTmp = nArrayLineDoneCur.GetAt(i) ;

							CString strGuiDaoNameTmp = strGuiDaoName ;
							CString strLeftForS = _T("N"), strRightForS = _T("N") ;
							int iIsMainSec = 0 ;
							int iIS1FNJ = iNumLineNotCross>1?1:0 ;						
							//是否是渡线
							if (nArrayLineCurType.GetAt(i)==1)
							{
								strGuiDaoNameTmp = _T("") ;
							}
							int iLineType = nArrayLineCurType.GetAt(i) ;

							AcDbEntity *pEnt = NULL ;
							if (m_mysys.OpenAcDbEntity(nHandleLineTmp, pEnt))
							{
								if (nHandleLineTmp==nHandleLineMain)
								{
									iIsMainSec = 1 ;
									strLeftForS = _T("F") ;
									strRightForS = _T("S") ;								
								}
								if (nHandleLineTmp!=nHandleLineMain)
								{
									iIsMainSec = 0 ;
									if (iLineType!=1)
									{
										strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d"), nHandleLineTmp) ;
										int iFwLineVertex = 0 ;
										m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_fwline_vertex"), iFwLineVertex) ;
										if (iFwLineVertex>0) //左侧受电
										{
											strLeftForS = _T("S") ;
										}
										else
										{
											strRightForS = _T("S") ;
										}
										iGuiDaoNameIndex++ ;
										strGuiDaoNameTmp.AppendFormat(_T("%d"),  iGuiDaoNameIndex) ;
									}
								}

								m_mysys.SetXdata(pEnt, _T("GUIDAOID"), nMaxGuiDaoId+1) ; //将本次区段标记为一个轨道电路（xdata：GUIDAOID）
								m_mysys.SetXdata(pEnt, _T("ISMAINSEC"), iIsMainSec) ; 
								m_mysys.SetXdata(pEnt, _T("IS1FNJ"), iIS1FNJ) ;
								m_mysys.SetXdata(pEnt, _T("RIGHT_FORS"), strRightForS) ;	
								m_mysys.SetXdata(pEnt, _T("LEFT_FORS"),strLeftForS) ;	
								m_mysys.SetXdata(pEnt, _T("GUIDAONAME"), strGuiDaoNameTmp) ;
								
								if (!strGuiDaoNameTmp.IsEmpty())
								{
									this->ShowGdName(pEnt) ;
								}

								pEnt->close() ;
							}
						}						
					}

					cSet.MoveNext() ;
				}
			}

			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in FindAllGuiDao"));
	}

	m_pDb->setClayer(objIdClayer) ;
}

// 将所有实体读入数据库（程序转换平面图中）
void CZnSxt::ReadAllEntToDB2(void)
{
	CString strMsg ;

	acutPrintf(_T("\n读取平面图，注意此函数针对一张dwg中一个车站情况,默认搜索距离信号楼长度为%.0f"), m_dMaxLenPmt) ;
	//找到距离信号楼距离框的图块 
	AcGePoint3d pt_3d_toxhlheader, pt_3d_toxhlheader2 ;//距信号楼距离图块基点
	if(this->GetToXhlHeaderPos(pt_3d_toxhlheader, pt_3d_toxhlheader2)!=1)
	{
		AfxMessageBox(_T("未发现距信号楼距离栏图块 PMT_TOXHL_HEADER，程序退出，请处理!")) ;
		return ;
	}

	double dY1 = pt_3d_toxhlheader.y ; //距离栏最上一条线
	double dY2 = pt_3d_toxhlheader.y - m_dRowHeightToXhl ;
	double dY3 = pt_3d_toxhlheader.y - m_dRowHeightToXhl*2 ;
	double dY4 = pt_3d_toxhlheader.y - m_dRowHeightToXhl*3 ;
	double dY5 = pt_3d_toxhlheader.y - m_dRowHeightToXhl*4 ;

	int nLen = 0 ;
	struct resbuf* pRb ;
	CLongArray nArrayHandle ;

	//在header下面的实体集合
	pRb = acutBuildList(-4,  _T("*,<,*"), 10,  asDblArray(pt_3d_toxhlheader),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	m_AdoDb.ClearTable(_T("sxt_ent")) ;
	m_AdoDb.ClearTable(_T("sxt_line")) ;

	CString strSql ;
	for(int i=0; i<nLen; i++)
	{
		UINT nHandle =nArrayHandle.GetAt(i) ;
		AcDbEntity * pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;
				AcGePoint3d pt_3d_position = pRef->position() ;
				//将块统一为X比例和旋转角变化（X、Y都置-1，等同于旋转180度，X置-1并且旋转180度等同于Y置-1，Y置-1并且旋转180度等同于X置-1）
				AcGeScale3d scale_3d = pRef->scaleFactors()  ;		
				if (scale_3d.sy<0)
				{
					pRef->upgradeOpen() ;
					scale_3d.set(-scale_3d.sx, -scale_3d.sy, scale_3d.sz) ;
					pRef->setScaleFactors(scale_3d) ;
					pRef->setRotation(pRef->rotation()-PI) ;
					pRef->downgradeOpen() ;
				}	

				CString strName ;
				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;
				//acutPrintf(_T("坐标 %.3f-%.3f,块名：%s"), pt_3d_position.x, pt_3d_position.y, strBlkName) ;

				CStringArray strArrayBlkName ;
				CBlkUtility::DivideString(strBlkName, _T("_"), strArrayBlkName) ;
				CString strBlkType = _T("") ;
				if (strArrayBlkName.GetCount()>0)
				{
					strBlkType = strArrayBlkName.GetAt(0) ;
				}

				if (strBlkName==_T("PMT_JCB"))
				{
					m_mysys.GetAttValue(pRef, _T("LEN"), strName) ;

					strSql.Format(_T("insert into sxt_ent(sxt_ent_handle,  sxt_ent_block, sxt_ent_x, sxt_ent_y,  sxt_ent_name, sxt_ent_type,  sxt_ent_x1, sxt_ent_y1)  \
									 values(%d, '%s', %.10f, %.10f,  '%s', %d, %.10f, %.10f) "), nHandle, strBlkName,pt_3d_position.x, pt_3d_position.y, strName, 300, pt_3d_position.x, pt_3d_position.y) ;
					m_AdoDb.Execute(strSql) ;
				}
				else if (strBlkName.Left(8)==_T("PMT_XHL_")||strBlkName.Left(8)==_T("PMT_ZBS_"))
				{
					strSql.Format(_T("insert into sxt_ent(sxt_ent_handle,  sxt_ent_block, sxt_ent_x, sxt_ent_y,  sxt_ent_type,  sxt_ent_x1, sxt_ent_y1)  \
									 values(%d, '%s', %.10f, %.10f,  %d, %.10f, %.10f) "), nHandle, strBlkName,pt_3d_position.x, pt_3d_position.y, 310, pt_3d_position.x, pt_3d_position.y) ;
					m_AdoDb.Execute(strSql) ;
				}

				if (strBlkType==_T("DC"))
				{
					m_mysys.GetAttValue(pRef, _T("BIANHAO"), strName) ;
					//acutPrintf(_T("\n道岔编号%s"), strName) ;
					//和DC块定/反位连接的线信息
					CLongArray nArrayHandleIntersectLine, nArrayIndexOfVertex ;
					int iDwIntersectMoveToVertex = -1 ;
					AcGePoint3dArray pt3dArrayIntersect ;
					if(this->GetDcConLineDFW(pRef, nArrayHandleIntersectLine, nArrayIndexOfVertex, pt3dArrayIntersect, iDwIntersectMoveToVertex) )
					{
						strSql.Format(_T("insert into sxt_ent(sxt_ent_handle,  sxt_ent_block, sxt_ent_x, sxt_ent_y, sxt_ent_movetovertex, sxt_ent_name, sxt_ent_type,  sxt_ent_online_handle,  sxt_ent_online_vertex,  sxt_ent_online_intersectx,  sxt_ent_online_intersecty,  sxt_ent_fwline_handle,  sxt_ent_fwline_vertex,  sxt_ent_x1, sxt_ent_y1, sxt_ent_online_intersectx1,  sxt_ent_online_intersecty1)  \
										 values(%d, '%s', %.10f, %.10f, %d, '%s', %d, %d, %d, %.10f, %.10f, %d, %d, %.10f, %.10f, %.10f, %.10f) "), nHandle, strBlkName,pt_3d_position.x, pt_3d_position.y, iDwIntersectMoveToVertex, strName, 0, nArrayHandleIntersectLine.GetAt(0), nArrayIndexOfVertex.GetAt(0), pt3dArrayIntersect.at(0).x, pt3dArrayIntersect.at(0).y, nArrayHandleIntersectLine.GetAt(1), nArrayIndexOfVertex.GetAt(1), pt_3d_position.x, pt_3d_position.y, pt3dArrayIntersect.at(0).x, pt3dArrayIntersect.at(0).y) ;
						m_AdoDb.Execute(strSql) ;
					}					
				}
				else if (strBlkType==_T("XHJ")||strBlkType==_T("JYJ")) //信号机要么在某段线上，要么在2段线结合处
				{
					if (nHandle==21306)
					{
						int iiii = 0 ;
					}
					int iEntType = 100 ; //xhj
					if (strBlkType==_T("JYJ"))
					{
						iEntType = 200 ;
						if (strBlkName.Left(6)==_T("JYJ_JX"))
						{
							iEntType=250 ; //为实现极性切割增加的绝缘节
						}
					}

					int iDistToXhl = 0 ;
					CString strDistToXhl ;
					if(m_mysys.GetAttValue(pRef, _T("DIST"), strDistToXhl) )
					{
						strDistToXhl.TrimLeft(_T("(")) ;
						strDistToXhl.TrimRight(_T(")")) ;
						iDistToXhl = _ttoi(strDistToXhl) ;
					}

					m_mysys.GetAttValue(pRef, _T("XHJNAME"), strName) ;

					CLongArray nArrayHandleIntersect, nArrayIndexOfVertex ;
					AcGePoint3dArray pt3dArrayIntersect ;
					int iIntersectNum = GetXhjJyjConLine(pRef, nArrayHandleIntersect, nArrayIndexOfVertex, pt3dArrayIntersect) ;
					int iHandleOnLine = 0, iHandleLeft = 0, iHandleRight = 0, iVertexOnLine = 0, iVertexLeft = 0, iVertexRight = 0 ;
					if (iIntersectNum>0) //特别注意此类条件，不然后面的  pt3dArrayIntersect.at(0)极易崩溃（因为数值没有存储值）
					{
						if (iIntersectNum==2)
						{
							iHandleOnLine = nArrayHandleIntersect.GetAt(0) ;
							iVertexOnLine = nArrayIndexOfVertex.GetAt(0) ;
						}
						else if (iIntersectNum<10)
						{
							if (iIntersectNum==1)
							{
								iHandleLeft = nArrayHandleIntersect.GetAt(0) ;
								iVertexLeft = nArrayIndexOfVertex.GetAt(0) ;
							}
							else if (iIntersectNum==5)
							{
								iHandleRight = nArrayHandleIntersect.GetAt(0) ;
								iVertexRight = nArrayIndexOfVertex.GetAt(0) ;
							}
						}
						else
						{
							if (iIntersectNum/10==1||iIntersectNum%10==1)
							{
								iHandleLeft = nArrayHandleIntersect.GetAt((iIntersectNum/10==1)?1:0) ;
								iVertexLeft = nArrayIndexOfVertex.GetAt((iIntersectNum/10==1)?1:0) ;
							}
							if (iIntersectNum/10==5||iIntersectNum%10==5)
							{
								iHandleRight = nArrayHandleIntersect.GetAt((iIntersectNum/10==5)?1:0) ;
								iVertexRight = nArrayIndexOfVertex.GetAt((iIntersectNum/10==5)?1:0) ;
							}
						}

						strSql.Format(_T("insert into sxt_ent(sxt_ent_handle,  sxt_ent_block, sxt_ent_x, sxt_ent_y, sxt_ent_name, sxt_ent_type,  sxt_ent_online_handle,  sxt_ent_online_vertex,  sxt_ent_online_intersectx,  sxt_ent_online_intersecty,  sxt_ent_conline_left, sxt_ent_conline_left_vertex, sxt_ent_conline_right, sxt_ent_conline_right_vertex, sxt_ent_x1, sxt_ent_y1, sxt_ent_online_intersectx1,  sxt_ent_online_intersecty1, sxt_ent_toxhldist)  \
										 values(%d, '%s', %.10f, %.10f, '%s', %d, %d, %d, %.10f, %.10f, %d, %d, %d, %d, %.10f, %.10f, %.10f, %.10f, %d) "), nHandle, strBlkName, pt_3d_position.x, pt_3d_position.y, strName, iEntType, iHandleOnLine, iVertexOnLine, pt3dArrayIntersect.at(0).x, pt3dArrayIntersect.at(0).y,iHandleLeft, iVertexLeft, iHandleRight, iVertexRight, pt_3d_position.x, pt_3d_position.y, pt3dArrayIntersect.at(0).x, pt3dArrayIntersect.at(0).y, iDistToXhl) ;
						m_AdoDb.Execute(strSql) ;

					}
				}
			}
			pEnt->close() ;
		}	
	}
	//再遍历一遍，将轨道线读入
	for(int i=0; i<nLen; i++)
	{
		AcDbEntity * pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				long nHandleLine =nArrayHandle.GetAt(i) ;
				strSql.Format(_T("select * from sxt_ent where sxt_ent_online_handle=%d or sxt_ent_fwline_handle=%d or sxt_ent_conline_left=%d or sxt_ent_conline_right=%d"), nHandleLine, nHandleLine, nHandleLine, nHandleLine ) ;
				int iRetSum = m_RecordSet.GetRecordCount(strSql) ;
				if (iRetSum>0)
				{
					AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;	
					int iNumVert = pPolyLine->numVerts() ;
					for (int i=0; i<iNumVert; i++)
					{
						AcGePoint3d pt_3d_vertex(0,0,0) ;
						pPolyLine->getPointAt(i, pt_3d_vertex) ;
						strSql.Format(_T("insert into sxt_line(sxt_line_handle, sxt_line_vertex,  sxt_line_vertex_x, sxt_line_vertex_y, sxt_line_vertex1,  sxt_line_vertex_x1, sxt_line_vertex_y1 )  \
										 values(%d, %d, %.10f, %.10f, %d, %.10f, %.10f) "), nHandleLine, i,  pt_3d_vertex.x, pt_3d_vertex.y, i,  pt_3d_vertex.x, pt_3d_vertex.y) ;
						m_AdoDb.Execute(strSql) ;
					}
				}
			}
			pEnt->close() ;
		}
	}

	//再遍历一遍，将各设备距离中心的距离读入数据库
	vector<vector<vector<CString> > > vecToXhl ;
	this->GetDistToXhl(pt_3d_toxhlheader, vecToXhl) ;
	for (int i=0; i<vecToXhl.size(); i++)
	{
		for (int j=0; j<vecToXhl[i].size(); j++)
		{
			strSql.Format(_T("update sxt_ent set sxt_ent_toxhldist=%d where sxt_ent_name='%s'"), _ttoi(vecToXhl[i][j][2]), vecToXhl[i][j][1]) ;
			m_AdoDb.Execute(strSql) ;
		}
	}

	//计算绝缘节距信号楼距离（要么和信号机并置，要么显式指定了其距信号楼距离）,更新信号机、道岔距信号楼距离（左侧为负数）
	this->UpdateToXhlDist() ;
	//设置实体在上行还是下行，决定ZZJ、盒子之类的朝向
	this->SetEntUpOrDown() ;
	//标出交叉渡线及绝缘节、端头线
	this->FindCrossLineAndDtLine() ;	    
}

// 循环查询轨道线连接的信号机及道岔，已作为计算轨道名称的依据
void CZnSxt::GetGuiDaoInfo(UINT nLineHandle, CLongArray& nArrayHandleXHJ, CLongArray& nArrayHandleDC, CLongArray& nArrayLineDoneCur, CLongArray& nArrayLineCurType)
{
	nArrayLineDoneCur.Add(nLineHandle) ;
	try
	{
		CString strSql ;
		strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d"), nLineHandle) ;
		long nLineType = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_line_type"), nLineType) ;
		nArrayLineCurType.Add(nLineType) ;

		CLongArray nArrayHandleXHJTmp, nArrayHandleDCTmp ;
		strSql.Format(_T("select * from sxt_ent where (sxt_ent_conline_left=%d or sxt_ent_conline_right=%d or sxt_ent_online_handle=%d) and sxt_ent_type>99 and sxt_ent_type<200 and left(sxt_ent_block,6)<>'XHJ_FS'"), nLineHandle, nLineHandle, nLineHandle ) ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nArrayHandleXHJTmp) ;
		nArrayHandleXHJ.Append(nArrayHandleXHJTmp) ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_online_handle=%d and sxt_ent_type>-1 and sxt_ent_type<100"), nLineHandle ) ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nArrayHandleDCTmp) ;
		nArrayHandleDC.Append(nArrayHandleDCTmp) ;

		for (int i=0; i<nArrayHandleDCTmp.GetCount(); i++)
		{
			UINT nHandleDc = nArrayHandleDCTmp.GetAt(i) ;
			strSql.Format(_T("select sxt_ent_fwline_handle from sxt_ent where sxt_ent_handle=%d"), nHandleDc) ;
			long nHandleFwLine = 0 ;
			m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_fwline_handle"), nHandleFwLine) ;
			if (nHandleFwLine>0)
			{
				this->GetGuiDaoInfo(nHandleFwLine, nArrayHandleXHJ, nArrayHandleDC, nArrayLineDoneCur, nArrayLineCurType) ;
			}
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in GetGuiDaoInfo"));
	}
}

// 取得轨道电路中最大最小的道岔号
void CZnSxt::GetMaxMinDC(const CLongArray& nArrayHandleDC,  int& iMaxDC, int& iMinDC)
{
	CString strDCHandles ;
	CBlkUtility::ConvertArrayToString(nArrayHandleDC, _T(","),  strDCHandles) ;
	CString strSql ;
	CStringArray strArrayDCName ;
	strSql.Format(_T("select sxt_ent_name from sxt_ent where sxt_ent_handle in (%s)"), strDCHandles) ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strArrayDCName) ;
	int iNumDC = strArrayDCName.GetCount() ;
	for (int i=0; i<iNumDC; i++)
	{
		int iDCNum = 0 ;
		iDCNum = _ttoi(strArrayDCName.GetAt(i)) ;
		if (i==0)
		{
			iMaxDC = iMinDC = iDCNum ;
		}
		else
		{
			if (iDCNum>iMaxDC)
			{
				iMaxDC = iDCNum ;
			}
			else if (iDCNum<iMinDC)
			{
				iMinDC = iDCNum ;
			}
		}
	}
}

long CZnSxt::GetMaxGuiDaoId(void)
{
	long nRet = 0 ;

	AcDbBlockTable * pBlkTbl = NULL ;
	this->m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;
		Acad::ErrorStatus es = pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead) ;

		if (Acad::eOk == es)
		{
			int nId = 0 ;
			if (m_mysys.GetXdata(pEnt, _T("GUIDAOID"), nId))
			{
				if (nId>nRet)
				{
					nRet = nId ;
				}
			}				

			pEnt->close() ;
		}
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return nRet ;
}

//显示平面图信息（比如轨道电路名称，送受电端）,iType=0,显示所有
void CZnSxt::ShowInfo(int iType)
{
	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase() ;
	CUserSystem mysys ;
	mysys.m_pDb = pDb ;
	BOOL bShowHide =  TRUE ; //对某个特性的隐藏FALSE或显示TRUE记录在扩展字典里面
	AcDbObjectId objIdLayerOld  ; 

	//显示/隐藏各区段的送受电设置
	if (iType==1)
	{
		objIdLayerOld = pDb->clayer() ;
		mysys.Set_Layer(_T(""), _T("PMT_SHOWINFO_FSD")) ;

		int iShowHideTmp = 0 ;
		if(mysys.ReadFromRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_FSD"), iShowHideTmp))
		{
			bShowHide = (BOOL)iShowHideTmp ;
		}
		mysys.WriteToRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_FSD"), (int)(!bShowHide)) ;

		this->ClearShowInfo(_T("PMT_SHOWINFO_FSD")) ;//先清理当前图纸中显示的相关信息（不管隐藏还是显示，这一步都要）

		if (bShowHide) 
		{
			AcDbBlockTable * pBlkTbl = NULL ;
			pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

			AcDbBlockTableRecord * pBlkTblRec = NULL ;
			pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
			pBlkTbl->close() ;
			AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
			pBlkTblRec->newIterator( pBlkTblRecIter );
			pBlkTblRec->close();

			AcDbEntity * pEnt = NULL ;
			for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
			{
				//遍历并打开实体 ;
				Acad::ErrorStatus es = pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead) ;
				if (Acad::eOk == es)
				{
					this->ShowGdFS(pEnt) ;
					pEnt->close() ;
				}
			}
			delete pBlkTblRecIter ;
			pBlkTblRecIter = NULL ;
		}
	}
	else 	if (iType==2) 	//显示/隐藏各多段线handle
	{
		objIdLayerOld = pDb->clayer() ;
		mysys.Set_Layer(_T(""), _T("PMT_SHOWINFO_LINEHANDLE")) ;

		int iShowHideTmp = 0 ;
		if(mysys.ReadFromRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_LINEHANDLE"), iShowHideTmp))
		{
			bShowHide = (BOOL)iShowHideTmp ;
		}
		mysys.WriteToRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_LINEHANDLE"), (int)(!bShowHide)) ;

		this->ClearShowInfo(_T("PMT_SHOWINFO_LINEHANDLE")) ;//先清理当前图纸中显示的相关信息（不管隐藏还是显示，这一步都要）
		
		if (bShowHide)
		{
			AcDbBlockTable * pBlkTbl = NULL ;
			pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

			AcDbBlockTableRecord * pBlkTblRec = NULL ;
			pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
			pBlkTbl->close() ;
			AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
			pBlkTblRec->newIterator( pBlkTblRecIter );
			pBlkTblRec->close();

			AcDbEntity * pEnt = NULL ;
			for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
			{
				//遍历并打开实体 ;
				Acad::ErrorStatus es = pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead) ;
				if (Acad::eOk == es)
				{
					if (pEnt->isKindOf(AcDbPolyline::desc()))
					{
						AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;
						AcGePoint3d pt_3d_left(0,0,0), pt_3d_right(0,0,0) ;
						pPolyLine->getStartPoint(pt_3d_left) ;

						CString strHandle ;
						long nHandle = m_mysys.GetHandle(pEnt) ;
						strHandle.Format(_T("%d"), nHandle) ;
						pt_3d_left.x+=1 ;
						pt_3d_left.y+=1 ;
						AcDbObjectId objIdGuiDaoName ;
						mysys.AddText(objIdGuiDaoName, strHandle, pt_3d_left, pt_3d_left, AcDb::kTextLeft, AcDb::kTextBase, 2.5, 0.8, 0, 1) ;
						AcDbEntity* pEntGuiDaoName = NULL ;
						if (mysys.OpenAcDbEntity(objIdGuiDaoName, pEntGuiDaoName))
						{
							mysys.SetXdata(pEntGuiDaoName, _T("PMT_SHOWINFO_LINEHANDLE"), 1) ;
							pEntGuiDaoName->close() ;
						}
					}

					pEnt->close() ;
				}
			}
			delete pBlkTblRecIter ;
			pBlkTblRecIter = NULL ;
		}
	}
	else 	if (iType==3) 	//显示/隐藏各轨道区段名称
	{
		objIdLayerOld = pDb->clayer() ;
		mysys.Set_Layer(_T(""), _T("PMT_SHOWINFO_GUIDAONAME")) ;

		int iShowHideTmp = 0 ;
		if(mysys.ReadFromRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_GUIDAONAME"), iShowHideTmp))
		{
			bShowHide = (BOOL)iShowHideTmp ;
		}
		mysys.WriteToRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_GUIDAONAME"), (int)(!bShowHide)) ;

		this->ClearShowInfo(_T("PMT_SHOWINFO_GUIDAONAME")) ;//先清理当前图纸中显示的相关信息（不管隐藏还是显示，这一步都要）

		if (bShowHide)
		{
			AcDbBlockTable * pBlkTbl = NULL ;
			pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

			AcDbBlockTableRecord * pBlkTblRec = NULL ;
			pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
			pBlkTbl->close() ;
			AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
			pBlkTblRec->newIterator( pBlkTblRecIter );
			pBlkTblRec->close();

			AcDbEntity * pEnt = NULL ;
			for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
			{
				//遍历并打开实体 ;
				Acad::ErrorStatus es = pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead) ;
				if (Acad::eOk == es)
				{
					this->ShowGdName(pEnt) ;
					pEnt->close() ;
				}
			}
			delete pBlkTblRecIter ;
			pBlkTblRecIter = NULL ;
		}
	}

	pDb->setClayer(objIdLayerOld) ;
}

// 增加直弯股切割绝缘节
int CZnSxt::AddCutJYJ(void)
{
	CLongArray nArrayHandleDc ;
	int iNum = this->GetAllDc(nArrayHandleDc) ;
	for (int i=0; i<iNum; i++)
	{
		long nHandle = nArrayHandleDc.GetAt(i) ;
		this->AddCutJYJ(nHandle) ;
	}
	return 0;
}

// 清除属于道岔（nHandleBelong）的切割绝缘节
int CZnSxt::DeleteCutJYJ(long nHandleBelong)
{
	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;	
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;

				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;				
				if (strBlkName.Left(3)==_T("JYJ"))
				{
					long nHandleBelongTmp= 0, nHandleCloneFrom = 0 ;
					m_mysys.GetXdata(pEnt, _T("BELONGDC"), nHandleBelongTmp) ;		
					m_mysys.GetXdata(pEnt, _T("CloneFrom"), nHandleCloneFrom) ;
					if (nHandleBelongTmp==nHandleBelong&&nHandleCloneFrom==0)
					{
						pEnt->upgradeOpen() ;
						pEnt->erase() ;
					}
				}
			}

			pEnt->close() ;
		}	
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;
	return 0;
}

// 给指定道岔添加切割绝缘节
int CZnSxt::AddCutJYJ(long nHandleDC)
{
	//先清除该DC的关联切割绝缘节
	this->DeleteCutJYJ(nHandleDC) ;

// 	if (nHandleDC==137092)
// 	{
// 		int iii = 0 ;
// 	}

	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandleDC, pEnt, AcDb::kForRead ))
	{
		AcDbBlockReference* pRef = AcDbBlockReference ::cast(pEnt) ;

		CLongArray nArrayHandleIntersectLine, nArrayIndexOfVertex ;
		int iDwIntersectMoveToVertex = -1 ;
		AcGePoint3dArray pt3dArrayIntersect ;
		if(this->GetDcConLineDFW(pRef, nArrayHandleIntersectLine, nArrayIndexOfVertex, pt3dArrayIntersect, iDwIntersectMoveToVertex) )
		{
			int iCutJYJ = 0 ; //默认弯股切割
			m_mysys.GetXdata(pEnt, _T("CUTJYJPOS"), iCutJYJ) ;
			AcGePoint3d pt_3d_cutjyj(0,0,0) ; //切割绝缘节的基点
			double dAngle = 0 ;

			if ((iCutJYJ==0&&iDwIntersectMoveToVertex==-1)||iCutJYJ==1&&iDwIntersectMoveToVertex!=-1) //弯股切割时
			{							
				long nHandleFwLine = nArrayHandleIntersectLine.GetAt(1) ;
				int iFwLineVertex = nArrayIndexOfVertex.GetAt(1) ;
				AcDbEntity* pEntFwLine = NULL ;
				if(m_mysys.OpenAcDbEntity(nHandleFwLine, pEntFwLine, AcDb::kForRead))
				{
					AcDbPolyline* pPolyLineFw = AcDbPolyline::cast(pEntFwLine) ;
					AcGePoint3d pt_3d_start(0,0,0), pt_3d_end(0,0,0) ;
					pPolyLineFw->getPointAt(iFwLineVertex, pt_3d_start) ;
					pPolyLineFw->getPointAt(iFwLineVertex==0?(iFwLineVertex+1):(iFwLineVertex-1), pt_3d_end) ;

					AcDbLine *pLineTmp = new AcDbLine(pt_3d_start, pt_3d_end) ;
					pLineTmp->getPointAtDist(1.5, pt_3d_cutjyj) ;
					AcGeVector2d vecTmp(pt_3d_end.x-pt_3d_start.x, pt_3d_end.y-pt_3d_start.y) ;
					dAngle = vecTmp.angle() ;				
					delete pLineTmp ;
					pLineTmp = NULL ;

					pEntFwLine->close() ;
				}
			}
			else //直股切割或者弯股切割但是定位在弯股时
			{
				long nHandleDwLine = nArrayHandleIntersectLine.GetAt(0) ;
				int iDwLineVertex = nArrayIndexOfVertex.GetAt(0) ;
				AcDbEntity* pEntDwLine = NULL ;
				if(m_mysys.OpenAcDbEntity(nHandleDwLine, pEntDwLine, AcDb::kForRead))
				{
					AcDbPolyline* pPolyLineDw = AcDbPolyline::cast(pEntDwLine) ;
					AcGePoint3d pt_3d_start(0,0,0), pt_3d_end(0,0,0) ;
					pt_3d_start = pt3dArrayIntersect.at(0) ;
					if (nArrayIndexOfVertex.GetAt(1)==0)
					{
						pPolyLineDw->getPointAt(nArrayIndexOfVertex.GetAt(0)+1, pt_3d_end) ;
					}
					else
					{
						pPolyLineDw->getPointAt(nArrayIndexOfVertex.GetAt(0), pt_3d_end) ;
					}

					AcDbLine *pLineTmp = new AcDbLine(pt_3d_start, pt_3d_end) ;
					pLineTmp->getPointAtDist(iDwIntersectMoveToVertex==1?4:3, pt_3d_cutjyj) ;
					AcGeVector2d vecTmp(pt_3d_end.x-pt_3d_start.x, pt_3d_end.y-pt_3d_start.y) ;
					dAngle = vecTmp.angle() ;		
					delete pLineTmp ;
					pLineTmp = NULL ;

					pEntDwLine->close() ;
				}
			}

			AcDbBlockReference* pRefTmp = NULL ;
			if (m_mysys.InsertBlock(pRefTmp, _T("JYJ_JX_0"), pt_3d_cutjyj))
			{
				pRefTmp->setRotation(dAngle) ;
				m_mysys.SetXdata(pRefTmp, _T("BELONGDC"), nHandleDC) ;
				pRefTmp->close() ;
				pRefTmp = NULL ;									
			}	
		}
		pEnt->close() ;
	}

	return 0;
}

// 在sxt_line表中增加切割绝缘节对应的节点 db
int CZnSxt::AddVertexOfCutJYJ(void)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type=250 order by sxt_ent_x")) ;

		if (cSet.Open(strSql))
		{
			while (!cSet.IsEOF())
			{
				int iEntType = -1 ;
				cSet.GetFieldValue(_T("sxt_ent_type"), iEntType) ;
				long nHandleEnt = 0, nHandleLine = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandleEnt) ;
				cSet.GetFieldValue(_T("sxt_ent_online_handle"), nHandleLine) ;

				double dIntersectX = 0.000, dIntersectY = 0.000, dIntersectX1 = 0.000, dIntersectY1 = 0.000 ;
				cSet.GetFieldValue(_T("sxt_ent_online_intersectx"), dIntersectX) ;
				cSet.GetFieldValue(_T("sxt_ent_online_intersecty"), dIntersectY) ;
				cSet.GetFieldValue(_T("sxt_ent_online_intersectx1"), dIntersectX1) ;
				cSet.GetFieldValue(_T("sxt_ent_online_intersecty1"), dIntersectY1) ;
				//在dIntersectX处是否已有顶点
				strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and abs(sxt_line_vertex_x1-%.10f)<0.001"), nHandleLine, dIntersectX1) ;
				int iVertexSame = -1 ;
				m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex"), iVertexSame) ;	
				if (iVertexSame!=-1)
				{
					strSql.Format(_T("update sxt_line set sxt_line_vertex_type=11, sxt_line_vertex_handle=%d where sxt_line_handle=%d and sxt_line_vertex=%d"), nHandleEnt, nHandleLine, iVertexSame) ;
					m_AdoDb.Execute(strSql) ;
				}
				else
				{
					strSql.Format(_T("update sxt_line set sxt_line_vertex1=sxt_line_vertex1+1 where sxt_line_vertex_x1>%.10f and sxt_line_handle=%d"), dIntersectX1, nHandleLine) ;
					m_AdoDb.Execute(strSql) ;
					strSql.Format(_T("select top 1 * from sxt_line where sxt_line_vertex_x1<%.10f and sxt_line_handle=%d order by sxt_line_vertex_x1 desc"), dIntersectX1, nHandleLine) ;
					int iVertexTmp = 0 ;
					m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex1"), iVertexTmp) ;					
					strSql.Format(_T("insert into sxt_line(sxt_line_handle, sxt_line_vertex_type, sxt_line_vertex1, sxt_line_vertex_x, sxt_line_vertex_y, sxt_line_vertex_x1, sxt_line_vertex_y1, sxt_line_vertex_handle) values(%d, %d, %d, %.10f, %.10f,  %.10f, %.10f, %d)"), nHandleLine, iEntType==0?2:1, iVertexTmp+1, dIntersectX, dIntersectY, dIntersectX1, dIntersectY1, nHandleEnt) ;
					m_AdoDb.Execute(strSql) ;
				}			

				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in AddVertexOfCutJYJ"));
	}

	return 0;
}
//static int iii = 0 ;
// 从一条特定的线开始，递归推出后续连接的所有极性（一条多段线上可能因为切割绝缘节有多个极性）,bForward=true：向右推进，反之向左, iStartVertex，从哪个顶点开始推导，iTestMode=0，仅提示极性冲突段，1，显示极性线条，2，显示动画
void CZnSxt::FillPolar2(long nHandleLine, int iPolar, int iStartVertex, bool bForward, int iTestMode)
{
	if (m_bFillPolarErro==true)
	{
		return ;
	}
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;

		//begin 端头线，不设置极性
		strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d"), nHandleLine) ;
		long nLineType = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_line_type"), nLineType) ;
		if (nLineType==2) 
		{
			return ;
		}
		//end 端头线，不设置极性

		long nHandleLeftJyj = 0, nHandleRightJyj = 0 ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type>=200 and sxt_ent_type<220 and sxt_ent_conline_right=%d"), nHandleLine) ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleLeftJyj) ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type>=200 and sxt_ent_type<220 and sxt_ent_conline_left=%d"), nHandleLine) ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleRightJyj) ;        
		int nHandleEndConJyj = (bForward?nHandleRightJyj:nHandleLeftJyj) ; //线末端连接的绝缘节handle

		//检测是否已设置过极性
		strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex1=%d "), nHandleLine, bForward?iStartVertex:iStartVertex-1) ;
// 		if (bForward==false) //反向时为了保证设置点的极性和正向一致，每次错一位（比如一个线段2个顶点0,1，始终只以0点的极性作为该线段的极性）
// 		{
// 			strSql.Format(_T("select top 1 * from (select top 2 * from sxt_line where sxt_line_handle=%d order by sxt_line_vertex1 desc) order by sxt_line_vertex1 asc"), nHandleLine) ;
// 		}
		int iPolarToCheck = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_line_polar"), iPolarToCheck) ;

		if (iTestMode==2)
		{
			//m_mysys.SetEntColorIndex(nHandleLine, 1) ;
			m_mysys.FlashEnt(nHandleLine, 500, 50) ;
 			//m_mysys.SetEntColorIndex(nHandleLine, 3) ;
		}

		if (iPolarToCheck==0)
		{
			//遍历所有vertex，遇到切割绝缘节就变极性（sxt_line_vertex_type=1）
			strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex1%s%d order by sxt_line_vertex1 %s"), nHandleLine, bForward?_T(">="):_T("<="), iStartVertex, bForward?_T("asc"):_T("desc")) ;
			if (cSet.Open(strSql))
			{
				int iNumVertex = cSet.GetRecordCount() ;
				int iPolarTmp = iPolar ;
				int iIndexVertex = 0 ;
				while (!cSet.IsEOF())
				{
					int iVertex1 = 0 ;
					cSet.GetFieldValue(_T("sxt_line_vertex1"), iVertex1) ;
					//遇到切割绝缘节，极性要变
					int iVertexType = 0 ;
					cSet.GetFieldValue(_T("sxt_line_vertex_type"), iVertexType) ;
					if ((iIndexVertex>0&&iVertexType==1)|| (iIndexVertex==(iNumVertex-1)&&nHandleEndConJyj>0))
					{
						iPolarTmp = -iPolarTmp ;
					}
					//正向时，最后一个点不设置极性，反向时，第一个不设置极性，所有线段极性取其起点极性
					if (bForward)
					{
						if (iIndexVertex<(iNumVertex-1))
						{
							strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex1=%d"), nHandleLine, iVertex1) ;
							m_RecordSet.SetRecordValue(strSql, _T("sxt_line_polar"),  iPolarTmp) ;

							if (iTestMode>0)
							{
								AcDbObjectId objIdLayer = m_mysys.m_pDb->clayer() ;
								m_mysys.Set_Layer(_T(""), _T("CRSCD_SJY_XTS_PMT_TESTJX")) ;
								double dX1 = 0, dY1 = 0, dX2 = 0, dY2 = 0 ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x"), dX1) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y"), dY1) ;
								strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex1=%d"), nHandleLine, iVertex1+1) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x"), dX2) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y"), dY2) ;
								m_mysys.AddAcDbPolyLine(AcGePoint3d(dX1, dY1, 0), AcGePoint3d(dX2, dY2, 0), 0.8, 0, iPolarTmp==1?3:1) ;
								m_mysys.m_pDb->setClayer(objIdLayer) ;
							}
						}
					}
					else
					{
						if (iVertex1>0)
						{
							strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex1=%d"), nHandleLine, iVertex1-1) ;
							m_RecordSet.SetRecordValue(strSql, _T("sxt_line_polar"),  iPolarTmp) ;

							if (iTestMode>0)
							{
								AcDbObjectId objIdLayer = m_mysys.m_pDb->clayer() ;
								m_mysys.Set_Layer(_T(""), _T("CRSCD_SJY_XTS_PMT_TESTJX")) ;
								double dX1 = 0, dY1 = 0, dX2 = 0, dY2 = 0 ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x"), dX1) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y"), dY1) ;
								strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d and sxt_line_vertex1=%d"), nHandleLine, iVertex1) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x"), dX2) ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y"), dY2) ;
								m_mysys.AddAcDbPolyLine(AcGePoint3d(dX1, dY1, 0), AcGePoint3d(dX2, dY2, 0), 0.8, 0, iPolarTmp==1?3:1) ;
								m_mysys.m_pDb->setClayer(objIdLayer) ;
							}
						}
					}

					if (iIndexVertex<(iNumVertex-1))
					{
						//begin 查询此段线上有否道岔
						double dVertexX1 = 0, dVertexX2 = 0, dVertexY1 = 0, dVertexY2 = 0 ;
						cSet.GetFieldValue(_T("sxt_line_vertex_x1"), dVertexX1) ;
						cSet.GetFieldValue(_T("sxt_line_vertex_y1"), dVertexY1) ;
						cSet.MoveNext() ;

						cSet.GetFieldValue(_T("sxt_line_vertex_x1"), dVertexX2) ;
						cSet.GetFieldValue(_T("sxt_line_vertex_y1"), dVertexY2) ;
						strSql.Format(_T("select * from sxt_ent where sxt_ent_type=0 and sxt_ent_online_handle=%d and sxt_ent_x1>%.3f and sxt_ent_x1<%.3f and sxt_ent_y1>%.3f and sxt_ent_y1<%.3f order by sxt_ent_x1 %s"), nHandleLine, min(dVertexX1,dVertexX2), max(dVertexX1,dVertexX2), min(dVertexY1,dVertexY2)-1.5, max(dVertexY1,dVertexY2)+1.5,  bForward?_T("asc"):_T("desc")) ;
						CLongArray nArrayHandleFwLine  ;
						CLongArray iArrayVertexFwLine ;
						m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_fwline_handle"), nArrayHandleFwLine) ;
						m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_fwline_vertex"), iArrayVertexFwLine) ;	
						int iNumFwLine = nArrayHandleFwLine.GetCount() ;
						for (int i=0; i<iNumFwLine; i++)
						{
							int iPolarTmp2 = iPolarTmp ;
							int nHandleFwLine = nArrayHandleFwLine.GetAt(i) ;
							if (nHandleFwLine!=0&&nHandleFwLine!=nHandleLine)
							{
								int iVertexFwLine = iArrayVertexFwLine.GetAt(i) ;
// 								if (iVertexFwLine==0) //只处理顺序向右的
// 								{
									strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d"), nHandleFwLine) ;
									int iNumVertexFwLine = m_RecordSet.GetRecordCount(strSql) ;
									this->FillPolar2(nHandleFwLine, iPolarTmp2, iVertexFwLine==0?0:(iNumVertexFwLine-1), iVertexFwLine==0?true:false, iTestMode) ;
// 								}
							}
						}

						cSet.MovePrevious() ;	
						//end 查询此段线上有否道岔
					}

					//最后一个顶点
					if (iIndexVertex==(iNumVertex-1))
					{
						long nHandleNextLine = 0 ;
						strSql.Format(_T("select * from sxt_ent where sxt_ent_type>=200 and sxt_ent_type<220 and sxt_ent_handle=%d"), nHandleEndConJyj) ;
						CString strFieldName = (bForward?_T("sxt_ent_conline_right"):_T("sxt_ent_conline_left")) ;
						m_RecordSet.GetRecordValue(strSql, strFieldName, nHandleNextLine) ;
						if (nHandleNextLine>0)
						{
							strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d"), nHandleNextLine) ;
							int iNumVertexNextLine = m_RecordSet.GetRecordCount(strSql) ;
							this->FillPolar2(nHandleNextLine, iPolarTmp,  bForward?0:(iNumVertexNextLine-1), bForward, iTestMode) ;
						}
						else  //线搜寻前进方向的末端连接的是道岔fw
						{
							long nHandleConDc = 0 ;
							int iVertexToDc = 0 ;
							strSql.Format(_T("select * from sxt_ent where sxt_ent_type=0 and sxt_ent_fwline_handle=%d"), nHandleLine) ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleConDc) ;       
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_fwline_vertex"), iVertexToDc) ;  
							double dDcX = 0 ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_x1"), dDcX) ; 
							if (nHandleConDc>0&&((bForward&&iVertexToDc>0)||(!bForward&&iVertexToDc==0)))
							{
								long nHandleOnLine = 0 ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_handle"), nHandleOnLine) ; 
								strSql.Format(_T("select top 1 * from sxt_line where sxt_line_handle=%d and sxt_line_vertex_x1%s%.3f order by sxt_line_vertex_x1 %s"), nHandleOnLine, bForward?_T("<"):_T(">"), dDcX, bForward?_T("desc"):_T("asc")) ;
								int iStartVertexTmp = 0 ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex1"), iStartVertexTmp) ;
								//该顶点是纯顶点还是切割顶点，切割顶点是后加在数据库中的，实际线上无此顶点，在数据库中sxt_line_vertex=-1,纯顶点不变换极性
								int iStartVertexOrinal = 0 ;
								m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex"), iStartVertexOrinal) ;
								if (nHandleOnLine>0)
								{
									this->FillPolar2(nHandleOnLine, iPolarTmp, iStartVertexTmp, bForward, iTestMode) ;
									//另外方向
									if (bForward)
									{
										if (iStartVertexTmp==0)//反向下一个区段（nHandleOnline左边相邻的区段）
										{
											strSql.Format(_T("select * from sxt_ent where sxt_ent_conline_right=%d"), nHandleOnLine) ;
											long nHandleLeftLine = 0 ;
											m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_conline_left"), nHandleLeftLine) ;
											if (nHandleLeftLine>0)
											{
												strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d"), nHandleLeftLine) ;
												int iNumVertexOfLeftLine = m_RecordSet.GetRecordCount(strSql) ;
												this->FillPolar2(nHandleLeftLine, -iPolarTmp, iNumVertexOfLeftLine-1, !bForward, iTestMode) ;
											}
										}
										else //还是本轨道多段线（nHandleOnLine）//这里要注意，虽然还是本多段线，但是这个顶点可能是切割点也可能是纯顶点，纯顶点不要变极性
										{											
											this->FillPolar2(nHandleOnLine, (iStartVertexOrinal==-1?-iPolarTmp:iPolarTmp), iStartVertexTmp, !bForward, iTestMode) ;
										}
									}
									else
									{
										strSql.Format(_T("select * from sxt_line where sxt_line_handle=%d"), nHandleOnLine) ;
										int iNumOfVertexOnLine =m_RecordSet.GetRecordCount(strSql) ;
										if (iStartVertexTmp==(iNumOfVertexOnLine-1))//正向下一个区段（nHandleOnline右边相邻的区段）
										{
											strSql.Format(_T("select * from sxt_ent where sxt_ent_conline_left=%d"), nHandleOnLine) ;
											long nHandleRightLine = 0 ;
											m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_conline_right"), nHandleRightLine) ;
											if (nHandleRightLine>0)
											{										
												this->FillPolar2(nHandleRightLine, -iPolarTmp, 0, !bForward, iTestMode) ;
											}
										}
										else //还是本轨道多段线（nHandleOnLine）
										{
											this->FillPolar2(nHandleOnLine, (iStartVertexOrinal==-1?-iPolarTmp:iPolarTmp), iStartVertexTmp, !bForward, iTestMode) ;
										}
									}
								}
							}
						}
					}

					iIndexVertex++ ;

					cSet.MoveNext() ;
				}
				cSet.Close() ;
			}
		}
		else
		{
			if (iPolarToCheck!=iPolar)
			{
				double dX = 0, dY = 0 ;
				strSql.Format(_T("select top 1 * from sxt_line where sxt_line_handle=%d order by sxt_line_vertex1 %s"), nHandleLine, bForward?_T("asc"):_T("desc")) ;
				m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_x"), dX) ;
				m_RecordSet.GetRecordValue(strSql, _T("sxt_line_vertex_y"), dY) ;

// 				AcGePoint3d pt_3d_cen ; //(340.47,-5.53, 0)
// 				double dHeight = 0 ;
// 				double dWidth  = 0 ;
				//if (bTestMode)
				{
// 					AcGePoint2d pt_2d_cen = m_mysys.GetViewCenterPoint() ;					
// 					m_mysys.ConvertPoint(pt_2d_cen, pt_3d_cen) ;
// 					dHeight =m_mysys.GetViewHeight() ;
// 					dWidth = m_mysys.GetViewWidth() ;
					m_mysys.ZoomView(AcGePoint3d(dX, dY,0), 200,200) ;
					m_mysys.SetEntColorIndex(nHandleLine, 2) ;
					m_mysys.FlashEnt(nHandleLine, 1000) ;
				}
				CString strMsg ;
				strMsg.Format(_T("进行到handle=%d的线(其坐标为%.3f, %.3f)时极性冲突"), nHandleLine, dX, dY) ;
				acutPrintf(_T("\n%s"), strMsg) ;
				AfxMessageBox(strMsg) ;
				//if (bTestMode)
// 				{
// 					m_mysys.ZoomView(pt_3d_cen, dHeight, dWidth ) ;
// 				}	

				m_bFillPolarErro = true ;
			}
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in FillPolar2"));
	}	
}

// 取得上下行分界的y坐标，以上为下行，以下为上行
int CZnSxt::GetBorder(double &dBorderY, double &dBorderX)
{
	int iRet = 0 ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type=100 and sxt_ent_name='X'")) ;
		if (m_RecordSet.GetRecordCount(strSql)==1)
		{
			m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_y"), dBorderY) ;
			strSql.Format(_T("select * from sxt_ent where sxt_ent_type=100 and sxt_ent_name='S'")) ;
			if (m_RecordSet.GetRecordCount(strSql)==1)
			{
				double dBorderYTmp = 0 ;
				m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_y"), dBorderYTmp) ;
				if (fabs(dBorderYTmp-dBorderY)<2) //此处2可议
				{
					//从S出站分界，以左为下行，以右为上行
					strSql.Format(_T("select * from sxt_ent where sxt_ent_type=100 and left(sxt_ent_block,6)='XHJ_CZ' and left(sxt_ent_name,1)='S' and abs(sxt_ent_y-%.3f)<2 ")) ;
					if (m_RecordSet.GetRecordCount(strSql)==1)
					{
						m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_x"), dBorderX) ;
						iRet = 2 ;
					}
				}
				else
				{
					iRet = 1 ;
				}
			}
			else
			{
				iRet = 1 ;
			}
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in GetBorder"));
	}

	return iRet ;
}

// 计算实体是上行还是下行 db
void CZnSxt::SetEntUpOrDown(void)
{
	double dBorderY = 0.000, dBorderX = 0.000 ;
	int iBorderType = this->GetBorder(dBorderY, dBorderX) ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent")) ;
		if (cSet.Open(strSql))
		{
			if (cSet.IsBOF())
			{
				cSet.MoveFirst() ;
			}
			while (!cSet.IsEOF())
			{
				int iUpOrDown = 0 ;
				double dX = 0.000, dY = 0.000 ;
				cSet.GetFieldValue(_T("sxt_ent_x"), dX) ;
				cSet.GetFieldValue(_T("sxt_ent_y"), dY) ;
				if ((dY-dBorderY)>2)
				{
					iUpOrDown = 0 ;
				}
				else if ((dY-dBorderY)<-2)
				{
                    iUpOrDown = 1 ;
				}
				else
				{
					if (iBorderType==1)
					{
						iUpOrDown = 0 ;
					}
					else if (iBorderType==2)
					{
						if (dX>dBorderX)
						{
							iUpOrDown = 1 ;
						}
						else
						{
							iUpOrDown = 0 ;
						}
					}
				}
				cSet.Edit() ;
				cSet.SetFieldValue(_T("sxt_ent_upordown"), iUpOrDown) ;
				cSet.Update() ;

				cSet.MoveNext() ;
			}

			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in SetEntUpOrDown"));
	}
}

// 找出交叉渡线并更新数据库，算法是：一个绝缘节2端连接线都连接一个道岔反位就是渡线绝缘节
//找出端头线并更新数据库，算法是：一条线两端都是端头绝缘节或一头是端头，另一头无连接实体
void CZnSxt::FindCrossLineAndDtLine(void)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;

		//交叉渡线
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=2 and (sxt_ent_type mod 100)\\10<>5")) ;
		if (cSet.Open(strSql))
		{
			while (!cSet.IsEOF())
			{
				long nHandleLeftLine = 0, nHandleRightLine = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_conline_left"), nHandleLeftLine) ;
				cSet.GetFieldValue(_T("sxt_ent_conline_right"), nHandleRightLine) ;
				if (nHandleLeftLine!=0&&nHandleRightLine!=0)
				{
					strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d and sxt_ent_fwline_vertex=0"), nHandleLeftLine) ;
					int iFindDcLeft = m_RecordSet.GetRecordCount(strSql) ;
					strSql.Format(_T("select * from sxt_ent where sxt_ent_fwline_handle=%d and sxt_ent_fwline_vertex>0"), nHandleRightLine) ;
					int iFindDcRight = m_RecordSet.GetRecordCount(strSql) ;
                    
					if (iFindDcLeft>0 && iFindDcRight>0)
					{
						cSet.Edit() ;
						cSet.SetFieldValue(_T("sxt_ent_type"), 201) ;
						cSet.Update() ;
						//update线类型为渡线
						strSql.Format(_T("update sxt_line set sxt_line_type=1 where sxt_line_handle=%d"), nHandleLeftLine) ;
						m_AdoDb.Execute(strSql) ;
						strSql.Format(_T("update sxt_line set sxt_line_type=1 where sxt_line_handle=%d"), nHandleRightLine) ;
						m_AdoDb.Execute(strSql) ;
					}
				}

				cSet.MoveNext() ;
			}

			cSet.Close() ;
		}

		//端头线
		strSql.Format(_T("select * from sxt_ent where sxt_ent_block='JYJ_DT_0' ")) ;
		if (cSet.Open(strSql))
		{
			while (!cSet.IsEOF())
			{
// 				long nHandle = 0 ;
// 				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
// 				if (nHandle == 265194)
// 				{
// 					int iii = 0 ;
// 				}
				long nHandleConLines[2] ={ 0, 0} ; 
				cSet.GetFieldValue(_T("sxt_ent_conline_left"), nHandleConLines[0]) ;
				cSet.GetFieldValue(_T("sxt_ent_conline_right"), nHandleConLines[1]) ;
				for (int i=0; i<2; i++)
				{
					BOOL bIsDtLine = FALSE ;
					if (nHandleConLines[i]!=0)
					{
						strSql.Format(_T("select * from sxt_ent where  sxt_ent_conline_%s=%d"), i==0?_T("right"):_T("left"), nHandleConLines[i]) ;
						if (m_RecordSet.GetRecordCount(strSql)>0)  //有连接实体，但实体也是端头
						{
							CStringArray strArrayBlkName ;
							m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_block"), strArrayBlkName) ; //一般有2个结果，还有个信号机并置
							if (CBlkUtility::FindStrInArray(strArrayBlkName, _T("JYJ_DT_0"))!=-1)
							{
								bIsDtLine = TRUE ;
							}
						}
						else 	//该线另一侧无连接实体
						{
							strSql.Format(_T("select * from sxt_ent where  sxt_ent_fwline_handle=%d"), nHandleConLines[i]) ;
							if (m_RecordSet.GetRecordCount(strSql)==0)
							{
								bIsDtLine = TRUE ;
							}							
						}
						if (bIsDtLine)
						{
							strSql.Format(_T("update sxt_line set sxt_line_type=2 where sxt_line_handle=%d"), nHandleConLines[i]) ;
							//m_mysys.SetEntColorIndex(nHandleConLines[i], 1) ;
							m_AdoDb.Execute(strSql) ;
						}
					}
				}
				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}      

	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in FindCrossLine"));
	}
}

// 绝缘节距信号楼距离（和信号机并置的取信号机的值，其他取设定的值的已在读入实体到数据库时处理）
//计算绝缘节距信号楼距离（要么和信号机并置，要么显式指定了其距信号楼距离）,更新信号机、道岔距信号楼距离（左侧为负数）
void CZnSxt::UpdateToXhlDist(void)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=2 and (sxt_ent_type mod 100)\\10<>5 and sxt_ent_toxhldist=0")) ;
		if (cSet.Open(strSql))
		{
			if (cSet.IsBOF())
			{
				cSet.MoveFirst() ;
			}
			while (!cSet.IsEOF())
			{
				double dX = 0, dY = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_x"), dX) ;
				cSet.GetFieldValue(_T("sxt_ent_y"), dY) ;
				strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=1 and abs(sxt_ent_x-%.3f)<0.1 and abs(sxt_ent_y-%.3f)<0.1"), dX, dY) ;
				int iDist = 0 ;
				m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_toxhldist"), iDist ) ;
				cSet.Edit() ;
				cSet.SetFieldValue(_T("sxt_ent_toxhldist"), iDist) ;
				cSet.Update() ;

				cSet.MoveNext() ;
			}

			cSet.Close() ;
		}

		//将距信号楼距离按左右区分（左侧为负数，右侧为正数）
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type=310")) ;
		double dX_XHL = 0 ;
		if (m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_x"), dX_XHL) ==1)
		{
			strSql.Format(_T("select * from sxt_ent where sxt_ent_toxhldist>0")) ;
			if (cSet.Open(strSql))
			{
				while (!cSet.IsEOF())
				{
					double dX = 0 ;
					cSet.GetFieldValue(_T("sxt_ent_x"), dX) ;
					if (dX<dX_XHL)
					{
						int iDist = 0 ;
						cSet.GetFieldValue(_T("sxt_ent_toxhldist"), iDist) ;
						cSet.Edit() ;
						cSet.SetFieldValue(_T("sxt_ent_toxhldist"), -iDist) ;
						cSet.Update() ;
					}
					cSet.MoveNext() ;
				}
				cSet.Close() ;
			}
		}
		else
		{
			AfxMessageBox(_T("在更新距信号楼距离（将左侧设为负数，右侧为正数）时，未正确查询到信号楼x坐标!")) ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in UpdateToXhlDist"));
	}
}

int CZnSxt::ClearShowInfo(const CString& strToClearEntApp)
{
	int iRet = 0 ;
	AcDbDatabase* pDb = acdbHostApplicationServices()->workingDatabase() ;
	CUserSystem mysys ;
	mysys.m_pDb = pDb ;

	AcDbBlockTable * pBlkTbl = NULL ;
	pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;
		Acad::ErrorStatus es = pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead) ;
		if (Acad::eOk == es)
		{
			int iAppValue = 0 ;
			if (mysys.GetXdata(pEnt, strToClearEntApp, iAppValue))
			{
				pEnt->upgradeOpen() ;
				pEnt->erase() ;
				iRet++ ;
			}
			pEnt->close() ;
		}
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return iRet ;
}

// 删除当前图纸指定的扩展数据
int CZnSxt::DeleteXdata(const CStringArray& strArrayAppName)
{
	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;
		Acad::ErrorStatus es = pBlkTblRecIter->getEntity(pEnt, AcDb::kForWrite) ;
		if (Acad::eOk == es)
		{
			for (int i=0; i<strArrayAppName.GetCount(); i++)
			{
				m_mysys.DeleteXdata(pEnt, strArrayAppName.GetAt(i)) ;
			}
			pEnt->close() ;
		}
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return 0;
}

// 绘制跳线
void CZnSxt::DrawJumpLine(void)
{
	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0,_T("LWPOLYLINE"), kDxfRegAppName, _T("ForZZJ"), 0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEntFwInsdie = NULL ;
		if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEntFwInsdie, AcDb::kForRead))
		{
			int nHandleCloneFrom = 0 ;
			m_mysys.GetXdata(pEntFwInsdie, _T("CloneFrom"), nHandleCloneFrom) ;
			if (nHandleCloneFrom==98369)
			{
				int iii = 0 ;
			}

			CString strXdataVal ;
			m_mysys.GetXdata(pEntFwInsdie, _T("ForZZJ"), strXdataVal) ;
			CStringArray strArrayXdataVal ;
			CBlkUtility::DivideString(strXdataVal, _T("-"), strArrayXdataVal) ;
			if (strArrayXdataVal.GetCount()==5)
			{
				CString strDirect = strArrayXdataVal.GetAt(1) ;
				CString strFwVertex = strArrayXdataVal.GetAt(2) ;
				CString strDwLineHandle = strArrayXdataVal.GetAt(3) ;
				int nDwLineHandle = _tstoi(strDwLineHandle) ;
				CString strDwVertex = strArrayXdataVal.GetAt(4) ;
				int iDwVertex = _tstoi(strDwVertex) ;

				AcGePoint2d pt_2d_all[4][2] ; //  pt_2d_fw_inside[2], pt_2d_fw_out[2],pt_2d_dw_inside[2], pt_2d_dw_out[2] ;

				AcDbPolyline* pLineFwInside = AcDbPolyline::cast(pEntFwInsdie) ;
				int iNumVertsFwIn = pLineFwInside->numVerts() ;
				pLineFwInside->getPointAt(strFwVertex==_T("0")?1:iNumVertsFwIn-2, pt_2d_all[0][0]) ;
				pLineFwInside->getPointAt(strFwVertex==_T("0")?2:iNumVertsFwIn-3, pt_2d_all[0][1]) ;

				//fwline outside
				long nHandleFwOut = this->GetPLine(nHandleCloneFrom, strDirect==_T("1")?1:0) ;
				if (nHandleFwOut!=-1)
				{
					AcDbEntity* pEntFwOut = NULL ;
					if (m_mysys.OpenAcDbEntity(nHandleFwOut, pEntFwOut, AcDb::kForRead))
					{
						AcDbPolyline* pLineFwOut= AcDbPolyline::cast(pEntFwOut) ;
						int iNumVertsFwOut = pLineFwOut->numVerts() ;
						pLineFwOut->getPointAt(strFwVertex==_T("0")?0:iNumVertsFwOut-1, pt_2d_all[1][0]) ;
						pLineFwOut->getPointAt(strFwVertex==_T("0")?1:iNumVertsFwOut-2, pt_2d_all[1][1]) ;
						pEntFwOut->close() ;
					}
				}

				//dwline inside
				long nHandleDwInside = this->GetPLine(nDwLineHandle,  strDirect==_T("1")?1:0) ;
				if (nHandleDwInside!=-1)
				{
					AcDbEntity* pEntDWInside = NULL ;
					if (m_mysys.OpenAcDbEntity(nHandleDwInside, pEntDWInside, AcDb::kForRead))
					{
						int iDwVertexSxt = -1 ;
						CString strVertexInfo ;
						m_mysys.GetXdata(pEntDWInside, _T("VertexInfo"), strVertexInfo) ;
						CString strToFind ; 
						strToFind.Format(_T("C:%s"), strDwVertex) ;
						//int iFindLoc1 = strVertexInfo.Find(strToFind) ;
						int iFindLoc1 = -1 ;
						CUIntArray nArrayLoc ;
						int iSumFind = CBlkUtility::FindString(strVertexInfo, strToFind, nArrayLoc) ;
						if (iSumFind>0)
						{
							iFindLoc1 = nArrayLoc.GetAt(iSumFind-1) ;
						}						
						if (iFindLoc1!=-1)
						{
							CString strVertexInfo1 = strVertexInfo.Left(iFindLoc1) ;
							int iFindLoc2 = strVertexInfo1.ReverseFind(_T('A')) ;
							if (iFindLoc2!=-1)
							{
								CString strDwVertex = strVertexInfo1.Mid(iFindLoc2+2, 1)  ;
								iDwVertexSxt = _tstoi(strDwVertex) ;
							}
						}
						AcDbPolyline* pLineDwInside= AcDbPolyline::cast(pEntDWInside) ;
						pLineDwInside->getPointAt(strFwVertex==_T("0")?iDwVertexSxt:(iDwVertexSxt+1), pt_2d_all[2][0]) ;
						pLineDwInside->getPointAt(strFwVertex==_T("0")?(iDwVertexSxt+1):iDwVertexSxt, pt_2d_all[2][1]) ;
						pEntDWInside->close() ;
					}
				}

				//dwline outside
				long nHandleDwOut = this->GetPLine(nDwLineHandle, strDirect==_T("1")?0:1) ;
				if (nHandleDwOut!=-1)
				{
					AcDbEntity* pEntDWOut = NULL ;
					if (m_mysys.OpenAcDbEntity(nHandleDwOut, pEntDWOut, AcDb::kForRead))
					{
						int iDwVertexSxt = -1 ;
						CString strVertexInfo ;
						m_mysys.GetXdata(pEntDWOut, _T("VertexInfo"), strVertexInfo) ;
						CString strToFind ; 
						strToFind.Format(_T("C:%s"), strDwVertex) ;
						int iFindLoc1 = strVertexInfo.Find(strToFind) ;
						if (iFindLoc1!=-1)
						{
							CString strVertexInfo1 = strVertexInfo.Left(iFindLoc1) ;
							int iFindLoc2 = strVertexInfo1.ReverseFind(_T('A')) ;
							if (iFindLoc2!=-1)
							{
								CString strDwVertex = strVertexInfo1.Mid(iFindLoc2+2, 1)  ;
								iDwVertexSxt = _tstoi(strDwVertex) ;
							}
						}
						AcDbPolyline* pLineDwOut= AcDbPolyline::cast(pEntDWOut) ;
						pLineDwOut->getPointAt(strFwVertex==_T("0")?iDwVertexSxt:(iDwVertexSxt+1), pt_2d_all[3][0]) ;
						pLineDwOut->getPointAt(strFwVertex==_T("0")?(iDwVertexSxt+1):iDwVertexSxt, pt_2d_all[3][1]) ;
						pEntDWOut->close() ;
					}
				}	

				//计算绘制跳线
				AcGePoint3d pt_3d_all[4][2] ;
				for (int i=0; i<4; i++)
				{
					for (int j=0; j<2; j++)
					{
						m_mysys.ConvertPoint(pt_2d_all[i][j], pt_3d_all[i][j]) ;
					}
				}
				AcDbLine* pLineFwIn = new AcDbLine(pt_3d_all[0][0], pt_3d_all[0][1]) ;
				AcDbLine* pLineFwOut = new AcDbLine(pt_3d_all[1][0], pt_3d_all[1][1]) ;
				AcDbLine* pLineDwIn = new AcDbLine(pt_3d_all[2][0], pt_3d_all[2][1]) ;
				AcDbLine* pLineDwOut = new AcDbLine(pt_3d_all[3][0], pt_3d_all[3][1]) ;

				AcGeVector2d vecFw = pt_2d_all[0][1] - pt_2d_all[0][0] ;
				AcGeVector2d vecDw = pt_2d_all[2][1] - pt_2d_all[2][0] ;
				double dAngle = vecFw.angleTo(vecDw) ;
				dAngle+=(PI/8) ; //将角度增加一定值
				if (strFwVertex==_T("0")&&strDirect==_T("1")||strFwVertex!=_T("0")&&strDirect==_T("0"))
				{
					dAngle = -dAngle ;
				}                
				AcGePoint3dArray pt3dArrayIn ;
				Acad::ErrorStatus es1 = pLineDwIn->intersectWith(pLineFwIn, AcDb::kExtendBoth, pt3dArrayIn) ;				
				AcGePoint3dArray pt3dArrayOut ;
				Acad::ErrorStatus es2 = pLineDwOut->intersectWith(pLineFwOut, AcDb::kExtendBoth, pt3dArrayOut) ;

				if (es1==Acad::eOk && es2==Acad::eOk)
				{
					int iNum1 = pt3dArrayIn.length() ;
					int iNum2 = pt3dArrayOut.length() ;
					if (iNum2==1&&iNum1==1)
					{
						AcGePoint3d pt_3d_out = pt3dArrayOut.at(0) ;
						AcGePoint3d pt_3d_in = pt3dArrayIn.at(0) ;
						double dLen = pt_3d_in.distanceTo(pt_3d_out)+5.5 ;	
						for (int iLineIndex=0; iLineIndex<2; iLineIndex++)
						{
							AcGePoint3d pt_3d_onlinefw;
							m_mysys.GetPoint(pt_3d_out, dLen+iLineIndex*1.5, vecFw.angle(), pt_3d_onlinefw) ;
							m_mysys.AddAcDbPolyLine(AcGePoint3d(pt_3d_onlinefw.x-0.25, pt_3d_onlinefw.y,0), AcGePoint3d(pt_3d_onlinefw.x+0.25, pt_3d_onlinefw.y,0), 0.5, PI, 7, TRUE) ; //点
							AcGePoint3d pt_3d_onlinedw ;
							m_mysys.GetPoint(pt_3d_out, dLen+iLineIndex*1.5, vecDw.angle(), pt_3d_onlinedw) ;
							m_mysys.AddAcDbPolyLine(AcGePoint3d(pt_3d_onlinedw.x-0.25, pt_3d_onlinedw.y,0), AcGePoint3d(pt_3d_onlinedw.x+0.25, pt_3d_onlinedw.y,0), 0.5, PI, 7, TRUE) ; //点

							m_mysys.AddAcDbPolyLine(pt_3d_onlinefw, pt_3d_onlinedw, 0, dAngle, 7) ;//跳线
						}
					}
				}
                
				delete pLineFwIn ;
				pLineFwIn = NULL ;
				delete pLineFwOut ;
				pLineFwOut = NULL ;				
				delete pLineDwIn ;
				pLineDwIn = NULL ;				
				delete pLineDwOut ;
				pLineDwOut = NULL ;
			}
			pEntFwInsdie->close() ;
		}
	}

}
//取得 CloneFrom=nCloneFromHandle,UPDOWN=iUpOrDwon 的多段线
long CZnSxt::GetPLine(long nCloneFromHandle, int iUpOrDwon)
{
	long nRet = 0 ;
	int iRetNum = 0 ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	//pRb = acutBuildList(RTDXF0,_T("LWPOLYLINE"), 1001, _T("CloneFrom"), 1070, nCloneFromHandle, 1001, _T("UPDOWN"), 1070, iUpOrDwon, 0) ;
	pRb = acutBuildList(RTDXF0,_T("LWPOLYLINE"), 1001, _T("CloneFrom"), 1001, _T("UPDOWN"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;
	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEnt = NULL ;
		long nTemp = nArrayHandle.GetAt(i) ;
		if (m_mysys.OpenAcDbEntity(nTemp, pEnt, AcDb::kForRead))
		{
			long  nCloneFrom = 0 ;
			int iUPDOWN = 0 ;
			m_mysys.GetXdata(pEnt, _T("CloneFrom"), nCloneFrom) ;
			m_mysys.GetXdata(pEnt, _T("UPDOWN"), iUPDOWN) ;
			if (nCloneFrom==nCloneFromHandle&&iUPDOWN==iUpOrDwon)
			{
				nRet = nTemp ;
                iRetNum++ ;
			}
			pEnt->close() ;
		}
	}
	if (iRetNum==1)
	{
		return nRet ;
	}
	else
	{
		acutPrintf(_T("\n在查找CloneFrom=%d,UPDOWN=%d的多段线时，结果为%d"), nCloneFromHandle, iUpOrDwon, iRetNum) ;
		return -1 ;
	}
}

// 绘制指定送受电、厄流图形
void CZnSxt::DrawBoxFsE(long nHandleJyj)
{
// 	if (nHandleJyj==2636)
// 	{
// 		int iii = 0 ;
// 	}
	//先清除原来的
	this->ClearBoxFsE(nHandleJyj) ;

	AcGePoint3d pt_3d_up, pt_3d_down ;
	BOOL bGetPtOfSxtJyj = this->GetPtOfSxtJyj(nHandleJyj, pt_3d_up, pt_3d_down) ;
	if (bGetPtOfSxtJyj==FALSE) //没有双线部分，直接退出，解决了没有双线图时运行此函数会导致平面图信号机等移位问题
	{
		return ;
	}

	//保存当前图层，设置要绘制的图层
	AcDbObjectId ojbIdLayerOld = m_pDb->clayer() ;
	m_mysys.SetCLayer(_T("CRSC_SJY_XTS_SXT_DLJL_GD"), 2) ;
	m_iColorIndexCur = 256 ;

	// begin 绘制盒子
	CString strSql ;
	strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleJyj) ;
	int iUpOrDown = 0 ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_upordown"), iUpOrDown) ;

	///begin 绝缘节所在线段信息	
	AcGeVector2d vecOnLine = GetAngleOfJyjOnLine(nHandleJyj) ;
	double dAngleOnLine = 0 ;
	dAngleOnLine = vecOnLine.angle() ;
	//end  绝缘节所在线段信息

	//左右轨道
	long nHandleLeftLine = 0, nHandleRightLine = 0 ;
	this->GetGdBesideJyj(nHandleJyj, nHandleLeftLine, nHandleRightLine) ;
	CGdDATA dataGdLeft, dataGdRight ;
	this->GetEntData(nHandleLeftLine, dataGdLeft) ;
	this->GetEntData(nHandleRightLine, dataGdRight) ;

	AcDbBlockReference* pRef = NULL ;
	AcGePoint3d pt_3d_cen; //双线中间线和绝缘节交点位置，此处为厄流插入点

	pt_3d_cen = m_mysys.GetMidPoint(pt_3d_up, pt_3d_down) ;
	double dEntX = pt_3d_cen.x, dEntY = pt_3d_cen.y ;
	CString strBlkName ;
	AcGePoint3d pt_3d_xb(0,0,0) ; //送受电盒子

	//双线中线一段
	AcGePoint2d pt_2d_cen1 = pt_3d_cen.convert2d(AcGePlane::kXYPlane)+vecOnLine ;
	AcGePoint3d pt_3d_cen1(0,0,0) ;
	m_mysys.ConvertPoint(pt_2d_cen1, pt_3d_cen1) ;

	//绝缘节左边
	if (dataGdLeft.m_bIsDH) //有厄流
	{
		double dAngleE = dAngleOnLine ;
		strBlkName = _T("BOX_E_0") ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_cen))
		{
			if (iUpOrDown==1)
			{
				dAngleE+=PI ;
				AcGeScale3d scale_3d = pRef->scaleFactors()  ;	
				scale_3d.set(-scale_3d.sx, scale_3d.sy, scale_3d.sz) ;
				pRef->setScaleFactors(scale_3d) ;
			}
			pRef->setRotation(pRef->rotation()+dAngleE) ;
			m_mysys.SetXdata(pRef, _T("BELONG_JYJ"), nHandleJyj) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleLeftLine) ;

			pRef->close() ;
			pRef = NULL ;
		}
		//连线
		AcGePoint3d pt_3d_left(pt_3d_cen.x-2, pt_3d_cen.y+6,0), pt_3d_right(pt_3d_cen.x+2, pt_3d_cen.y+6,0) ;						
		AcDbObjectId objIdPline = m_mysys.AddAcDbPolyLine(pt_3d_left, pt_3d_right, 0.2, 0, m_iColorIndexCur) ;
		AcDbEntity* pEntLine = NULL ;
		if (m_mysys.OpenAcDbEntity(objIdPline, pEntLine))
		{
			m_mysys.RotationEnt(pEntLine,dAngleE, pt_3d_cen ) ;	
			m_mysys.SetXdata(pEntLine, _T("BELONG_JYJ"), nHandleJyj) ;
			pEntLine->close() ;
		}
		//左侧点
		pt_3d_left.rotateBy(dAngleE, AcGeVector3d::kZAxis, pt_3d_cen) ;
		AcDbObjectId objIdConNodeLeft= m_mysys.DrawConNode(pt_3d_left, 0.4) ;
		m_mysys.SetXdata(objIdConNodeLeft, _T("BELONG_JYJ"), nHandleJyj) ;
		
		if (!dataGdRight.m_bIsDH)//右侧没有厄流，需要将左侧厄流连上轨道
		{
			AcDbObjectId objIds[3] ;
			AcGePoint3d pt_3d_3(pt_3d_right.x+1, pt_3d_right.y, 0) ;
			AcGePoint3d pt_3d_4(pt_3d_3.x, pt_3d_cen.y+2.5, 0) ;
			AcGePoint3d pt_3d_5(pt_3d_3.x, pt_3d_cen.y-2.5, 0) ;
			AcGePoint3dArray pt3dArray ;
			pt3dArray.append(pt_3d_right) ;
			pt3dArray.append(pt_3d_3) ;
			pt3dArray.append(pt_3d_4) ;
			pt3dArray.append(pt_3d_5) ;
			objIds[0] = m_mysys.AddAcDbPolyLine(pt3dArray, 0.2, m_iColorIndexCur) ;
			AcGePoint3d pt_3d_startArc(pt_3d_4.x-0.2, pt_3d_4.y, 0) ;						
			objIds[1] = m_mysys.AddAcDbPolyLine(pt_3d_startArc, PI, pt_3d_4, 0.4, m_iColorIndexCur) ;
			pt_3d_startArc.y = pt_3d_5.y ;
			objIds[2] = m_mysys.AddAcDbPolyLine(pt_3d_startArc, PI, pt_3d_5, 0.4, m_iColorIndexCur) ;

			for (int i=0; i<3; i++)
			{
				pEntLine = NULL ;
				if (m_mysys.OpenAcDbEntity(objIds[i], pEntLine))
				{
					m_mysys.RotationEnt(pEntLine,dAngleOnLine, pt_3d_cen ) ;	
					if (iUpOrDown==1)
					{
						AcGeMatrix3d matrix ;
						matrix.setToMirroring(AcGeLine3d(pt_3d_cen, pt_3d_cen1)) ;
						pEntLine->transformBy(matrix) ;
					}
					m_mysys.SetXdata(pEntLine, _T("BELONG_JYJ"), nHandleJyj) ;
					pEntLine->close() ;
				}
			}
		}
	}
	if (dataGdLeft.m_strRightFOrS!=_T("N")&&!(m_i4Or2Line==1&&dataGdLeft.m_bIsDMH)) //二线制且是电码化区段时不绘制该送受电（和电码化同缆）
	{
		strBlkName.Format(_T("BOX_FSD_%s_0"), dataGdLeft.m_strRightFOrS) ;
		double dBoxX = dEntX-4 ;
		double dBoxY = dEntY+(iUpOrDown==0?(dataGdLeft.m_bIsDH?10:6):-(dataGdRight.m_bIsDH?10:6)) ;
		pt_3d_xb.x = dBoxX ;
		pt_3d_xb.y = dBoxY ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_xb))
		{
			m_mysys.RotationEnt(pRef, dAngleOnLine, pt_3d_cen) ;
			m_mysys.SetXdata(pRef, _T("BELONG_JYJ"), nHandleJyj) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleLeftLine) ;
			m_mysys.SetXdata(pRef, _T("LEFT_RIGHT"), (long)0) ;

			pRef->close() ;
			pRef = NULL ;
		}
	}
	//绝缘节右边
	if (dataGdRight.m_bIsDH) //有厄流
	{
		double dAngleE = dAngleOnLine ;
		if (m_mysys.InsertBlock(pRef, _T("BOX_E_0"), pt_3d_cen))
		{
			if (iUpOrDown==0)
			{
				AcGeScale3d scale_3d = pRef->scaleFactors()  ;	
				scale_3d.set(-scale_3d.sx, scale_3d.sy, scale_3d.sz) ;
				pRef->setScaleFactors(scale_3d) ;
			}
			else if (iUpOrDown==1)
			{
				dAngleE+=PI ;
			}
			pRef->setRotation(pRef->rotation()+dAngleE) ;
			m_mysys.SetXdata(pRef, _T("BELONG_JYJ"), nHandleJyj) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleRightLine) ;

			pRef->close() ;
			pRef = NULL ;
		}
		//右侧点
		AcGePoint3d pt_3d_node(pt_3d_cen.x+2, pt_3d_cen.y+6,0) ;
		pt_3d_node.rotateBy(dAngleE, AcGeVector3d::kZAxis, pt_3d_cen) ;
		AcDbObjectId objIdConNodeRight= m_mysys.DrawConNode(pt_3d_node, 0.4) ;
		m_mysys.SetXdata(objIdConNodeRight, _T("BELONG_JYJ"), nHandleJyj) ;
		//连线
		if (!dataGdLeft.m_bIsDH)
		{
			AcGePoint3d pt_3d_left(pt_3d_cen.x-2, pt_3d_cen.y+6,0), pt_3d_right(pt_3d_cen.x+2, pt_3d_cen.y+6,0) ;							
			AcDbObjectId objIdPline = m_mysys.AddAcDbPolyLine(pt_3d_left, pt_3d_right, 0.2, m_iColorIndexCur) ;
			AcDbEntity* pEntLine = NULL ;
			if (m_mysys.OpenAcDbEntity(objIdPline, pEntLine))
			{
				m_mysys.RotationEnt(pEntLine,dAngleE, pt_3d_cen ) ;	
				m_mysys.SetXdata(pEntLine, _T("BELONG_JYJ"), nHandleJyj) ;
				pEntLine->close() ;
			}

			AcDbObjectId objIds[3] ;
			AcGePoint3d pt_3d_3(pt_3d_left.x-1, pt_3d_left.y, 0) ;
			AcGePoint3d pt_3d_4(pt_3d_3.x, pt_3d_cen.y+2.5, 0) ;
			AcGePoint3d pt_3d_5(pt_3d_3.x, pt_3d_cen.y-2.5, 0) ;
			AcGePoint3dArray pt3dArray ;
			pt3dArray.append(pt_3d_right) ;
			pt3dArray.append(pt_3d_3) ;
			pt3dArray.append(pt_3d_4) ;
			pt3dArray.append(pt_3d_5) ;
			objIds[0] = m_mysys.AddAcDbPolyLine(pt3dArray, 0.2, m_iColorIndexCur) ;
			AcGePoint3d pt_3d_startArc(pt_3d_4.x-0.2, pt_3d_4.y, 0) ;						
			objIds[1] = m_mysys.AddAcDbPolyLine(pt_3d_startArc, PI, pt_3d_4, 0.4, m_iColorIndexCur) ;
			pt_3d_startArc.y = pt_3d_5.y ;
			objIds[2] = m_mysys.AddAcDbPolyLine(pt_3d_startArc, PI, pt_3d_5, 0.4, m_iColorIndexCur) ;

			for (int i=0; i<3; i++)
			{
				pEntLine = NULL ;
				if (m_mysys.OpenAcDbEntity(objIds[i], pEntLine))
				{
					m_mysys.RotationEnt(pEntLine,dAngleOnLine, pt_3d_cen ) ;	
					if (iUpOrDown==1)
					{
						AcGeMatrix3d matrix ;
						matrix.setToMirroring(AcGeLine3d(pt_3d_cen, pt_3d_cen1)) ;
						pEntLine->transformBy(matrix) ;
					}
					m_mysys.SetXdata(pEntLine, _T("BELONG_JYJ"), nHandleJyj) ;

					pEntLine->close() ;
				}
			}							
		}
	}
	if (dataGdRight.m_strLeftFOrS!=_T("N")&&!(m_i4Or2Line==1&&dataGdRight.m_bIsDMH))
	{
		strBlkName.Format(_T("BOX_FSD_%s_0"), dataGdRight.m_strLeftFOrS) ;
		double dBoxX = dEntX+4 ;
		double dBoxY = dEntY+(iUpOrDown==0?(dataGdRight.m_bIsDH?10:6):-(dataGdRight.m_bIsDH?10:6)) ;
		pt_3d_xb.x = dBoxX ;
		pt_3d_xb.y = dBoxY ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_xb))
		{
			m_mysys.RotationEnt(pRef, dAngleOnLine, pt_3d_cen) ;
			m_mysys.SetXdata(pRef, _T("BELONG_JYJ"), nHandleJyj) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleRightLine) ;
			m_mysys.SetXdata(pRef, _T("LEFT_RIGHT"), (long)1) ;

			pRef->close() ;
			pRef = NULL ;
		}
	}		
	//end 绘制盒子

	//移动信号机
	long nHandleXhj = 0 ;
	this->GetXhjInSxt(nHandleLeftLine, nHandleRightLine, nHandleXhj) ;
	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandleXhj, pEnt))
	{
		AcDbBlockReference* pRefXhj = AcDbBlockReference::cast(pEnt) ;
		m_mysys.GetBlockName(pRefXhj, strBlkName) ;	
		double dAngle = pRefXhj->rotation() ;
		AcGePoint3d pt_3d_ref = pRefXhj->position() ;

		//盒子高度
		int iLeft = (dataGdLeft.m_strRightFOrS!=_T("N")?1:0)+(dataGdLeft.m_bIsDH?1:0) ;
		int iRight = (dataGdRight.m_strLeftFOrS!=_T("N")?1:0)+(dataGdRight.m_bIsDH?1:0) ;
		double dBoxHeight = 0.000 ;
		int iMax = max(iLeft, iRight) ;
		if (iMax==1)
		{
			dBoxHeight+=5 ;
		}
		else if (iMax==2)
		{
			dBoxHeight+=9 ;
		}

		//begin 在双线图中，信号机基点不能在中线上，要至少上/下移2.5
		AcGePoint3d pt_3d_new2(pt_3d_cen) ;
		int iMarkLoc = strBlkName.Find(_T("_Z_")) ;//正向信号机, 逆向信号机
		//角度在90度~270度之间是倒置了
		if (dAngle>PI/2&&dAngle<PI*3/2)
		{
			pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?2.5:-2.5) ;
			if (iUpOrDown==0) //下行
			{
				pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?dBoxHeight:0) ;
			}
			else
			{
				pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?0:-dBoxHeight) ;
			}
		}
		else
		{
			pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?-2.5:2.5) ;
			if (iUpOrDown==0) //下行
			{
				pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?0:dBoxHeight) ;
			}
			else
			{
				pt_3d_new2.x=pt_3d_new2.x+(iMarkLoc==-1?-dBoxHeight:0) ;
			}
		}
		pt_3d_new2.rotateBy(PI/2+dAngleOnLine, AcGeVector3d::kZAxis, pt_3d_cen) ;
		AcGeVector3d vec = pt_3d_new2 - pt_3d_ref ;
		AcGeMatrix3d matrix ;
		matrix.setToTranslation(vec) ;
		pRefXhj->transformBy(matrix) ; 

		pEnt->close() ;
	}

	m_pDb->setClayer(ojbIdLayerOld) ; //恢复当前图层
}

// 绝缘节所在线的vec
AcGeVector2d CZnSxt::GetAngleOfJyjOnLine(long nHandleJyj)
{
	AcGeVector2d vecOnLine(0,0) ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		//begin 绝缘节所在线段信息
		strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleJyj ) ;
		if (cSet.Open(strSql))
		{
			int iRsCount = cSet.GetRecordCount() ;
			if (iRsCount==1)
			{
				long nHandleOnLine  = 0 , nVertexOnLine ;
				cSet.GetFieldValue(_T("sxt_ent_online_handle"), nHandleOnLine) ;
				if (nHandleOnLine!=0)//未分段pmt
				{
					cSet.GetFieldValue(_T("sxt_ent_online_vertex"), nVertexOnLine) ;
				}
				else //分段平面图
				{
					cSet.GetFieldValue(_T("sxt_ent_conline_left"), nHandleOnLine) ;
					cSet.GetFieldValue(_T("sxt_ent_conline_left_vertex"), nVertexOnLine) ;
					//防止端头绝缘节(不能同时采用left和right线)
					if (nHandleOnLine!=0)
					{
						nVertexOnLine-- ;
					}
					else
					{
						cSet.GetFieldValue(_T("sxt_ent_conline_right"), nHandleOnLine) ;
						cSet.GetFieldValue(_T("sxt_ent_conline_right_vertex"), nVertexOnLine) ;
					}
				}

				AcGePoint2d pt_2d_online1, pt_2d_online2 ;
				AcDbEntity* pEntOnLine = NULL ;
				if (m_mysys.OpenAcDbEntity(nHandleOnLine, pEntOnLine, AcDb::kForRead))
				{
					if (pEntOnLine->isKindOf(AcDbPolyline::desc()))
					{
						AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEntOnLine) ;
						pPolyLine->getPointAt(nVertexOnLine, pt_2d_online1) ;
						pPolyLine->getPointAt(nVertexOnLine+1, pt_2d_online2) ;
					}
					pEntOnLine->close() ;
				}
				vecOnLine = pt_2d_online2-pt_2d_online1 ;
			}
			else
			{
				CString strMsg ;
				strMsg.Format(_T("handle为%d的绝缘节个数为%d"), nHandleJyj, iRsCount) ;
				AfxMessageBox(strMsg) ;
			}
		}

		//end  绝缘节所在线段信息
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in GetAngleOfJyjOnLine"));
	}

	return vecOnLine ;
}

// 双线图中上下2个绝缘节
BOOL CZnSxt::GetPtOfSxtJyj(long nHandleJyj, AcGePoint3d& pt_3d_up, AcGePoint3d& pt_3d_down)
{
	BOOL bRet = FALSE ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0,_T("INSERT"), 1001, _T("CloneFrom"), 1001, _T("UPDOWN"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	int iTimeSetUp = 0, iTimeSetDown = 0 ;
	
	for (int i=0; i<nLen; i++)
	{
		long nHandle = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			long nHandleCloneFrom = 0 ;
			m_mysys.GetXdata(pEnt, _T("CloneFrom"), nHandleCloneFrom) ;
			if (nHandleCloneFrom==nHandleJyj)
			{
				int iUPDOWN = -1 ;
				m_mysys.GetXdata(pEnt, _T("UPDOWN"), iUPDOWN) ;
				if (iUPDOWN>-1)
				{
					AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
					AcGePoint3d pt_3d_tmp = pRef->position() ;
					if (iUPDOWN==1)
					{
						pt_3d_up = pt_3d_tmp ;
						iTimeSetUp++ ;
					}
					else
					{
						pt_3d_down = pt_3d_tmp ;
						iTimeSetDown++ ;
					}
				}
			}
			pEnt->close() ;
		}		
	}

	if (iTimeSetUp==1&&iTimeSetDown==1)
	{
		bRet = TRUE ;
	}

	return bRet ;
}

// 绘制所有送受电、厄流
void CZnSxt::DrawBoxFsE(void)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=2 and ((sxt_ent_type mod 100)\\10)<>5 and sxt_ent_type<>201")) ; //整除为2的,且不是250、251之类的切割绝缘节，也不是201之类的渡线绝缘节
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				this->DrawBoxFsE(nHandle) ;
				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in DrawBoxFsE"));
	}
}

// 取得指定绝缘节两侧的轨道
int CZnSxt::GetGdBesideJyj(long nHandleJyj, long& nHandleLeftLine, long& nHandleRightLine)
{
	int iRet = 0 ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleJyj) ;
		long nHandleLeftLineTmp = 0, nHandleRightLineTmp = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_conline_left"), nHandleLeftLine) ;
		if (nHandleLeftLineTmp!=0)
		{
			nHandleLeftLine = nHandleLeftLineTmp ;
			iRet++ ;
		}
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_conline_right"), nHandleRightLine) ;
		if (nHandleRightLineTmp!=0)
		{
			nHandleRightLine = nHandleRightLineTmp ;
			iRet++ ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in GetGdBesideJyj"));
	}

	return iRet ;
}

// 清除指定的送受电、厄流图形
void CZnSxt::ClearBoxFsE(long nHandleJyj)
{
	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(1001, _T("BELONG_JYJ"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		long nHandle = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			long nHandleBelong = 0 ;
			m_mysys.GetXdata(pEnt, _T("BELONG_JYJ"), nHandleBelong) ;
			if (nHandleBelong==nHandleJyj)
			{
				long iIsFor = 0 ;
				m_mysys.GetXdata(pEnt, _T("IS_FOR"), iIsFor) ;
				if (iIsFor==0) //is_for=0表示是轨道电路送受电的盒子等，=1表示电码化的
				{
					pEnt	->upgradeOpen() ;
					pEnt->erase() ;
				}
			}
			pEnt->close() ;
		}		
	}
}

// 取得两线间的信号机在sxt中的copy
int CZnSxt::GetXhjInSxt(long nHandleLeftLine, long nHandleRightLine, long& nHandleXhj)
{
	int iRet = 0 ;
	CString strSql ;
	long nHandleXhjInPmt = 0 ;
	if (nHandleLeftLine==0&&nHandleRightLine==0)
	{
		return 0 ;
	}
	else if (nHandleLeftLine>0&&nHandleRightLine==0)
	{
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=1 and sxt_ent_conline_left=%d "), nHandleLeftLine) ;
	}
	else if (nHandleLeftLine==0&&nHandleRightLine>0)
	{
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=1 and sxt_ent_conline_right=%d"), nHandleRightLine) ;
	}
	else if (nHandleLeftLine>0&&nHandleRightLine>0)
	{
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=1 and (sxt_ent_conline_left=%d or sxt_ent_conline_right=%d)"), nHandleLeftLine, nHandleRightLine) ;
	}
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleXhjInPmt) ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0, _T("INSERT"), 1001, _T("CloneFrom"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		long nHandle = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			long nHandleCloneFrom = 0 ;
			m_mysys.GetXdata(pEnt, _T("CloneFrom"), nHandleCloneFrom) ;
			if (nHandleCloneFrom==nHandleXhjInPmt)
			{
				nHandleXhj = nHandle ;
				iRet++ ;
			}
			pEnt->close() ;
		}		
	}
	return iRet;
}

// 绘制指定道岔的转辙机
void CZnSxt::DrawZzj(long nHandleDc)
{
// 	if (nHandleDc==116158)
// 	{
// 		int iii = 0 ;
// 	}
	//先清除原有图形
	//this->ClearZzj(nHandleDc) ;
	this->ClearEnt(nHandleDc, 1) ;
	long nHandlePlineBelongDc = 0 ;
	if (this->GetSxBelongDc(nHandleDc, nHandlePlineBelongDc)==0) //没找到相应的双线，直接退出
	{
		return ;
	}

	//保存当前图层，设置要绘制的图层
	AcDbObjectId ojbIdLayerOld = m_pDb->clayer() ;
	m_mysys.SetCLayer(_T("CRSC_SJY_XTS_SXT_DLJL_ZZJ"), 3) ;
	m_iColorIndexCur = 256 ;

	CString strSql ;
	strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleDc) ;
	int iUpOrDownDc = 0 ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_upordown"), iUpOrDownDc) ;

	//道岔信息
	CDcDATA dcData ;
	this->GetEntData(nHandleDc, dcData) ;
	double dAngleDc = 	m_mysys.GetAngle(nHandleDc) ; //道岔块旋转角度	

	int iDcDirectV = 0 ; //道岔FW朝向（1，朝上；0，朝下） 
	int iFwIndexVertex = 0 ; //FW端线的顶点vertex（起点或终点）
	AcGePoint2d pt_2d_ext0, pt_2d_ext1, pt_2d_ext2 ; //双线中和online平行的一小段线的端头点和紧邻的点
	AcGeVector2d vecDw ;

	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandlePlineBelongDc, pEnt, AcDb::kForRead))
	{
		CString strForZZJ ;
		m_mysys.GetXdata(pEnt, _T("ForZZJ"), strForZZJ) ;
		CStringArray strArrayForZZJ ;
		CBlkUtility::DivideString(strForZZJ, _T("-"), strArrayForZZJ) ;
		if (strArrayForZZJ.GetCount()==5)
		{
			CString strTmp = strArrayForZZJ.GetAt(0) ;
			long nHandleDcTmp = 0 ;
			nHandleDcTmp = _ttol(strTmp) ;
			if (nHandleDcTmp==nHandleDc)
			{
				AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
				iDcDirectV = _ttoi(strArrayForZZJ.GetAt(1)) ; //道岔FW朝向（1，朝上；0，朝下） 
				iFwIndexVertex =  _ttoi(strArrayForZZJ.GetAt(2)) ; //FW端线的顶点vertex（起点或终点）
				if (iFwIndexVertex==0)
				{
					pPolyLine->getPointAt(0, pt_2d_ext0) ;
					pPolyLine->getPointAt(1, pt_2d_ext1) ;
					pPolyLine->getPointAt(2, pt_2d_ext2) ;
					vecDw = pt_2d_ext1-pt_2d_ext0 ;
				}
				else
				{
					int iNumOfPline = pPolyLine->numVerts() ;
					pPolyLine->getPointAt(iNumOfPline-1, pt_2d_ext0) ;
					pPolyLine->getPointAt(iNumOfPline-2, pt_2d_ext1) ;
					pPolyLine->getPointAt(iNumOfPline-3, pt_2d_ext2) ;
					vecDw = pt_2d_ext0-pt_2d_ext1 ;
				}
			}
		}
		pEnt->close() ;
	}	

	//begin draw zzj
	CStringArray strArrayDrag ;
	dcData.GetDrag(strArrayDrag, 0) ;

	int iDcDirectH =(iFwIndexVertex==0?0:1) ; //道岔FW水平朝向，0朝右，1，朝左
	AcGeLineSeg2d line2d(pt_2d_ext0, pt_2d_ext1) ;		
	
	AcGePoint2d pt_2d_extcen =line2d.midPoint() ;
	AcGePoint2d pt_2d_extcen_mid = pt_2d_extcen ;
	pt_2d_extcen_mid.y+=(iDcDirectV==1?1.5:-1.5) ;
	pt_2d_extcen_mid.rotateBy(vecDw.angle(), pt_2d_extcen) ; //双线中心线

	//映射到双线的一边
	AcGePoint2d pt_2d_extcen_jg = pt_2d_extcen_mid ;
	pt_2d_extcen_jg.y +=(iUpOrDownDc==0?-2.5:2.5); 
	pt_2d_extcen_jg.rotateBy(vecDw.angle(), pt_2d_extcen) ;
	AcDbBlockReference* pRefZzj = NULL ;
	CString strBlkNameZzj ;
	strBlkNameZzj.Format(_T("ZZJ_SXT_JX_0")) ;

	//第几动（对于交流直捅到楼的，即使多动也各自按一动处理）
	CString strActionInfo ;
	this->GetDcActionInfo(dcData.m_strJG_QyType, dcData.GetActionNum(), dcData.m_nActIndex==0?1:dcData.m_nActIndex, strActionInfo) ;

	for (int i=0; i<dcData.m_nJG_QyNum; i++)
	{
		double dX_Jg = pt_2d_extcen_jg.x+(iDcDirectH==0?-1:1)*dcData.m_nJG_QyNum*3/2+(iDcDirectH==0?1:-1)*1.5+i*3*(iDcDirectH==0?1:-1) ;
		AcGePoint2d pt_2d_zzj(dX_Jg, pt_2d_extcen_jg.y) ;
		pt_2d_zzj.rotateBy(vecDw.angle(), pt_2d_extcen_jg) ;
		AcGePoint3d pt_3d_zzj(pt_2d_zzj.x, pt_2d_zzj.y, 0) ;
		CStringArray strArrayVal ;
		CString strVal ;
		strVal.Format(_T("J%d"), i+1) ;
		strArrayVal.Add(strVal) ;	
		strArrayVal.Add(strArrayDrag.GetAt(i)) ;
		strArrayVal.Add(strActionInfo) ;
		if (m_mysys.InsertBlock(pRefZzj, strBlkNameZzj, pt_3d_zzj, &strArrayVal, FALSE))
		{
			m_mysys.RotationEnt(pRefZzj, vecDw.angle()+(iUpOrDownDc==1?PI:0), pt_3d_zzj) ;
			m_mysys.SetXdata(pRefZzj, _T("IS_JXG"), 1) ;
			m_mysys.SetXdata(pRefZzj, _T("BELONG_ENT"), nHandleDc) ;
			//m_mysys.SetXdata(pRefZzj, _T("ACTION_INFO"), strActionInfo) ;
			//绘制对应的箱盒
			this->DrawBoxZzj(pRefZzj, &dcData, i,  iDcDirectH) ;
			pRefZzj->close() ;
			pRefZzj = NULL ;
		}										
	}

	//求芯轨绘制中心点
	strArrayDrag.RemoveAll() ;
	dcData.GetDrag(strArrayDrag, 1) ;

	AcGePoint2d pt_2d_xg(0,0) ; //芯轨绘制中心点
	AcGeLineSeg2d lineseg2d_xg1(pt_2d_ext1 , pt_2d_ext2) ;
	AcGePoint3d pt_3d_xg ;
	this->GetXgInsertPt(nHandleDc, iDcDirectV, pt_3d_xg) ;	
	pt_2d_xg = pt_3d_xg.convert2d(AcGePlane::kXYPlane) ;
	//第几动（对于交流直捅到楼的，即使多动也各自按一动处理）
	strActionInfo = _T("") ;
	this->GetDcActionInfo(dcData.m_strXG_QyType, dcData.GetActionNum(), dcData.m_nActIndex, strActionInfo) ;
	for (int i=0; i<dcData.m_nXG_QyNum; i++)
	{
		double dX_Xg = pt_2d_xg.x+(iDcDirectH==0?-1:1)*dcData.m_nXG_QyNum*3/2+(iDcDirectH==0?1:-1)*1.5+i*3*(iDcDirectH==0?1:-1) ;
		AcGePoint2d pt_2d_zzj(dX_Xg, pt_2d_xg.y) ;
		pt_2d_zzj.rotateBy(vecDw.angle(), pt_2d_xg) ;
		AcGePoint3d pt_3d_zzj(pt_2d_zzj.x, pt_2d_zzj.y, 0) ;
		CStringArray strArrayVal ;
		CString strVal ;
		strVal.Format(_T("X%d"), i+1) ;
		strArrayVal.Add(strVal) ;
		strArrayVal.Add(strArrayDrag.GetAt(i)) ;
		strArrayVal.Add(strActionInfo) ;
		if (m_mysys.InsertBlock(pRefZzj, strBlkNameZzj, pt_3d_zzj, &strArrayVal, FALSE))
		{
			m_mysys.RotationEnt(pRefZzj, vecDw.angle()+(iUpOrDownDc==1?PI:0), pt_3d_zzj) ;
			m_mysys.SetXdata(pRefZzj, _T("IS_JXG"), 2) ;
			m_mysys.SetXdata(pRefZzj, _T("BELONG_ENT"), nHandleDc) ;
			//m_mysys.SetXdata(pRefZzj, _T("ACTION_INFO"), strActionInfo) ;
			//绘制对应的箱盒
			this->DrawBoxZzj(pRefZzj, &dcData, i, iDcDirectH) ;
			pRefZzj->close() ;
			pRefZzj = NULL ;
		}										
	}
	//end draw zzj
	m_pDb->setClayer(ojbIdLayerOld) ; //恢复当前图层
}



// 清除指定道岔的转辙机
void CZnSxt::ClearZzj(long nHandleDc)
{
	CLongArray nArrayHandle ;
	this->FindEntBelong(nHandleDc, nArrayHandle) ;
	for (int i=0; i<nArrayHandle.GetCount(); i++)
	{
		long nHandle = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForWrite))
		{
			long nHandleBelong = 0 ;
			m_mysys.GetXdata(pEnt, _T("BELONG_ENT"), nHandleBelong) ;
			if (nHandleBelong==nHandleDc)
			{
				pEnt->erase() ;
			}
			pEnt->close() ;
		}		
	}
}

// 绘制所有道岔的转辙机
void CZnSxt::DrawZzj(void)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type=0")) ; 
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				this->DrawZzj(nHandle) ;
				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in DrawZzj"));
	}
}

// 取得芯轨绘制中心点
int CZnSxt::GetXgInsertPt(long nHandleDc, int iDcDirectV, AcGePoint3d &pt_3d_ret)
{
	int iRet = 0 ;

	CString strSql ;
	strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleDc) ;
	long nHandleDW = 0 ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_online_handle"), nHandleDW) ;
	long nHandleFW = 0 ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_fwline_handle"), nHandleFW) ;
	CString strDcName ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strDcName) ;

	long nHandleDwSxt = 0, nHandleFwSxt = 0 ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0, _T("LWPOLYLINE"), 1001, _T("CloneFrom"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		long nHandle = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			long nHandleCloneFrom = 0 ;
			m_mysys.GetXdata(pEnt, _T("CloneFrom"), nHandleCloneFrom) ;
			int iUpOrDownTmp = -1 ;
			m_mysys.GetXdata(pEnt, _T("UPDOWN"), iUpOrDownTmp) ;

			if (nHandleCloneFrom==nHandleDW)
			{
				if (iDcDirectV==iUpOrDownTmp)
				{
					nHandleDwSxt = nHandle ;
				}				
			}
			else if (nHandleCloneFrom==nHandleFW)
			{
				if (iDcDirectV!=iUpOrDownTmp)
				{
					nHandleFwSxt = nHandle ;
				}
			}
			pEnt->close() ;
		}		
	}

	if (nHandleDwSxt!=0&&nHandleFwSxt!=0)
	{
		AcDbEntity* pEnt1 = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandleDwSxt, pEnt1, AcDb::kForRead))
		{
			AcDbEntity* pEnt2 = NULL ;
			if (m_mysys.OpenAcDbEntity(nHandleFwSxt, pEnt2, AcDb::kForRead))
			{
				AcGePoint3dArray pt3dArray ;
				pEnt2->intersectWith(pEnt1, AcDb::kOnBothOperands, pt3dArray) ;
				iRet = pt3dArray.length() ;
				if (iRet==1)
				{
					pt_3d_ret = pt3dArray.at(0) ;
				}

				pEnt2->close() ;
			}

			pEnt1->close() ;
		}
	}

	if (iRet!=1)
	{
		acutPrintf(_T("\n求道岔%s的双线中芯轨绘制点时,交点为%d"), strDcName, iRet) ;
	}

	return iRet;
}

// 绘制信号机箱盒
void CZnSxt::DrawBoxXhj(long nHandleXhj)
{
// 	if (nHandleXhj==21172)
// 	{
// 		int ii = 0 ;
// 	}
	this->ClearEnt(nHandleXhj, 1) ;
	long nHandleCloneXhj = 0 ; 
	if (this->GetCloneXhj(nHandleXhj, nHandleCloneXhj)==0)
	{
		return ;
	}

	//保存当前图层，设置要绘制的图层
	AcDbObjectId ojbIdLayerOld = m_pDb->clayer() ;
	m_mysys.SetCLayer(_T("CRSC_SJY_XTS_SXT_DLJL_XHJ"), 6) ;
	m_iColorIndexCur = 256 ;


	CString strSql ;
	strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleXhj) ;
	CString strXhjBlkName, strDevName ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_block"), strXhjBlkName) ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_name"), strDevName) ;
	CStringArray strArrayXhjBlkName ;
	CBlkUtility::DivideString(strXhjBlkName, _T("_"), strArrayXhjBlkName) ;
	CString strXhjType, strLightUnit, strIndicatorNum, strZorF, strGorA ;

	if (strArrayXhjBlkName.GetCount()==8) //有前面导入时的筛选，这步应能确保是标准块
	{
		strXhjType = strArrayXhjBlkName.GetAt(1) ;
		strLightUnit = strArrayXhjBlkName.GetAt(2) ;
		strIndicatorNum = strArrayXhjBlkName.GetAt(3) ;
		strZorF = strArrayXhjBlkName.GetAt(5) ;
		strGorA = strArrayXhjBlkName.GetAt(6) ;		
	}
	AcGePoint3d pt_3d_clonexhj ;
	double dAngleCloneXhj = 0 ;
	double dOffsetBox = 9 ;
	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandleCloneXhj, pEnt, AcDb::kForRead))
	{
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		AcDbExtents extCloneXhj ;
		pRef->getGeomExtents(extCloneXhj) ;
		if (extCloneXhj.maxPoint().y-extCloneXhj.minPoint().y>6.5)
		{
			dOffsetBox = 13 ;
		}
		pt_3d_clonexhj = pRef->position() ;
		dAngleCloneXhj = pRef->rotation() ;
		pEnt->close() ;
	}

	//箱盒类型
	CString strBoxType, strBoxType1, strBoxType2 ;
	//default
	CString strDefaultBox ; //从数据取的该信号机默认箱盒列表（取第一个做默认值）
	strSql.Format(_T("select * from define_default_xhj where block_name='%s'"), strXhjBlkName) ;
	m_RecordSet.GetRecordValue(strSql, _T("box_type"), strDefaultBox) ;
	int iLoc1 = strDefaultBox.Find(_T(";")) ;
	strBoxType = (iLoc1==-1?strDefaultBox:strDefaultBox.Left(iLoc1)) ;
	int iLoc2 = strBoxType.Find(_T("-")) ;
	if (iLoc2!=-1)
	{
		strBoxType1 = strBoxType.Left(iLoc2) ;
		strBoxType2 =strBoxType.Right(strBoxType.GetLength()-iLoc2-1) ;
	}
	else
	{
		strBoxType1 = strBoxType ;
	}

	//是否已实际设置
	CString strBoxTypeSet ;
	if (m_mysys.GetXdata(nHandleXhj, _T("BOXTYPE"), strBoxTypeSet))
	{
		int iLoc3 = strBoxTypeSet.Find(_T("-")) ;
		if (iLoc3!=-1)
		{
			strBoxType1 = strBoxTypeSet.Left(iLoc3) ;
			strBoxType2 =strBoxTypeSet.Right(strBoxTypeSet.GetLength()-iLoc3-1) ;
		}
		else
		{
			strBoxType1 = strBoxTypeSet ;
			strBoxType2 = _T("") ;
		}
	}

	AcGePoint3d pt_3d_insert = pt_3d_clonexhj ;
	pt_3d_insert.y = pt_3d_clonexhj.y+(strZorF==_T("Z")?dOffsetBox:-dOffsetBox) ;
	CString strBlkName ;
	CStringArray strArrayAtt ;

	AcDbBlockReference* pRef = NULL ;	

	//XB1
	long nHandleXB1 = 0 ;
	strBlkName.Format(_T("BOX_%s_0"), strBoxType1.Left(2)) ;
	CString strAtt ;
	if (strBoxType1.Left(2)==_T("XB"))
	{
		strAtt = strBoxType1 ;
	}
	else if (strBoxType1.Left(2)==_T("HZ"))
	{
		strAtt = strBoxType1.Right((strBoxType1.GetLength()>2?strBoxType1.GetLength():2)-2) ;
	}
	strArrayAtt.Add(strAtt) ;
	if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_insert, &strArrayAtt, FALSE ))
	{
		//增加属性盒子编号，以便做径路图
		if (!strBoxType2.IsEmpty())
		{
			AcGePoint3d pt_3d_att = pt_3d_insert ;
			pt_3d_att.y+=2.6 ;
			m_mysys.AddAttValue(pRef, _T("BOXINDEX"), _T("II"), pt_3d_att, 0, FALSE, 1, 1, 0, 256 ) ;
		}
		m_mysys.RotationEnt(pRef, dAngleCloneXhj, pt_3d_clonexhj) ;
		m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleXhj) ;
		m_mysys.SetXdata(pRef, _T("BOX_INDEX"), 1) ;
		m_mysys.SetXdata(pRef, _T("BOX_SUM"), strBoxType2.IsEmpty()?1:2) ;

		nHandleXB1 = m_mysys.GetHandle(pRef) ;
		pRef->close() ;
	}
	if (!strBoxType2.IsEmpty())
	{
		strArrayAtt.RemoveAll() ;
		strAtt = _T("") ;
		if (strBoxType2.Left(2)==_T("XB"))
		{
			strAtt = strBoxType2 ;
		}
		else if (strBoxType2.Left(2)==_T("HZ"))
		{
			strAtt = strBoxType2.Right((strBoxType2.GetLength()>2?strBoxType2.GetLength():2)-2) ;
		}
		strArrayAtt.Add(strAtt) ;
		AcGePoint3d pt_3d_insert2 = pt_3d_insert ;
		pt_3d_insert2.x+=15 ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_insert2, &strArrayAtt, FALSE ))
		{
			//增加属性盒子编号，以便做径路图
			AcGePoint3d pt_3d_att = pt_3d_insert2 ;
			pt_3d_att.y+=2.6 ;
			m_mysys.AddAttValue(pRef, _T("BOXINDEX"), _T("I"), pt_3d_att, 0, FALSE, 1, 1, 0, 256 ) ;

			m_mysys.RotationEnt(pRef, dAngleCloneXhj, pt_3d_clonexhj) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleXhj) ;
			m_mysys.SetXdata(pRef, _T("BOX_INDEX"), 2) ;
			m_mysys.SetXdata(pRef, _T("BOX_SUM"), 2) ;
			pRef->close() ;
		}
		//连线
		AcGePoint3d pt_3d_start = pt_3d_insert ;
		pt_3d_start.x+=2.5 ;
		AcGePoint3d pt_3d_end = pt_3d_start ;
		pt_3d_end.x+=10 ;
		AcDbObjectId objIdLine = m_mysys.AddAcDbPolyLine(pt_3d_start, pt_3d_end, 0, 0, 256) ;
		long nHandleCable = 0 ;
		nHandleCable = m_mysys.GetHandle(objIdLine) ;
		m_mysys.SetXdata(objIdLine, _T("BELONG_ENT"), nHandleXB1) ;
		m_mysys.SetXdata(nHandleCable, _T("IS_CABLE"), 1) ;
		m_mysys.RotationEnt(objIdLine, dAngleCloneXhj, pt_3d_clonexhj) ;
		//电缆信息
		CString strDevType ;
		strDevType.Format(_T("XHJB_%s_%s_%s-%s"), strArrayXhjBlkName.GetAt(2), strArrayXhjBlkName.GetAt(4), strBoxType1, strBoxType2) ;
		vector<CCableCore> vec ;
		int iNumCore=0, iBackup = 0 ;
		CBoxNodeEnt boxNode(strBoxType1, strDevName, strDevType, 1) ;
		boxNode.m_iEntType = 1 ;
// 		boxNodeXhj.m_strDevType = strDevType ;
// 		boxNodeXhj.m_strDevName = strDevName ;
		iNumCore = this->GetDevCore(boxNode) ;		
		CString strCableTypeName ;
		this->GetUseCableType(iNumCore, 0, iBackup, strCableTypeName, 1 ) ;
		CString strCableInfo ;
		strCableInfo.Format(_T("10-%s(%d)"), strCableTypeName, iBackup) ;

		this->DrawCableString(nHandleCable, strCableInfo) ;
		CString strAppNames[7] = {_T("DIST1"), _T("DIST2"), _T("CROSSGD"),  _T("BOXNUM"), _T("PLUS"), _T("BOXFNUM"), _T("INTOXHL")} ;
		long iDatas[7] = {0, 0, 0, 2, 0, 0, 0} ;
		for (int i=0; i<7; i++)
		{
			m_mysys.SetXdata(nHandleCable, strAppNames[i], iDatas[i]) ;
		}	
	}
	m_pDb->setClayer(ojbIdLayerOld) ; //恢复当前图层
}

// 绘制所有信号机盒子
void CZnSxt::DrawBoxXhj(void)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=1 ")) ; //整除为1的
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				this->DrawBoxXhj(nHandle) ;
				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in DrawBoxXhj"));
	}
}

// 绘制ZZJ箱盒
void CZnSxt::DrawBoxZzj(AcDbBlockReference* pRefJxg, const CDcDATA* pDataDc, int iQyIndex, int iDcDirectH)
{
	if (pRefJxg!=NULL)
	{ 	
		CStringArray strArrayAtt ;
		CString strBlkName = _T("BOX_HZ_0") ;
		double dOffsetBox = 1.0 ;

		long iIsJxg = 1 ;
		m_mysys.GetXdata(pRefJxg, _T("IS_JXG"), iIsJxg) ;
		CString strQyType = (iIsJxg==1?pDataDc->m_strJG_QyType:pDataDc->m_strXG_QyType);
		int nQyNum = (iIsJxg==1?pDataDc->m_nJG_QyNum:pDataDc->m_nXG_QyNum);

		long nHandleCurJxg = m_mysys.GetHandle(pRefJxg) ;

		double dAngle = pRefJxg->rotation() ;
		AcGePoint3d pt_3d_ref = pRefJxg->position() ;
		AcGePoint3d pt_3d_box ;
		m_mysys.GetPoint(pt_3d_ref, 15, dAngle+PI/2, pt_3d_box) ;

		long nHandleBox = 0 ; //记录绘制的箱盒的handle，后面连线的belong设置会用

		double dAngleFmod = fmod(dAngle, PI) ;
		if (fabs(dAngleFmod-PI)<0.1) //3.14、3.15角度基本一样
		{
			dAngleFmod = 0 ;
		}

		//大于1个盒子则间距设为dOffsetBox
		if (nQyNum>1)
		{
			AcGePoint3d pt_3d_boxTmp = pt_3d_box ;
			int iQyIndexReverse = (iDcDirectH==0?iQyIndex:(nQyNum-iQyIndex-1)) ;


			if (iQyIndexReverse<(nQyNum/2)) //统一左移
			{
				double dTmp = dOffsetBox*(nQyNum/2-iQyIndexReverse-(nQyNum%2==0?0.5:0)) ;
				m_mysys.GetPoint(pt_3d_box, dTmp, dAngleFmod+PI, pt_3d_boxTmp) ;					
			}
			else if (iQyIndexReverse>(nQyNum/2-1+nQyNum%2))//统一右移
			{
				double dTmp = dOffsetBox*(iQyIndexReverse-nQyNum/2+(nQyNum%2==0?0.5:0)) ;
				m_mysys.GetPoint(pt_3d_box, dTmp, dAngleFmod, pt_3d_boxTmp) ;
			}
			pt_3d_box = pt_3d_boxTmp ;
		}

		strArrayAtt.RemoveAll() ;
		strArrayAtt.Add(pDataDc->m_strBoxType.Mid(2)) ;
		AcDbBlockReference* pRef = NULL ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_box, &strArrayAtt, FALSE ))
		{
			//m_mysys.SetXdata(pRef, _T("BELONG_DC"), nHandleDc) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleCurJxg) ;
			nHandleBox = m_mysys.GetHandle(pRef) ;
			pRef->close() ;
			pRef = NULL ;
		}

		//若>1机牵引，且串联，先预先把这几机盒子连起来
		if (iQyIndex>0)
		{
			CString strCableString ;
			CString strJxgType ;

			if (strQyType==_T("ZD6-EJ"))
			{
				if (m_mysys.GetAttValue(pRefJxg, _T("TYPE"), strJxgType)&&strJxgType==_T("ZD6-J"))
				{
					strCableString = _T("15-14(3)") ;
					if (m_iQkNum==1)
					{
						strCableString = _T("15-16(3)") ;
					}
				}
			}
			else if (strQyType.Left(8)==_T("ZYJ7+SH6"))
			{
				if (m_mysys.GetAttValue(pRefJxg, _T("TYPE"), strJxgType)&&strJxgType==_T("SH6"))
				{
					strCableString = _T("15-16(4)") ;
					if (m_iQkNum==1)
					{
						strCableString = _T("15-16(2)") ;
					}
				}
			}

			if (!strCableString.IsEmpty())
			{
				AcGePoint3d pt_3d_start, pt_3d_end ;
				m_mysys.GetPoint(pt_3d_box, iDcDirectH==1?1.5:-1.5, dAngleFmod, pt_3d_start) ;
				m_mysys.GetPoint(pt_3d_box, iDcDirectH==1?2.5:-2.5,  dAngleFmod, pt_3d_end) ;
				AcDbObjectId ojbIdLine = m_mysys.AddAcDbPolyLine(pt_3d_start, pt_3d_end) ;
				m_mysys.SetXdata(ojbIdLine, _T("BELONG_ENT"),  nHandleBox) ;
				long nHandleLine = m_mysys.GetHandle(ojbIdLine) ;
				this->DrawCableString(nHandleLine, strCableString) ;

				CString strAppNames[8] = {_T("IS_CABLE"), _T("DIST1"), _T("DIST2"), _T("CROSSGD"),  _T("BOXNUM"), _T("PLUS"), _T("BOXFNUM"), _T("INTOXHL")} ;
				long iDatas[8] = {1, 0, 0, 0, 2, 0, 0, 0} ;
				for (int i=0; i<8; i++)
				{
					m_mysys.SetXdata(nHandleLine, strAppNames[i], iDatas[i]) ;
				}
			}            
		}
	}
}

// 取得clone信号机
int CZnSxt::GetCloneXhj(long nHandleXhj, long& nHandleCloneXhj)
{
	int iRet = 0 ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0, _T("INSERT"), 1001, _T("CloneFrom"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		long nHandle = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			long nHandleCloneFrom = 0 ;
			m_mysys.GetXdata(pEnt, _T("CloneFrom"), nHandleCloneFrom) ;
			if (nHandleCloneFrom==nHandleXhj)
			{
				nHandleCloneXhj = nHandle ;
				iRet++ ;
			}
			pEnt->close() ;
		}		
	}
	return iRet;
}

// 计算并绘制电缆信息
void CZnSxt::ComputeCableInfo(long nHandleBoxStart, long nHandleBoxEnd, long nHandleCable,  int iJigType)
{
// 	if (nHandleBoxStart==148096)
// 	{
// 		int iii = 0 ;
// 	}
	long nHandleCableString = 0 ; //电缆（箱盒之间的多段线）对应的文本
	this->GetCableString(nHandleCable, nHandleCableString, 0) ;
	CString strCableString ;
	if (nHandleCableString!=0)
	{
		m_mysys.GetTextString(nHandleCableString, strCableString) ;
	}

	long nHandleCableName = 0 ;
	this->GetCableString(nHandleCable, nHandleCableName, 2) ;
	CString strCableName ;
	if (nHandleCableName!=0)
	{
		m_mysys.GetTextString(nHandleCableName, strCableName) ;
	}

	long nHandleCableSn = 0 ;
	this->GetCableString(nHandleCable, nHandleCableSn, 3) ;
	CString strCableSn ;
	if (nHandleCableSn!=0)
	{
		m_mysys.GetTextString(nHandleCableSn, strCableSn) ;
	}

	CCableInfoDlg dlg ;

	//2个连接盒子距信号楼的距离
	long iDIST1=0, iDIST2=0, iDIST=0 ;
// 	if (!m_mysys.GetXdata(nHandleCable, _T("DIST1"), iDIST1))
// 	{
		iDIST1 = this->GetBoxDistToXhl(nHandleBoxStart) ;
// 	}
// 	if (!m_mysys.GetXdata(nHandleCable, _T("DIST2"), iDIST2))
// 	{
		iDIST2 = this->GetBoxDistToXhl(nHandleBoxEnd) ;
// 	}
	iDIST = abs(iDIST1-iDIST2) ;
	dlg.m_nArrayData.Add(iDIST1) ;
	dlg.m_nArrayData.Add(iDIST2) ;

	//过股
	long iNumCrossGd = 0 ;
// 	if (!m_mysys.GetXdata(nHandleCable, _T("CROSSGD"), iNumCrossGd))
// 	{
		CLongArray nArrayHandleGd ;
		iNumCrossGd = this->GetCrossGd(nHandleCable, nArrayHandleGd) ;

		//其他
		long iIsXhlCable = 0 ;
		if (m_mysys.GetXdata(nHandleBoxEnd, _T("IS_XHLCABLE"), iIsXhlCable))
		{
			long iNumCrossGd2 = 0  ;
			if (m_mysys.GetXdata(nHandleBoxEnd, _T("CROSSGD_CABLE"), iNumCrossGd2))
			{
				iNumCrossGd+=iNumCrossGd2 ;
			}
		}
// 	}
	dlg.m_nArrayData.Add(iNumCrossGd) ;

	//箱盒
	long iNumBox = 2 ;
// 	if (!m_mysys.GetXdata(nHandleCable, _T("BOXNUM"), iNumBox))
// 	{		
// 		long iIsXhlCable = 0 ;
// 		if (m_mysys.GetXdata(nHandleBoxEnd, _T("IS_XHLCABLE"), iIsXhlCable))
		if (iIsXhlCable>0)
		{
			iNumBox =1 ;
		}
// 	}
	dlg.m_nArrayData.Add(iNumBox) ;

	//修正
	long iNumPlus = 0 ;
	m_mysys.GetXdata(nHandleCable, _T("PLUS"), iNumPlus) ;
	dlg.m_nArrayData.Add(iNumPlus) ;

	//进楼
	long iIntoXhl = 0 ;
// 	if (!m_mysys.GetXdata(nHandleCable, _T("INTOXHL"), iIntoXhl))
// 	{
// 		long iIsXhlCable = 0 ;
// 		if (m_mysys.GetXdata(nHandleBoxEnd, _T("IS_XHLCABLE"), iIsXhlCable))
		if (iIsXhlCable>0)
		{
			long iIntoXhlTmp = 0 ;
			if(m_mysys.GetXdata(nHandleBoxEnd, _T("INTOXHL_CABLE"), iIntoXhlTmp) )
			{
				iIntoXhl = iIntoXhlTmp ;
			}			
		}
// 	}
	dlg.m_nArrayData.Add(iIntoXhl) ;

	//接续盒 ，接续本来要放到进楼前面，但是接续要推算，故放到最后，但是在数组里面插入位置仍在进楼前
	long iNumBoxF = 0 ;
// 	if (!m_mysys.GetXdata(nHandleCable, _T("BOXFNUM"), iNumBoxF))
// 	{
		int iLen1 =(iDIST+iNumCrossGd*dlg.m_iPerGd+(iNumCrossGd>1?(iNumCrossGd-1)*dlg.m_iPerBetweenGd:0)+iNumBox*dlg.m_iPerBox+iNumPlus+iIntoXhl) * dlg.m_dCoefficient ;
		int iNumBoxF1 = iLen1/dlg.m_iPerBetweenBoxF ;
		iNumBoxF = iNumBoxF1 ;
		int iLen2 = (iDIST+iNumCrossGd*dlg.m_iPerGd+(iNumCrossGd>1?(iNumCrossGd-1)*dlg.m_iPerBetweenGd:0)+iNumBox*dlg.m_iPerBox+iNumPlus+iNumBoxF1*dlg.m_iPerBoxF+iIntoXhl) * dlg.m_dCoefficient ;
		int iNumBoxF2 = iLen2/dlg.m_iPerBetweenBoxF ;
		if (iNumBoxF2>iNumBoxF1)
		{
			iNumBoxF = iNumBoxF2 ;
		}
// 	}
	dlg.m_nArrayData.InsertAt(5, iNumBoxF) ;


	//计算此盒子容纳的电缆芯线
// 	m_iCurDljlType = -1 ; //reset当前进行的径路类型 -1：通用（混合），0：道岔，1，信号机，2：轨道电路, 3:电码化

	sequential_tree<CBoxNodeEnt> cableBoxTree ;
	this->BuildCableBoxTree(nHandleBoxStart, nHandleCable, cableBoxTree) ;
	long iLenForPlus =0 ;
	if (!m_mysys.GetXdata(nHandleCable, _T("LENFORPLUS"), iLenForPlus))
	{
		int iLen1 = (abs(iDIST1)+iNumCrossGd*dlg.m_iPerGd+(iNumCrossGd>1?(iNumCrossGd-1)*dlg.m_iPerBetweenGd:0)+iNumBox*dlg.m_iPerBox+iNumPlus+iNumBoxF*dlg.m_iPerBoxF+iIntoXhl) * dlg.m_dCoefficient ;
		iLenForPlus = (iLen1/10)*10+((iLen1%10)<5?5:10) ; //此处是用以初步计算转辙机加芯，该距离是当前距信号楼距离，不是前后箱盒距离
	}
	dlg.m_iLenForPlus = iLenForPlus ;
	//CString strCableTypeAndBackup ;

	CString strCableTypeName ;
	int iBackup = 0 ;

	int iNumCore = this->GetCableCoreNum(cableBoxTree, strCableTypeName, iBackup, iLenForPlus) ; 
// 	int iFind1 = strCableTypeAndBackup.Find(_T("(")) ;
// 	int iFind2 = strCableTypeAndBackup.Find(_T(")")) ;
// 	if (iFind1!=-1)
// 	{
		CBoxNodeEnt boxNode ;
		this->GetBoxInfo(nHandleBoxStart, boxNode) ;
		int iLen3 = (abs(iDIST)+iNumCrossGd*dlg.m_iPerGd+(iNumCrossGd>1?(iNumCrossGd-1)*dlg.m_iPerBetweenGd:0)+iNumBox*dlg.m_iPerBox+iNumPlus+iNumBoxF*dlg.m_iPerBoxF+iIntoXhl) * dlg.m_dCoefficient ;
		int iLenBetweenBox = (iLen3/10)*10+((iLen3%10)<5?5:10) ; //前后箱盒距离

// 		strCableTypeName = strCableTypeAndBackup.Left(iFind1) ;
		if (boxNode.m_iEntType==-1||iLenBetweenBox>400)
		{
			strCableTypeName.Append(_T("L")) ;
		}
// 		iBackup = _ttoi(strCableTypeAndBackup.Mid(iFind1+1, iFind2-iFind1-1)) ;
// 	}	
//     //访问该树
// 	 sequential_tree<CBoxNodeEnt> *pTree=cableBoxTree ;
// 	 while(!pTree)
// 	 {
// 		 CBoxNodeEnt *pTemp=pTree->node_begin();
// 
// 	 }
	dlg.m_strCableOld = strCableString ;
	dlg.m_strCableIntoXhlName = strCableName ;
	dlg.m_strCableSn = strCableSn ;
	dlg.m_strCableTypeName = strCableTypeName ;
	dlg.m_nBackup = iBackup ;
	dlg.m_grid.m_vecCableCore =cableBoxTree.get()->m_vecCableCore ;
	
	if(dlg.DoModal()==IDOK)
	{
		this->SetCableString(nHandleCable, nHandleCableString, dlg.m_strCableAll, 0) ;
		if (iJigType!=2)
		{
			this->SetCableString(nHandleCable, nHandleCableSn, dlg.m_strCableSn, 1) ;
		}
		else if (iJigType==2) // 0，箱盒之间的电缆；1，信号楼主电缆；2，起始连接盒子，末端连在信号楼主电缆
		{
			this->SetCableName(nHandleCable, nHandleCableName, dlg.m_strCableIntoXhlName) ;
		}
		CString strAppNames[8] = {_T("DIST1"), _T("DIST2"), _T("CROSSGD"),  _T("BOXNUM"), _T("PLUS"), _T("BOXFNUM"), _T("INTOXHL"), _T("LENFORPLUS")} ;
		for (int i=0; i<8; i++)
		{
			int iData = (i==7?dlg.m_iLenForPlus:	dlg.m_nArrayData.GetAt(i) ) ;
			m_mysys.SetXdata(nHandleCable, strAppNames[i], (long)iData) ;
		}		
	}
}

// 通过电缆多线段的handle取得其连接字符串,iType=2时是电缆name,=3时是电缆编号
int CZnSxt::GetCableString(long nHandleCableLine, long& nHandleCableString, int iType)
{
	int iRet = 0 ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	if (iType==3)
	{
		pRb = acutBuildList(RTDXF0,_T("TEXT"), kDxfRegAppName, _T("BELONG_ENT"), kDxfRegAppName, _T("IS_CABLESN"), 0) ;
	}
	else if (iType==2)
	{
		pRb = acutBuildList(RTDXF0,_T("TEXT"), kDxfRegAppName, _T("BELONG_ENT"), kDxfRegAppName, _T("IS_CABLENAME"), 0) ;
	}
	else
	{
		pRb = acutBuildList(RTDXF0,_T("TEXT"), kDxfRegAppName, _T("BELONG_ENT"), kDxfRegAppName, _T("IS_CABLEINFO"), 0) ;
	}
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		long nHandleTmp = 0 ;
		if (m_mysys.GetXdata(nArrayHandle.GetAt(i), _T("BELONG_ENT"), nHandleTmp ))
		{
			if (nHandleTmp == nHandleCableLine)
			{
				nHandleCableString = nArrayHandle.GetAt(i) ;
				//
				iRet++ ;
			}
		}
	}

	return iRet ;
}

// 获取盒子距信号楼距离
int CZnSxt::GetBoxDistToXhl(long nHandleBox)
{
	int iDist = 0 ;

	CString strSql ;
	long nHandleBelong = 0 ; 
	if (m_mysys.GetXdata(nHandleBox, _T("IS_XHLCABLE"), nHandleBelong))
	{
		iDist = 0 ;
	}
	else if(m_mysys.GetXdata(nHandleBox, _T("BELONG_ENT"), nHandleBelong))
	{
		//默认是信号机
		long nHandleTmp = 0 ;
		CString strBoxBelongBlkName ;
		if (m_mysys.GetXdata(nHandleBox, _T("BELONG_JYJ"), nHandleTmp)&&nHandleTmp>0) //送受电
		{
			nHandleBelong = nHandleTmp ;
		}
		else if (m_mysys.GetBlockName(nHandleBelong, strBoxBelongBlkName )&&strBoxBelongBlkName.Left(3)==_T("ZZJ")) //转辙机
		{
			 if(m_mysys.GetXdata(nHandleBelong, _T("BELONG_ENT"), nHandleTmp))
			 {
				 nHandleBelong = nHandleTmp ;
			 }
		}
		strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleBelong) ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_toxhldist"), iDist) ;
	}
	else //分向盒
	{
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandleBox, pEnt,AcDb::kForRead))
		{
			AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
			int iDistToXhl = 0 ;
			CString strDistToXhl ;
			if(m_mysys.GetAttValue(pRef, _T("DIST"), strDistToXhl) )
			{
				CString strReg = _T("^([1-9]+[0-9]*)$") ;
				if (CBlkUtility::IsStrMatch(strReg, strDistToXhl))
				{
					CString strRet ;
					CBlkUtility::RegReplace(strReg, _T("$1"), strDistToXhl, strRet) ;
					iDist = _ttoi(strRet) ;
					
				}
			}			
			pEnt->close() ;
		}
	}
	return abs(iDist) ;
}

// 电缆穿过的轨道
int CZnSxt::GetCrossGd(long nHandleCable, CLongArray& nArrayHandleGd)
{
	int iRet = 0 ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0,_T("LWPOLYLINE"), kDxfRegAppName, _T("CloneFrom"), kDxfRegAppName, _T("UPDOWN"), 0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandleCable, pEnt, AcDb::kForRead))
	{
		AcDbPolyline* pPlineCable = AcDbPolyline::cast(pEnt) ;

		for(int i=0; i<nLen; i++)
		{
			AcDbEntity* pEnt2 = NULL ;
			if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt2, AcDb::kForRead))
			{
				int iUPDOWN = 0 ;
				m_mysys.GetXdata(pEnt2, _T("UPDOWN"), iUPDOWN) ;
				if (iUPDOWN==1)
				{
					AcGePoint3dArray pt3dArray ;
					pPlineCable->intersectWith(pEnt2, AcDb::kOnBothOperands, pt3dArray) ;
					if (pt3dArray.length()>0)
					{
						nArrayHandleGd.Add(nArrayHandle.GetAt(i)) ;
						iRet++;
					}
				}

				pEnt2->close() ;
			}
		}
		pEnt->close() ;
	}

	return iRet;
}

// 设置电缆字符串，若nHandleCableString=0，则需要创建一个文本对象
int CZnSxt::SetCableString(long nHandleCableLine, long nHandleCableString, CString& strCableString, int iStringType)
{
	if (nHandleCableString<1)
	{
		AcDbEntity* pEnt = NULL ;
		if(m_mysys.OpenAcDbEntity(nHandleCableLine, pEnt, AcDb::kForRead))
		{
			this->DrawCableString(nHandleCableLine, strCableString, iStringType) ;		
			pEnt->close() ;
		}
	}
	else
	{
		AcDbEntity* pEnt = NULL ;
		if(m_mysys.OpenAcDbEntity(nHandleCableString, pEnt, AcDb::kForWrite))
		{
			AcDbText* pText = AcDbText::cast(pEnt) ;
			pText->setTextString(strCableString) ;
			pEnt->close() ;
		}
	}
	return 0;
}

// 设置电缆名称,只有如楼端有
int CZnSxt::SetCableName(long nHandleCableLine, long nHandleCableName, CString& strCableName)
{
	if (nHandleCableName<1)
	{
		AcDbEntity* pEnt = NULL ;
		if(m_mysys.OpenAcDbEntity(nHandleCableLine, pEnt, AcDb::kForRead))
		{
			AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
			int iNumVert = pPolyLine->numVerts() ;
			AcGeLineSeg3d lineSeg ;
			pPolyLine->getLineSegAt(iNumVert-2, lineSeg) ;
			AcGeVector3d vec = lineSeg.direction() ;
			AcGePoint3d pt_3d_end = lineSeg.endPoint() ;
			AcGePoint3d pt_3d_text ;
			m_mysys.GetPoint(pt_3d_end, 1.5, vec.angleOnPlane(AcGePlane::kXYPlane), pt_3d_text) ;
			AcDbObjectId objId ;
			if (strCableName.IsEmpty())
			{
				strCableName = _T("XX-XX#") ;
			}
			m_mysys.AddText(objId, strCableName, pt_3d_text, pt_3d_text, AcDb::kTextMid, AcDb::kTextBase, 2, 0.7, vec.angleOnPlane(AcGePlane::kXYPlane)-PI/2, 256) ;
			this->SetLayer(objId, m_iCurDljlType) ;
			m_mysys.SetXdata(objId, _T("IS_CABLENAME"), 1) ;
			m_mysys.SetXdata(objId, _T("BELONG_ENT"), nHandleCableLine) ;
			pEnt->close() ;
		}
	}
	else
	{
		AcDbEntity* pEnt = NULL ;
		if(m_mysys.OpenAcDbEntity(nHandleCableName, pEnt, AcDb::kForWrite))
		{
			AcDbText* pText = AcDbText::cast(pEnt) ;
			pText->setTextString(strCableName) ;
			pEnt->close() ;
		}
	}
	return 0;
}

// 从信号楼出来的主电缆，各分向盒出来的电缆连上次电缆等同于连到信号楼
void CZnSxt::ComputeCableInfo(long nHandleCable)
{
	CAcModuleResourceOverride myResources ; 

	CLongArray nArrayHandleGd ;
	int iNumCrossGd = this->GetCrossGd(nHandleCable, nArrayHandleGd) ;

	CXhlCableInfoDlg dlg ;
	dlg.m_iCrossGd = iNumCrossGd ;
	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandleCable, pEnt, AcDb::kForRead))
	{
		dlg.m_pEnt = pEnt ;
        dlg.DoModal() ;

		pEnt->close() ;
	}
}

// 读取径路图到数据库
void CZnSxt::ReadJltToDb(void)
{
	CString strSql ;

	m_AdoDb.ClearTable(_T("cablenet_ent")) ;

	CLongArray nArrayHandleCable[2] ;
	this->GetCable(nArrayHandleCable) ;

	int nSumCable = 0 ;
	nSumCable = nArrayHandleCable[1].GetCount() ;
	acutPrintf(_T("\n电缆:%d根"), nSumCable) ;

	int nSumXhlCable = 0 ;
	nSumXhlCable = nArrayHandleCable[0].GetCount() ;
	acutPrintf(_T("\n信号楼电缆:%d根"), nSumXhlCable) ;

	for (int i=0; i<nSumXhlCable; i++)
	{
		AcDbEntity* pEntXhlCable = NULL ;
		long nHandleXhlCable = nArrayHandleCable[0].GetAt(i) ;
		if (m_mysys.OpenAcDbEntity(nHandleXhlCable, pEntXhlCable, AcDb::kForRead))
		{
			AcDbPolyline* pPolyLineXhlCable = AcDbPolyline::cast(pEntXhlCable) ;
			AcGePoint3d pt_3d_start_xhlcable, pt_3d_end_xhlcable ;
			pPolyLineXhlCable->getStartPoint(pt_3d_start_xhlcable) ;
			pPolyLineXhlCable->getEndPoint(pt_3d_end_xhlcable) ;
			//acutPrintf(_T("\n主缆线：%d, end_point(%.3f, %.3f)"),nHandleXhlCable, pt_3d_end_xhlcable.x, pt_3d_end_xhlcable.y) ;
			int iIsXhlLeftOrRight = (pt_3d_end_xhlcable.x>pt_3d_start_xhlcable.x?1:0) ; //0:左侧,1:右侧;
			CString strMsg ;
			strMsg.Format(_T("读取第%d根主电缆槽相连的电缆..."), i+1) ;
			acedSetStatusBarProgressMeter(strMsg, 0, nSumCable) ;	
			for (int j=0; j<nSumCable; j++)
			{
				acedSetStatusBarProgressMeterPos(j) ;

				AcDbEntity* pEntCable = NULL ;
				long nHandleCable = nArrayHandleCable[1].GetAt(j) ;
				if (m_mysys.OpenAcDbEntity(nHandleCable, pEntCable, AcDb::kForRead))
				{
					AcDbPolyline* pPolyLineCable = AcDbPolyline::cast(pEntCable) ;
					AcGePoint3d pt_3d_start_cable, pt_3d_end_cable ;
					pPolyLineCable->getStartPoint(pt_3d_start_cable) ;
					pPolyLineCable->getEndPoint(pt_3d_end_cable) ;
					UINT nVertexIndex_Start = -1, nVertexIndex_End = -1 ;		
					int iIsPtOnPline_Start = -1, iIsPtOnPline_End = -1 ;
					iIsPtOnPline_Start = m_mysys.IsPointOnPolyLine(pPolyLineXhlCable, pt_3d_start_cable, nVertexIndex_Start, 0.25) ;
					iIsPtOnPline_End = m_mysys.IsPointOnPolyLine(pPolyLineXhlCable, pt_3d_end_cable, nVertexIndex_End, 0.25) ;
					if ((iIsPtOnPline_Start!=-1&&iIsPtOnPline_End==-1) || (iIsPtOnPline_Start==-1&&iIsPtOnPline_End!=-1))
					{					
						//acutPrintf(_T("\n连接到主缆的线：%d, start_point(%.3f, %.3f), end_point(%.3f, %.3f)"),nHandleCable, pt_3d_start_cable.x, pt_3d_start_cable.y, pt_3d_end_cable.x, pt_3d_end_cable.y) ;

						CString strCableName ;
						this->GetCableName(nHandleCable, strCableName) ;
						m_strCurNameIntoXhl = strCableName ;
						if (strCableName.GetLength()>1)
						{
							int iTypeJlt = CZnUtility::GetJltType(strCableName.Left(2)) ;
							if (iTypeJlt!=-1)
							{
								m_iCurDljlType = iTypeJlt ;
							}
						}

						CString strBranchIndex = _T("1") ;

						strSql.Format(_T("insert into cablenet_ent(handle_low, devname, dev_for, boxtype, box_block, left_or_right, belong_xhl_handle_low, branch_index) \
										 values(%d,'%s','%s','%s', '%s', %d, %d, '%s')"),\
										 nHandleCable, strCableName, _T("JY1"), _T("XHL"), _T("DZ_XHL_0"), iIsXhlLeftOrRight,  nHandleCable, strBranchIndex) ;
						m_AdoDb.Execute(strSql) ;

						strBranchIndex.AppendFormat(_T("-1")) ;
						this->FindBox(nHandleCable, nHandleCable, nHandleCable, strBranchIndex) ;

						strSql.Format(_T("update cablenet_ent as a, (select left_or_right from cablenet_ent where handle_low=%d) as b set a.left_or_right=b.left_or_right where belong_xhl_handle_low=%d"), nHandleCable, nHandleCable) ;
						m_AdoDb.Execute(strSql) ;
					}
					pEntCable->close() ;
				}
			}
			acedRestoreStatusBar() ;
			pEntXhlCable->close() ;
		}
	}
}

int CZnSxt::GetCableName(long nHandleCable, CString& strCableName)
{
	int iRet = 0 ;

	struct resbuf* pRb ;
	int nSum = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0,_T("TEXT"), kDxfRegAppName, _T("IS_CABLENAME"), kDxfRegAppName, _T("BELONG_ENT"), 0) ;
	nSum = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nSum; i++)
	{
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
		{
			long nHandleCableBelong = 0 ;
			if (m_mysys.GetXdata(pEnt, _T("BELONG_ENT"), nHandleCableBelong) )
			{
				if (nHandleCableBelong==nHandleCable)
				{
					AcDbText* pText = AcDbText::cast(pEnt) ;
					strCableName = pText->textString() ;
					iRet = 1 ;
				}
			}
			pEnt->close() ;
		}
	}

	return iRet ;
}

// 循环查询一条线所连接的盒子(从信号楼往末端查询,需要保证在绘径路图电缆时始终从末端往信号楼方向), 20150728改：默认（从根节点往子节点搜）iDirect=0，表示本段线搜寻方向为从末端向始端
void CZnSxt::FindBox(long nHandlePline, long nHandleNextBox, long nHandleBelongXhl, CString& strBranchIndex)
{
	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandlePline, pEnt, AcDb::kForRead))
	{
		if (pEnt->isKindOf(AcDbPolyline::desc()))
		{
			AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
			AcGePoint3d pt_3d_start, pt_3d_end ;
			pPolyLine->getStartPoint(pt_3d_start) ;
			pPolyLine->getEndPoint(pt_3d_end) ;

			CLongArray nArrayHandleFindBox_Start, nArrayHandleFindBox_End ;
			long nHandleBoxFind = 0 ;

			//查找在pt_3d_start,pt_3d_end处的盒子，返回值：0，未找到;1，找到一个多段线（主电缆），>1，盒子
			int iRetFind_Start =CZnUtility::FindConBox(pt_3d_start, nArrayHandleFindBox_Start) ;
			int iRetFind_End =CZnUtility::FindConBox(pt_3d_end, nArrayHandleFindBox_End) ;
			int iLoc_Start = CBlkUtility::FindLongInArray(nArrayHandleFindBox_Start, nHandleNextBox) ;
			int iLoc_End = CBlkUtility::FindLongInArray(nArrayHandleFindBox_End, nHandleNextBox) ;

			if (nHandleNextBox==nHandleBelongXhl) //连接到主电缆的缆，其一侧连接为主栏，没盒子(此时其信号楼端盒子nHandleNextBox==信号楼端nHandleBelongXhl，其实都是该电缆的nHandlePline)
			{
				if (iRetFind_Start>1&&iRetFind_End==1)
				{
					nHandleBoxFind = nArrayHandleFindBox_Start.GetAt(0) ;
				}
				else if (iRetFind_End>1&&iRetFind_Start==1)
				{
					nHandleBoxFind = nArrayHandleFindBox_End.GetAt(0) ;
				}
				else
				{
					pEnt->close() ;
					acutPrintf(_T("\n连接到主电缆的线缆在线起点或终点（%.3f,%.3f）-（%.3f,%.3f），在非入楼端发现不是1个连接的盒子！"), pt_3d_start.x, pt_3d_start.y ,pt_3d_end.x ,pt_3d_end.y) ;
					return ;
				}
			}
			else
			{
				if (!(iRetFind_Start>1&&iRetFind_End>1))
				{
					pEnt->close() ;
					acutPrintf(_T("\n在线起点和终点（%.3f,%.3f）-（%.3f,%.3f），未同时各发现1个连接的盒子！"), pt_3d_start.x, pt_3d_start.y ,pt_3d_end.x ,pt_3d_end.y) ;
					return ;
				}
				if (iLoc_Start!=-1&&iLoc_End==-1)
				{
					nHandleBoxFind = nArrayHandleFindBox_End.GetAt(0) ;
				}
				else if (iLoc_Start==-1&&iLoc_End!=-1)
				{
					nHandleBoxFind = nArrayHandleFindBox_Start.GetAt(0) ;
				}
				else
				{
					pEnt->close() ;
					acutPrintf(_T("\n在线起点和终点（%.3f,%.3f）-（%.3f,%.3f），未发现合格的连接盒子！"), pt_3d_start.x, pt_3d_start.y ,pt_3d_end.x ,pt_3d_end.y) ;
					return ;
				}
			}

			CBoxNodeEnt boxNode ;
			if (GetBoxInfo(nHandleBoxFind, boxNode ))
			{
				// 			if (nHandleBoxFind==26348)
				// 			{
				// 				int iii = 0 ;
				// 			}
				boxNode.m_nHandleCable = nHandlePline ;
				long nHandleCableString = 0 ; //电缆（箱盒之间的多段线）对应的文本
				this->GetCableString(nHandlePline, nHandleCableString) ;
				CString strCableString ;
				if (nHandleCableString!=0)
				{
					m_mysys.GetTextString(nHandleCableString, strCableString) ;
				}

				//begin 补充绘制电缆编号
				//20151121：肯尼亚非要把电缆编号写到电缆字符串前面
				CString strCableSn ;
				CBlkUtility::RegReplace(_T("^(.*-)?(\\d+-\\d+[A-Z]*\\(\\d+\\)).*$"), _T("$1"), strCableString, strCableSn) ;
				strCableSn.Trim() ;
				if (strCableSn.IsEmpty())
				{
					long nHandleCableSn = 0 ;
					this->GetCableString(nHandlePline, nHandleCableSn, 3) ;
					strCableSn = strBranchIndex.Right(strBranchIndex.GetLength()-2) ;
					strCableSn.Replace(_T('-'), _T('.')) ;
					strCableSn.Insert(0, m_strCurNameIntoXhl.Left(3)) ;
					strCableSn.Append(_T("#")) ; //20151119：迟汶，肯尼亚要求编号加上#
					if (nHandleCableSn==0)
					{
						this->SetCableString(nHandlePline, nHandleCableSn, strCableSn,  1) ;
					}
					else //调试过程发现莫名问题，有一nHandleCableSn为空文本，故增加下面判断处理空文本的语句
					{
						CString strText ;
						if (m_mysys.GetTextString(nHandleCableSn, strText))
						{
							if (strText.IsEmpty())
							{
								m_mysys.SetTextString(nHandleCableSn, strCableSn) ;
							}
							else //记录下来后续要导入数据库
							{
								strCableSn = strText ;
							}					
						}
					}
				}
				//end 补充绘制电缆编号

				//将电缆信息存储起来，导入excel供审核
				if (m_iCurJob==1)
				{
					m_strArrayCableSn.Add(strCableSn) ;
					m_strArrayCableAll.Add(strCableString) ;
					CString strAppNames[7] = {_T("DIST1"), _T("DIST2"), _T("CROSSGD"),  _T("BOXNUM"), _T("PLUS"), _T("BOXFNUM"), _T("INTOXHL")} ;
					m_vecCableData.resize(7) ;
					for (int i=0; i<7; i++)
					{
						long nData = 0 ;
						if (m_mysys.GetXdata(nHandlePline, strAppNames[i], nData) )
						{
							int iData = (int)nData ;
							m_vecCableData[i].push_back(iData) ;
							if (i<2)
							{
								acutPrintf(_T("\n%s:DIST%d-%d"), strCableSn, i+1,iData) ;
							}
						}
					}	
				}

				//查询和盒子相连的其他电缆
				CLongArray nArrayHandleFindCableTmp ;
				int iFindCable = this->FindCable(nHandleBoxFind, nHandlePline, nArrayHandleFindCableTmp) ;
				CLongArray nArrayHandleFindCable ;
				for (int i=0; i<iFindCable; i++)
				{
					long iIsCable = 0 ;
					if(m_mysys.GetXdata(nArrayHandleFindCableTmp.GetAt(i), _T("IS_CABLE"), iIsCable)&&iIsCable==1)
					{
						nArrayHandleFindCable.Add(nArrayHandleFindCableTmp.GetAt(i)) ;
					}
				}
				int iIsStart = 0 ;
				iFindCable = nArrayHandleFindCable.GetCount() ;
				if(iFindCable==0)
				{
					iIsStart = 1 ;
				}

				CString strDevFor = _T("JY1") ;
				if (boxNode.m_iEntType==1)
				{
					if (m_iDsNumXhj==1)
					{
						strDevFor = _T("JY3") ;
					}
					if (boxNode.m_strDevBlock.Mid(4,2)==_T("DC")) //调车
					{
						if (m_iDcDsNum==1)
						{
							strDevFor = _T("JY2") ;
						}
						else if (m_iDcDsNum==2)
						{
							strDevFor = _T("JY3") ;
						}
						else
						{
							strDevFor = _T("JY1") ;
						}
					}
				}
				if (boxNode.m_iEntType==0)
				{
					if (m_iQkNum==1)
					{
						strDevFor = _T("JY4") ;
					}
				}

				CString strSql ;
				strSql.Format(_T("insert into cablenet_ent(handle_low, devname, devtype, dev_block, dev_for, boxtype, box_block, next_ent_handle_low, cablestring, cable_sn, is_start, belong_xhl_handle_low, distance_to_xhl, branch_index, zzj_type, zzj_index, freq) \
								 values(%d, '%s', '%s', '%s', '%s', '%s', '%s', %d, '%s', '%s', %d, %d, %d, '%s', '%s', '%s', '%s')"),\
								 nHandleBoxFind, boxNode.m_strDevName, boxNode.m_strDevType, boxNode.m_strDevBlock, strDevFor, boxNode.m_strBoxType, boxNode.m_strBoxBlock, nHandleNextBox, strCableString, strCableSn,  iIsStart, nHandleBelongXhl, boxNode.m_iDistToXhl, strBranchIndex, boxNode.m_strDragType, boxNode.m_strActIndex, boxNode.m_strFreq) ;
				m_AdoDb.Execute(strSql) ;

				for (int i=0; i<iFindCable; i++)
				{
					CString strBranchIndexTmp ;
					strBranchIndexTmp.Format(_T("%s-%d"), strBranchIndex, i+1 ) ;
					this->FindBox(nArrayHandleFindCable.GetAt(i), nHandleBoxFind, nHandleBelongXhl, strBranchIndexTmp) ;
				}
			}
		}
		pEnt->close() ;
	}
}

// for tree 循环查询一条线所连接的盒子(从信号楼往末端查询,需要保证在绘径路图电缆时始终从末端往信号楼方向) 
void CZnSxt::FindBox(long nHandlePline, long nHandleNextBox, sequential_tree<CBoxNodeEnt>* pSeqTree)
{
	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandlePline, pEnt, AcDb::kForRead))
	{
		if (pEnt->isKindOf(AcDbPolyline::desc()))
		{
			AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
			AcGePoint3d pt_3d_start ;
			pPolyLine->getStartPoint(pt_3d_start) ;
			CLongArray nArrayHandleFindBox ;
			CZnUtility::FindConBox(pt_3d_start, nArrayHandleFindBox) ;
			int iNumFind = nArrayHandleFindBox.GetCount() ;

			int iBoxSum = 0 ;
			for (int iFindIndex=0; iFindIndex<iNumFind; iFindIndex++)
			{
				long nHandleBox = nArrayHandleFindBox.GetAt(iFindIndex) ;

				CBoxNodeEnt boxNode ;
				if (GetBoxInfo(nHandleBox, boxNode ))
				{
					if (iBoxSum==0)
					{
						boxNode.m_nHandleCable = nHandlePline ;
						long nHandleCableString = 0 ;
						if (this->GetCableString(nHandlePline, nHandleCableString) ==1)
						{
							m_mysys.GetTextString(nHandleCableString, boxNode.m_strCableString) ;
						}

						sequential_tree<CBoxNodeEnt>::iterator it = pSeqTree->insert(boxNode) ;
						//查询和盒子相连的其他电缆
						CLongArray nArrayHandleFindCable ;
						int iFindCable = this->FindCable(nHandleBox, nHandlePline, nArrayHandleFindCable) ;
						for (int i=0; i<iFindCable; i++)
						{
							this->FindBox(nArrayHandleFindCable.GetAt(i), nHandleBox, it.node()) ;
						}
					}
					else
					{
						acutPrintf(_T("\n注意：在 %.3f, %.3f 处有不止1个Box！"), pt_3d_start.x, pt_3d_start.y) ;
					}

					iBoxSum++ ;
				}
			}
		}

		pEnt->close() ;
	}
}
// 查找盒子的其他连接电缆
int CZnSxt::FindCable(long nHandleBox, long nHandlePrePline, CLongArray& nArrayHandleFind)
{
	int iRet = 0 ;

	AcDbEntity* pEntBox = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandleBox, pEntBox, AcDb::kForRead))
	{
		if (pEntBox->isKindOf(AcDbBlockReference::desc()))
		{
			AcDbBlockReference* pRef = AcDbBlockReference::cast(pEntBox) ;
			CString strBlkName ;
			m_mysys.GetBlockName(pRef, strBlkName) ;

			AcGePoint3d pt_3d_blk ;
			pt_3d_blk = pRef->position() ;

			AcGePoint3d pt_3d_minmax[2]  ;
			double dBoxWidth = 4 ; //默认是送受电盒子的长度
			if (strBlkName.Left(6)==_T("BOX_XB"))
			{
				dBoxWidth = 5 ;
				for (int i=0; i<2; i++)
				{
					pt_3d_minmax[i].x = pt_3d_blk.x+(i==0?-1:1)*dBoxWidth/2 ;
					pt_3d_minmax[i].y = pt_3d_blk.y+(i==0?-1:1)*2 ;
				}
				iRet = m_mysys.GetConnectLine(pt_3d_minmax[0], pt_3d_minmax[1], nArrayHandleFind, 0.05) ;
			}
			else if (strBlkName.Left(7)==_T("BOX_FSD"))
			{
				dBoxWidth = 4 ;
				for (int i=0; i<2; i++)
				{
					pt_3d_minmax[i].x = pt_3d_blk.x+(i==0?-1:1)*dBoxWidth/2 ;
					pt_3d_minmax[i].y = pt_3d_blk.y+(i==0?-1:1)*1.5 ;
				}
				iRet = m_mysys.GetConnectLine(pt_3d_minmax[0], pt_3d_minmax[1], nArrayHandleFind, 0.05) ;
			}
			else if (strBlkName.Left(6)==_T("BOX_HF"))
			{						
				dBoxWidth = 6 ;
				iRet = m_mysys.GetConnectLine(pt_3d_blk, 3, nArrayHandleFind, 0.05) ;
			}
			else if (strBlkName.Left(6)==_T("BOX_HZ"))
			{						
				dBoxWidth = 3 ;
				iRet = m_mysys.GetConnectLine(pt_3d_blk, 1.5, nArrayHandleFind, 0.05) ;
			}

			for (int i=0; i<iRet; i++)
			{						
				long nHandleFind = nArrayHandleFind.GetAt(i) ;
				if (nHandleFind==nHandlePrePline)
				{
					nArrayHandleFind.RemoveAt(i) ;
					iRet-- ;
					break;
				}
			}

			//20151119:迟汶，排序连接的电缆，以便编号规律
			if (strBlkName.Left(6)==_T("BOX_HF"))
			{						
				//对所有连线排序，主干电缆（两头都是HF的）靠前，逆时针和盒子相交的顺序
				//先逆时针排序所有线，取其和HF相交段的向量方向来判断
				double  dAnglePreLine = 0.000 ;
				dAnglePreLine = this->GetPlineAngle(nHandlePrePline, 0, 0) ;
				vector<ConLineInfo> vec ;
				for (int i=0; i<nArrayHandleFind.GetCount(); i++)
				{
					long nHandle = nArrayHandleFind.GetAt(i) ;
					ConLineInfo info ;
					info.nHandle = nHandle ;
					info.dAngle = this->GetPlineAngle(nHandle, dAnglePreLine, 1) ; //相对preline的逆时针角度（不能用angleTo函数）
					vec.push_back(info) ;
				}
				//sort(vec.begin(),vec.end(),greater());//降序排列
				sort(vec.begin(),vec.end(),less<ConLineInfo>()); //升序排列
				CLongArray nArrayHandleBranchCable ;
				nArrayHandleFind.RemoveAll() ;
				for (int i=0; i<vec.size(); i++)
				{
					long nHandle = vec.at(i).nHandle ;
					//是否是主干电缆,主干排靠前
					AcGePoint3d pt_3d_start ;
					this->GetPlinePoint(nHandle, pt_3d_start, 0) ;
					CLongArray nArrayHandleFindBox_Start ;
					int iRetFind_Start =CZnUtility::FindConBox(pt_3d_start, nArrayHandleFindBox_Start) ;
					if (iRetFind_Start==5) //分向盒 HF
					{
						nArrayHandleFind.Add(nHandle) ;
					}
					else
					{
						nArrayHandleBranchCable.Add(nHandle) ;
					}
				}
				nArrayHandleFind.Append(nArrayHandleBranchCable) ;
			}
		}
		pEntBox->close() ;
	}

	return iRet ;
}

// 绘制电缆网络图
void CZnSxt::DrawCableNet(void)
{
	double dOffSetX = 20 ;
	double dOffSetY = 20 ;
	double dBaseX = 0 ;

	CADORecordset cSet(&m_AdoDb) ;
	CString strSql ;

	strSql.Format(_T("update cablenet_ent  set pt_x=12345, pt_y=12345")) ;
	m_AdoDb.Execute(strSql) ;

	strSql.Format(_T("select * from cablenet_ent where boxtype='XHL'")) ;
	CLongArray nArrayHandleXhl ;
	int iNumXhl = m_RecordSet.GetRecordValue(strSql, _T("handle_low"), nArrayHandleXhl) ;
	double dYTotal = 0 ;
	acedSetStatusBarProgressMeter(_T("计算节点坐标..."), 0, iNumXhl) ;	
	for (int i=0; i<iNumXhl; i++)
	{
		acedSetStatusBarProgressMeterPos(i) ;

		long nHandleXhl = nArrayHandleXhl.GetAt(i) ;

		strSql.Format(_T("select * from cablenet_ent where is_start=1 and belong_xhl_handle_low=%d order by branch_index"), nHandleXhl) ;
		if (cSet.Open(strSql))
		{
			int iRsCount = cSet.GetRecordCount() ;

			int iIndex = 0 ;
			while(!cSet.IsEOF())
			{
				CString strName ;
				cSet.GetFieldValue(_T("devname"), strName) ;

				int iLeftOrRight = 0 ;
				cSet.GetFieldValue(_T("left_or_right"), iLeftOrRight) ;
				CString strBranchIndex ;
				cSet.GetFieldValue(_T("branch_index"), strBranchIndex) ; 
				int iNumMark = strBranchIndex.Replace(_T('-'), _T('_')) ;

				double x = dBaseX+(iLeftOrRight==0?-1:1)*iNumMark*dOffSetX ;
				double y =dYTotal- iIndex*dOffSetY ;
				cSet.Edit() ;
				cSet.SetFieldValue(_T("pt_x"), x) ;
				cSet.SetFieldValue(_T("pt_y"), y) ;
				cSet.Update() ;

				cSet.MoveNext() ;

				iIndex++ ;
			}
			dYTotal-=(iRsCount*dOffSetY) ;
			cSet.Close() ;
		}

		do 
		{
			strSql.Format(_T("select * from cablenet_ent where pt_x=12345 and pt_y=12345 and belong_xhl_handle_low=%d"), nHandleXhl) ;
			if (cSet.Open(strSql))
			{
				int iRsCount = cSet.GetRecordCount() ;
				if (iRsCount==0)
				{
					cSet.Close() ;
					break;
				}

				while(!cSet.IsEOF())
				{
					long nHandle = 0 ;
					cSet.GetFieldValue(_T("handle_low"), nHandle) ;
					int iLeftOrRight = 0 ;
					cSet.GetFieldValue(_T("left_or_right"), iLeftOrRight) ;
					strSql.Format(_T("select * from cablenet_ent where next_ent_handle_low=%d"), nHandle) ;
					int iRsCount1 = m_RecordSet.GetRecordCount(strSql) ;
					strSql.Format(_T("select * from cablenet_ent where next_ent_handle_low=%d and pt_x<>12345 and pt_y<>12345"), nHandle) ;
					int iRsCount2 = m_RecordSet.GetRecordCount(strSql) ;
					if (iRsCount1==iRsCount2)
					{
						strSql.Format(_T("select min(pt_y)+(max(pt_y)-min(pt_y))/2 as cur_y from cablenet_ent where next_ent_handle_low=%d"), nHandle) ;
						double dCurY = 0 ;
						m_RecordSet.GetRecordValue(strSql, _T("cur_y"), dCurY) ;
						strSql.Format(_T("select * from cablenet_ent where next_ent_handle_low=%d"), nHandle) ;
						double dCurX = 0 ;
						m_RecordSet.GetRecordValue(strSql, _T("pt_x"), dCurX) ;
						dCurX+=(iLeftOrRight==0?1:-1)*dOffSetX ;
						cSet.Edit() ;
						cSet.SetFieldValue(_T("pt_y"), dCurY) ;
						cSet.SetFieldValue(_T("pt_x"), dCurX) ;
						cSet.Update() ;
					}

					cSet.MoveNext() ;
				}
				cSet.Close() ;
			}

		} while (TRUE);

		dYTotal-=dOffSetY ;	
	}

	acedRestoreStatusBar() ;

	//绘制网络图
	AcDbDatabase* pDbNew = new AcDbDatabase() ;
	CUserSystem mysys ;
	mysys.m_pDb = pDbNew ;

	strSql.Format(_T("select * from cablenet_ent")) ;
	if (cSet.Open(strSql))
	{
		int iCount = cSet.GetRecordCount() ;
		acedSetStatusBarProgressMeter(_T("绘制网络图..."), 0, iCount) ;
		int iIndex = 0 ;
		while(!cSet.IsEOF())
		{
			acedSetStatusBarProgressMeterPos(iIndex) ;

			long nHandle = 0 ;
			cSet.GetFieldValue(_T("handle_low"), nHandle) ;
			CString strDevName ;
			cSet.GetFieldValue(_T("devname"), strDevName) ;
			CString strDevBlk ;
			cSet.GetFieldValue(_T("dev_block"), strDevBlk) ;
			CString strBoxType ;
			cSet.GetFieldValue(_T("boxtype"), strBoxType) ;
			CString strBoxBlock ;
			cSet.GetFieldValue(_T("box_block"), strBoxBlock) ;
			int iLeftOrRight = 0 ;
			cSet.GetFieldValue(_T("left_or_right"), iLeftOrRight) ;
			int iDistToXhl = 0 ;
			cSet.GetFieldValue(_T("distance_to_xhl"), iDistToXhl) ;
			long nHandleNext = 0 ;
			cSet.GetFieldValue(_T("next_ent_handle_low"), nHandleNext) ;
			CString strBranchIndex ;
			cSet.GetFieldValue(_T("branch_index"), strBranchIndex) ;
			CString strCableString ;
			cSet.GetFieldValue(_T("cablestring"), strCableString) ;
			CString strCableSn ;
			cSet.GetFieldValue(_T("cable_sn"), strCableSn) ;
			CString strFreq ;
			cSet.GetFieldValue(_T("freq"), strFreq) ;

			double x = 0 ;
			double y =0 ;
			cSet.GetFieldValue(_T("pt_x"), x) ;
			cSet.GetFieldValue(_T("pt_y"), y) ;
			CString strName ;
			cSet.GetFieldValue(_T("devname"), strName) ;
			AcGePoint3d pt_3d_box(x,y,0) ;

			CStringArray strArrayVal ;
			AcDbBlockReference* pRef = NULL ;
			double dBoxWidth = 0 ;
			CStringArray strArrayBoxBlock ;
			CBlkUtility::DivideString(strBoxBlock, _T("_"), strArrayBoxBlock) ;
			CString strBoxBlockPart1  ;
			if (strArrayBoxBlock.GetCount()>1)
			{
				strBoxBlockPart1 = strArrayBoxBlock.GetAt(1) ;
			}
			if (strBoxBlockPart1==_T("XHL"))
			{
				if (strDevName.IsEmpty())
				{
					strDevName = _T("XXX") ;
				}
				strArrayVal.Add(strDevName) ;
				if (mysys.InsertBlock(pRef, strBoxBlock, pt_3d_box, &strArrayVal, FALSE))
				{
					if (iLeftOrRight==0)
					{
						mysys.RotationEnt(pRef, PI, pt_3d_box) ;
					}
					pRef->close() ;
				}
			}
			else if (strBoxBlockPart1==_T("HF"))
			{
				dBoxWidth = 6 ;

				strArrayVal.Add(strDevName) ;
				CString strDistToXhl ;
				m_mysys.GetAttValue(nHandle, _T("DIST"), strDistToXhl) ;
				strArrayVal.Add(strDistToXhl) ;
				m_mysys.GetAttValue(nHandle, _T("HFNUM"), strDevName) ;
				CString strHf ;
				m_mysys.GetAttValue(nHandle, _T("HF"), strHf) ;
				strArrayVal.Add(strHf) ;
				if (mysys.InsertBlock(pRef, strBoxBlock, pt_3d_box, &strArrayVal, FALSE))
				{
					if (iLeftOrRight==0)
					{
						AcGePoint3d pt_3d_boxtmp(pt_3d_box.x, pt_3d_box.y+1, 0) ;
						mysys.MirrEnt(pRef, pt_3d_box, pt_3d_boxtmp) ;
					}
					pRef->close() ;
				}
			}
			else if (strBoxBlockPart1==_T("HZ"))
			{
				CString strZzjType ;
				cSet.GetFieldValue(_T("zzj_type"), strZzjType) ;
				if (!strZzjType.IsEmpty()) //转辙机
				{
					dBoxWidth = 6 ;
					CString strBoxBlockTmp = _T("BOX_HZ_ZZJ_0") ;
					strArrayVal.Add(strDevName) ;
					strArrayVal.Add(_T("0")) ;
					CString strZzjIndex ;
					cSet.GetFieldValue(_T("zzj_index"), strZzjIndex) ;
					strArrayVal.Add(strZzjIndex) ;
					strArrayVal.Add(strBoxType) ;
					strArrayVal.Add(strZzjType) ;
					if (mysys.InsertBlock(pRef, strBoxBlockTmp, pt_3d_box, &strArrayVal, FALSE))
					{
						pRef->close() ;
					}		
				}
				else if (strDevBlk.IsEmpty())
				{
					dBoxWidth = 3 ;

					CString strHz ;
					m_mysys.GetAttValue(nHandle, _T("HZ"), strHz) ;
					strArrayVal.Add(strHz) ;
					if (mysys.InsertBlock(pRef, strBoxBlock, pt_3d_box, &strArrayVal, FALSE))
					{
						pRef->close() ;
					}
				}
				else if (strDevBlk.Left(3)==_T("XHJ")) //信号机（调车等）
				{
					dBoxWidth = 3 ;

					CString strHz ;
					m_mysys.GetAttValue(nHandle, _T("HZ"), strHz) ;
					strArrayVal.Add(strHz) ;
					if (mysys.InsertBlock(pRef, strBoxBlock, pt_3d_box, &strArrayVal, FALSE))
					{
						pRef->close() ;
					}

					long nHandleBelongXhj = 0 ;
					m_mysys.GetXdata(nHandle, _T("BELONG_ENT"), nHandleBelongXhj) ;
					if (nHandleBelongXhj>0)
					{
						strArrayVal.RemoveAll() ;

						CString strXhjBlkName ;
						AcDbEntity* pEntXhj = NULL ;
						if (m_mysys.OpenAcDbEntity(nHandleBelongXhj, pEntXhj, AcDb::kForRead))
						{
							AcDbBlockReference* pRefXhj = AcDbBlockReference::cast(pEntXhj) ;
							m_mysys.GetBlockName(pRefXhj, strXhjBlkName) ;
							CString strXhjName ;
							m_mysys.GetAttValue(pRefXhj, _T("XHJNAME"), strXhjName) ;
							strArrayVal.Add(strXhjName) ;
							double dAngleXhj = pRefXhj->rotation() ;
							AcGePoint3d pt_3d_xhj = pt_3d_box ;
							pt_3d_xhj.y=pt_3d_box.y+1.5 ;
							double dAngleXhjNet = 0 ;
							if (dAngleXhj>PI/2&&dAngleXhj<PI*3/2)
							{
								dAngleXhjNet = PI ;
								pt_3d_xhj.y = pt_3d_box.y-1.5 ;
							}

							if (mysys.InsertBlock(pRef, strXhjBlkName, pt_3d_xhj, &strArrayVal, FALSE))
							{
								if (dAngleXhjNet>0)
								{
									mysys.RotationEnt(pRef, PI, pt_3d_xhj) ;
								}
								pRef->close() ;
							}						

							pEntXhj->close() ;
						}
					}
				}

			}
			else if (strBoxBlockPart1==_T("FSD"))
			{
				dBoxWidth = 4 ;
				CString strBoxBlockPart3 = strArrayBoxBlock.GetAt(3) ;


// 				long nHandleBelong= 0, nHandleBelong2= 0 ;
// 				m_mysys.GetXdata(nHandle, _T("BELONG_ENT"), nHandleBelong) ;
// 				m_mysys.GetXdata(nHandle, _T("BELONG_GD2"), nHandleBelong2) ;
// 				if (nHandleBelong>0)
// 				{
// 					strArrayVal.RemoveAll() ;
// 
// 					CString strGuiDaoName ;
// 					m_mysys.GetXdata(nHandleBelong, _T("GUIDAONAME"), strGuiDaoName) ;
// 					if (nHandleBelong2>0)
// 					{
// 						CString strGuiDaoName2 ;
// 						m_mysys.GetXdata(nHandleBelong2, _T("GUIDAONAME"), strGuiDaoName2) ;
// 						strGuiDaoName.AppendFormat(_T(" %s"), strGuiDaoName2) ;
// 					}
					strArrayVal.Add(strDevName) ;
					if (strBoxBlockPart3==_T("1")) //电码化
					{
						strArrayVal.Add(strFreq) ;
					}					

					if (mysys.InsertBlock(pRef, strBoxBlock, pt_3d_box, &strArrayVal, FALSE))
					{
						pRef->close() ;
					}
// 				}
			}
			else if (strBoxBlockPart1==_T("XB"))
			{
				dBoxWidth = 5 ;

				long iNumXhj = 1 ;
				m_mysys.GetXdata(nHandle, _T("BOX_SUM"), iNumXhj) ;
				long iIndexXhj = 1 ;
				m_mysys.GetXdata(nHandle, _T("BOX_INDEX"), iIndexXhj) ;
				CString strXb ;
				m_mysys.GetAttValue(nHandle, _T("XB"), strXb) ;
				strArrayVal.Add(strXb) ;
				if (mysys.InsertBlock(pRef, strBoxBlock, pt_3d_box, &strArrayVal, FALSE))
				{
					pRef->close() ;
				}

				if (iNumXhj==1||(iNumXhj==2&&iIndexXhj==1)) //多个XB箱，只在第一个XB箱上绘制信号机
				{
					long nHandleBelongXhj = 0 ;
					m_mysys.GetXdata(nHandle, _T("BELONG_ENT"), nHandleBelongXhj) ;
					if (nHandleBelongXhj>0)
					{
						strArrayVal.RemoveAll() ;

						CString strXhjBlkName ;
						AcDbEntity* pEntXhj = NULL ;
						if (m_mysys.OpenAcDbEntity(nHandleBelongXhj, pEntXhj, AcDb::kForRead))
						{
							AcDbBlockReference* pRefXhj = AcDbBlockReference::cast(pEntXhj) ;
							m_mysys.GetBlockName(pRefXhj, strXhjBlkName) ;
							CString strXhjName ;
							m_mysys.GetAttValue(pRefXhj, _T("XHJNAME"), strXhjName) ;
							strArrayVal.Add(strXhjName) ;
							double dAngleXhj = pRefXhj->rotation() ;
							AcGePoint3d pt_3d_xhj = pt_3d_box ;
							pt_3d_xhj.y=pt_3d_box.y+2 ;
							double dAngleXhjNet = 0 ;
							int iFindMark = strXhjBlkName.Find(_T("_Y_")) ; //iFindkMark不为-1说明是逆向信号机
							if (iFindMark!=-1)
							{
								pt_3d_xhj.y = pt_3d_box.y-2 ;
								dAngleXhjNet = 0 ;
								if (dAngleXhj>PI/2&&dAngleXhj<PI*3/2)
								{
									dAngleXhjNet = PI ;
									pt_3d_xhj.y = pt_3d_box.y+2 ;
								}
							}
							else
							{
								if (dAngleXhj>PI/2&&dAngleXhj<PI*3/2)
								{
									dAngleXhjNet = PI ;
									pt_3d_xhj.y = pt_3d_box.y-2 ;
								}
							}

							if (mysys.InsertBlock(pRef, strXhjBlkName, pt_3d_xhj, &strArrayVal, FALSE))
							{
								if (dAngleXhjNet>0)
								{
									mysys.RotationEnt(pRef, PI, pt_3d_xhj) ;
								}
								pRef->close() ;
							}						

							pEntXhj->close() ;
						}
					}
				}
			}

			//箱盒连线
			if (strBoxBlockPart1!=_T("XHL"))
			{				
				AcGePoint3dArray ptArray ;
				AcGePoint3d pt_3d_conline1, pt_3d_conline2, pt_3d_conline3 ; //从当前绘制盒子往信号楼方向绘制连接线
				strSql.Format(_T("select * from cablenet_ent where next_ent_handle_low=%d"), nHandleNext) ;
				int iNextBranchNum = m_RecordSet.GetRecordCount(strSql) ; //下个箱盒的分支总数
				strSql.Format(_T("select * from cablenet_ent where handle_low=%d"), nHandleNext) ;
				double dNextX = 0, dNextY = 0 ;
				m_RecordSet.GetRecordValue(strSql, _T("pt_x"), dNextX) ;
				m_RecordSet.GetRecordValue(strSql, _T("pt_y"), dNextY) ;
// 				CString strBranchIndexNext ; 
// 				m_RecordSet.GetRecordValue(strSql, _T("branch_index"), strBranchIndexNext) ;
				CString strBoxBlockNext, strBoxTypeNext, strZzjTypeNext ;
				m_RecordSet.GetRecordValue(strSql, _T("box_block"), strBoxBlockNext) ;
				m_RecordSet.GetRecordValue(strSql, _T("zzj_type"), strZzjTypeNext) ;
				CStringArray strArrayBlockNext ;
				CBlkUtility::DivideString(strBoxBlockNext, _T("_"),  strArrayBlockNext	) ;
				if (strArrayBlockNext.GetCount()>1)
				{
					strBoxTypeNext = strArrayBlockNext.GetAt(1) ;
				}
				double dBoxWidthNext = GetBoxWidth(strZzjTypeNext.IsEmpty()?strBoxTypeNext:_T("ZZJ")) ;
				double dBoxHeightNext = GetBoxHeight(strZzjTypeNext.IsEmpty()?strBoxTypeNext:_T("ZZJ")) ;
				int iCurBranchIndex = _ttoi(strBranchIndex.Right(1)) ; //当前箱盒所处的分支序号,此处默认分支数不会是2位数
				pt_3d_conline1.x = pt_3d_box.x+(iLeftOrRight==0?1:-1)*dBoxWidth/2 ; 
				pt_3d_conline1.y = pt_3d_box.y ;
				ptArray.append(pt_3d_conline1) ;//起点	
				//对于第2点，如果本box是next（离信号楼近）盒子伸出的第一个或最后一个box，则x和next盒子的基点对齐，即x值不用加减盒子的半长
				int  iXOffset = 1 ; //第2点的x坐标偏移否
				if (iNextBranchNum>1&&(iCurBranchIndex==1||iCurBranchIndex==iNextBranchNum))
				{
					iXOffset = 0 ;
				}
				pt_3d_conline2.x = dNextX+iXOffset*(iLeftOrRight==0?-1:1)*dBoxWidthNext/2 ;
				pt_3d_conline2.y = pt_3d_box.y ;
				ptArray.append(pt_3d_conline2) ; //第2点			
				if (pt_3d_box.y!=dNextY) 
				{
					pt_3d_conline3.x = dNextX ;
					pt_3d_conline3.y = dNextY+(pt_3d_box.y>dNextY?1:-1)*dBoxHeightNext/2 ;
					ptArray.append(pt_3d_conline3) ; //第3点
				}
				mysys.AddAcDbPolyLine(ptArray, 0) ;
				AcGePoint3d pt_3d_mid = mysys.GetMidPoint(pt_3d_conline1, pt_3d_conline2) ;
				pt_3d_mid.y+=1.5 ;
				AcDbObjectId objIdText ;
				mysys.AddText(objIdText, strCableString, pt_3d_mid, pt_3d_mid, AcDb::kTextMid, AcDb::kTextBase, 2, 0.7) ;
				CString strCableSnTmp ;
				CBlkUtility::RegReplace(_T("^(.*-)?(\\d+-\\d+[A-Z]*\\(\\d+\\)).*$"), _T("$1"), strCableString, strCableSnTmp) ;
				strCableSnTmp.Trim() ;
				if (strCableSnTmp.IsEmpty())
				{
					if (m_bNetWithSn) //网络图需要绘制电缆编号
					{
						pt_3d_mid.y -= 3 ;
						mysys.AddText(objIdText, strCableSn, pt_3d_mid, pt_3d_mid, AcDb::kTextMid, AcDb::kTextBase, 2, 0.7) ;
					}
				}
			}

			cSet.MoveNext() ;
			iIndex++ ;
		}
		cSet.Close() ;

		acedRestoreStatusBar() ;
	}

	CString strSavePath ;
	AcApDocument* pDocCur = acDocManager->curDocument() ;
	CString strFileCur =  pDocCur->fileName() ;
	CString strFileNet ; 
	strFileNet.Format(_T("%s-网络图.dwg"), strFileCur.Left(strFileCur.GetLength()-4) ) ;
	Acad::ErrorStatus es = pDbNew->saveAs(strFileNet) ;
	delete pDbNew ;
	pDbNew = NULL ;
	if (es == eOk)
	{
		acutPrintf(_T("\n生成网络图成功，文件保存至 %s"), strFileNet) ;
	}
	else
	{
		acutPrintf(_T("\n生成网络图失败!错误代码为:%d"), es) ;
	}
}

double CZnSxt::GetBoxWidth(const CString& strBoxType)
{
	double dWidht = 0 ;
	if (strBoxType.Find(_T("XB"))!=-1)
	{
		dWidht = 5 ;
	}
	else if (strBoxType.Find(_T("HZ"))!=-1)
	{
		dWidht = 3 ;
	}
	else if (strBoxType.Find(_T("HF"))!=-1)
	{
		dWidht = 6 ;
	}
	else if (strBoxType.Find(_T("FSD"))!=-1)
	{
		dWidht = 4 ;
	}
	else if (strBoxType.Find(_T("ZZJ"))!=-1)
	{
		dWidht = 6 ;
	}

	return dWidht ;
}

double CZnSxt::GetBoxHeight(const CString& strBoxType)
{
	double dHeight = 0 ;
	if (strBoxType.Find(_T("XB"))!=-1)
	{
		dHeight = 4 ;
	}
	else if (strBoxType.Find(_T("HZ"))!=-1)
	{
		dHeight = 3 ;
	}
	else if (strBoxType.Find(_T("HF"))!=-1)
	{
		dHeight = 6 ;
	}
	else if (strBoxType.Find(_T("FSD"))!=-1)
	{
		dHeight = 3 ;
	}
	else if (strBoxType.Find(_T("ZZJ"))!=-1)
	{
		dHeight = 6 ;
	}

	return dHeight ;
}

// 编辑实体（信号机、道岔、轨道电路）数据
void CZnSxt::ModEntData(void)
{
	CAcModuleResourceOverride myResources ;	

	ads_name en ;
	ads_point pt ;
	BOOL bReturn = false ;
	while (bReturn == false) 
	{
		int iReturn = acedEntSel(_T("\n选择对象（信号机、道岔等）"), en, pt) ;
		if(iReturn == RTNORM)
		{			
			AcDbEntity * pEnt = NULL ;
			if(m_mysys.OpenAcDbEntity(en, pEnt, AcDb::kForRead))
			{
				pEnt->highlight() ;				
				acedUpdateDisplay();

				if(pEnt->isKindOf(AcDbBlockReference::desc()))
				{
					AcDbBlockReference * pRef = AcDbBlockReference::cast(pEnt) ;
					CString strBlkName ;
					m_mysys.GetBlockName(pRef, strBlkName) ;
					if (strBlkName.Left(3)==_T("XHJ"))
					{
						CEntPropertyDlg dlg ;
						CXhjDATA *pDATA = new CXhjDATA(strBlkName) ;
						this->GetEntData(pEnt,*pDATA) ;

						dlg.m_grid.m_pEntDATA = pDATA ;
						if (dlg.DoModal()==IDOK)
						{
							pRef->upgradeOpen() ;
							this->SetEntData(pEnt, pDATA) ;
							pRef->downgradeOpen() ;

							long nHandleXhj = m_mysys.GetHandle(pEnt) ;
							pEnt->close() ;

							//更新箱盒
							this->DrawBoxXhj(nHandleXhj) ;
						}

						bReturn = true ;
					}
					else if (strBlkName.Left(2)==_T("DC"))
					{
						CEntPropertyDlg dlg ;
						CDcDATA *pDATA = new CDcDATA ;
						this->GetEntData(pEnt, *pDATA) ;

						dlg.m_grid.m_pEntDATA = pDATA ;
						if (dlg.DoModal()==IDOK)
						{
							pRef->upgradeOpen() ;
							this->SetEntData(pEnt, pDATA) ;
							pRef->downgradeOpen() ;							
														
							long nHandleDc = m_mysys.GetHandle(pEnt) ;		
							pEnt->close() ;

							//更新直弯切割绝缘节
							this->AddCutJYJ(nHandleDc) ;
							this->DrawZzj(nHandleDc) ;
						}

						bReturn = true ;
					}
					else
					{
						AfxMessageBox(_T("选择的不是信号机、道岔等能编辑的块！")) ;
						bReturn = false ;
					}
				}
				else if (pEnt->isKindOf(AcDbPolyline::desc()))
				{
					AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;

					int iXdata = 0 ;					
					if (m_mysys.GetXdata(pEnt, _T("ISGUIDAO"), iXdata)&&iXdata==1)
					{
						CEntPropertyDlg dlg ;
						CGdDATA *pDATA = new CGdDATA ;
						this->GetEntData(pEnt, *pDATA) ;

						dlg.m_grid.m_pEntDATA = pDATA ;
						if (dlg.DoModal()==IDOK)
						{
							pEnt->upgradeOpen() ;
							this->SetEntData(pEnt, pDATA) ;
							pEnt->downgradeOpen() ;

							//同步更新相关信息
							int iShowOrHide = 0 ;
							//if (m_mysys.ReadFromRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_GUIDAONAME"), iShowOrHide) &&iShowOrHide==0)
							CLongArray nArrayShowGdName ;
							if (FindShowGdName(pEnt, nArrayShowGdName)>0)
							{
								this->ShowGdName(pEnt) ; //更新轨道名称（辅助信息）
							}
							if (m_mysys.ReadFromRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_FSD"), iShowOrHide) &&iShowOrHide==0)
							{
								this->ShowGdFS(pEnt) ; //更新送受电（辅助信息）
							}
							this->UpdateFsBox(pEnt) ; //更新送受电盒子（在双线中）
							this->UpdateDmhBox(pEnt) ; //更新电码化盒子（在双线中）
							this->UpdateSxtGdName(pEnt, pDATA->m_strName) ; //更新在双线中轨道名称
						}

						bReturn = true ;
					}
					else if (m_mysys.GetXdata(pEnt, _T("IS_XHLCABLE"), iXdata)&&iXdata==1)
					{
						CXhlCableInfoDlg dlg ;
						dlg.m_pEnt = pEnt ;
						dlg.DoModal() ;

						bReturn = TRUE ;
					}
					else if (m_mysys.GetXdata(pEnt, _T("IS_CABLE"), iXdata)&&iXdata==1)
					{
						AcGePoint3d pt_3d_start, pt_3d_end, pt_3d_mid ;
						pPolyLine->getStartPoint(pt_3d_start) ;
						pPolyLine->getEndPoint(pt_3d_end) ;

						//起始端连接的盒子
						int iFindStartBoxNum = 0 ;
						CLongArray nArrayHandleStart ;
						CZnUtility::FindConBox(pt_3d_start, nArrayHandleStart) ;
						iFindStartBoxNum =nArrayHandleStart.GetCount() ;

						//末端连接的盒子
						int iFindEndBoxNum = 0 ;
						CLongArray nArrayHandleEnd ;
						CZnUtility::FindConBox(pt_3d_end, nArrayHandleEnd) ;
						iFindEndBoxNum = nArrayHandleEnd.GetCount() ;

						if (iFindStartBoxNum>0&&iFindEndBoxNum>0)
						{
							if (iFindEndBoxNum!=1||iFindStartBoxNum!=1)
							{
								acutPrintf(_T("\n注意：所选电缆两端连接箱盒不唯一,但仍以其中一个进行计算！")) ;								
							}
							long nHandleCable = 0 ;
							nHandleCable = m_mysys.GetHandle(pEnt) ;							
							long iIsXhlCable = 0 ;
							m_mysys.GetXdata(nArrayHandleEnd.GetAt(0), _T("IS_XHLCABLE"), iIsXhlCable) ;
							pEnt->unhighlight() ;
							pEnt->close() ; //此处先关闭是因为computecableinfo函数传递的参数nHandleCable会稍后打开读写实体，造成多次打开错误
							this->ComputeCableInfo(nArrayHandleStart.GetAt(0), nArrayHandleEnd.GetAt(0), nHandleCable, (iIsXhlCable>0?2:0)) ;
														
							bReturn = true ;
						}
						else
						{
							acutPrintf(_T("\n所选电缆不是两端都连接箱盒或信号楼或主电缆！")) ;
						}
					}
					else
					{
						AfxMessageBox(_T("选择的多段线不是轨道、电缆等可编辑对象！")) ;
						bReturn = false ;
					}
				}
				else
				{
					AfxMessageBox(_T("选择的不是本工具支持的实体！")) ;
					bReturn = false ;
				}
				if(pEnt!=NULL)
				{
					pEnt->unhighlight() ;
				}
				acedUpdateDisplay();
			}
			pEnt->close() ;	
		}
		else if (iReturn == RTERROR) 
		{
			bReturn = false ;
		}	
		else if (iReturn == RTCAN) {
			bReturn = true ;
		}
	}
}

// 查找pt_3d_base所在位置的实体
int CZnSxt::FindEnt(const AcGePoint3d& pt_3d_pick, const void* strDxfCode, const void* strAppName, CLongArray& nArrayHandle)
{
	//保存当前视图反位，本函数执行完后恢复
	CViewExtents viewExt ;
	m_mysys.GetViewExtents(viewExt) ;
	m_mysys.ZoomView() ;

	struct resbuf *pRb ;
	if (strDxfCode!=NULL&&strAppName!=NULL)
	{
		pRb = acutBuildList(RTDXF0, strDxfCode, kDxfRegAppName, strAppName, 0) ;
	}
	else if (strDxfCode!=NULL)
	{
		pRb = acutBuildList(RTDXF0, strDxfCode, 0) ;
	}
	else if (strAppName!=NULL)
	{
		pRb = acutBuildList(kDxfRegAppName, strAppName, 0) ;
	}
	else 
	{
		pRb = NULL ;
	}
	long iLen = 0 ;
	ads_point pt_min = {pt_3d_pick.x-0.1, pt_3d_pick.y-0.1, 0}, pt_max = {pt_3d_pick.x+0.1, pt_3d_pick.y+0.1, 0};
	iLen = m_mysys.GetSSEntHandle(_T("C"), pt_min, pt_max, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	m_mysys.SetViewExtents(viewExt) ;
	return iLen ;
}
// 计算指定盒子引入电缆总芯数
int CZnSxt::GetCableCoreNum(sequential_tree<CBoxNodeEnt>& cableBoxTree, CString& strCableTypeName, int& iBackup, int iDist)
{
	int iRet = 0 ;

	sequential_tree<CBoxNodeEnt>::post_order_iterator pit = cableBoxTree.post_order_begin();	//先遍历子节点	
	sequential_tree<CBoxNodeEnt>::post_order_iterator pit_end = cableBoxTree.post_order_end();	
	int iNumOfNodeVarFreq[3] = {0, 0, 0} ; //记录不同频率的节点个数，依次记录偶数频率、奇数频率、无频率（只对电码化有用）
	for (; pit != pit_end; ++pit)
	{
		//acutPrintf(_T("\n:%s, %s, %s"), pit->m_strBoxType, pit->m_strDevName, pit->m_strDevType) ;
		//begin 仅针对电码化
		if (m_iCurDljlType==3)
		{
			CBoxNodeEnt* pBoxNodeTmp = pit.node()->get() ;				

			//const CString strFreqCurNode = pBoxNodeTmp->m_strFreq ;
			if (! pBoxNodeTmp->m_strFreq.IsEmpty())
			{
				int iTmp = _ttoi(pBoxNodeTmp->m_strFreq.Left(2)) ;
				iNumOfNodeVarFreq[iTmp%2]++ ;
			}
			else
			{
				iNumOfNodeVarFreq[2]++ ;
			}
		}
		//end 仅针对电码化
		//本身的芯线（分向盒之类没有）
		if (!pit->m_strDevType.IsEmpty())
		{
			long iLenForPlus = 0 ;
            m_mysys.GetXdata(pit->m_nHandleCable, _T("LENFORPLUS"), iLenForPlus) ;
			this->GetDevCore(*(pit.node()->get()), iLenForPlus>0?iLenForPlus:iDist) ;				
		}
		//从子节点引入的芯线
		sequential_tree<CBoxNodeEnt>::iterator it = pit.node()->begin();
		for (; it != pit.node()->end(); ++it) //遍历各子节点
		{
			vector<CCableCore>::iterator it_core = it->m_vecCableCore.begin() ;
			for (;it_core!=it->m_vecCableCore.end(); ++it_core) //遍历当前子节点的芯线
			{
				if(pit->FindCableCore(*it_core)==false)//查找父节点本身新出芯是否已有，有则舍弃此条
				{
					if (it_core->IsInside()!=1)
					{
						pit->m_vecCableCore.push_back(*it_core) ;
					}
				}
			}
		}
	}

	int iNumNeed4PGroup = 0 ;
	//int iBackupNum = 0 ;
	//CString strCableTypeName ;
	iRet = cableBoxTree.get()->m_vecCableCore.size() ; //实际使用芯数
	int iCableType = 0 ;//普通电缆，1是屏蔽电缆

	if (m_iCurDljlType==3)
	{
		iCableType = 1 ;
		iNumNeed4PGroup = min(iNumOfNodeVarFreq[0], iNumOfNodeVarFreq[1])+abs(iNumOfNodeVarFreq[0]-iNumOfNodeVarFreq[1]) ;//需要用到的内屏蔽4芯组个数	
	}

	this->GetUseCableType(iRet, iNumNeed4PGroup, iBackup, strCableTypeName, m_iCurDljlType, iCableType ) ;
	//strCableTypeAndBackup.Format(_T("%s(%d)"), strCableTypeName, iBackupNum) ;

	return iRet ;
}

BOOL CZnSxt::GetBoxInfo(long nHandleBox, CBoxNodeEnt& boxNode)
{
	BOOL bRet = FALSE ;

	AcDbEntity* pEntFind = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandleBox, pEntFind, AcDb::kForRead))
	{
		if (pEntFind->isKindOf(AcDbBlockReference::desc()))
		{
			AcDbBlockReference* pRef = AcDbBlockReference::cast(pEntFind) ;
			m_mysys.GetBlockName(pRef, boxNode.m_strBoxBlock) ;
			CStringArray strArrayBlkNameBox ;
			CBlkUtility::DivideString(boxNode.m_strBoxBlock, _T("_"), strArrayBlkNameBox) ;
			int iNum = strArrayBlkNameBox.GetCount() ;
			if (iNum<3)
			{
				pEntFind->close() ;
				return FALSE ;
			}
			CString strBlkNameBoxPart0 = strArrayBlkNameBox.GetAt(0) ;
			CString strBlkNameBoxPart1 = strArrayBlkNameBox.GetAt(1) ;
			if (strBlkNameBoxPart0==_T("BOX"))
			{
				if (strBlkNameBoxPart1==_T("HF"))
				{
					m_mysys.GetAttValue(pRef, _T("HFNUM"), boxNode.m_strDevName) ;							
					int iHfNum = 0 ;
					m_mysys.GetAttValue(pRef, _T("HF"), iHfNum) ;
					boxNode.m_strBoxType.Format(_T("HF%d"), iHfNum) ;
					CString strDistToXhl ;
					m_mysys.GetAttValue(pRef, _T("DIST"), strDistToXhl) ;
					strDistToXhl.TrimLeft(_T('(')) ;
					strDistToXhl.TrimRight(_T(')')) ;
					boxNode.m_iDistToXhl = _tstoi(strDistToXhl) ;	
					boxNode.m_iEntType = -1 ;
					//m_iCurDljlType = -1 ;
					//pBoxNode = new CBoxNodeBase(strBoxType, strDevName, _T(""), nHandleBox,  0,  strBlkNameBox, _T(""), _T(""), iDistToXhl) ; 
				}
				else if (strBlkNameBoxPart1==_T("XB"))
				{
					m_mysys.GetAttValue(pRef, _T("XB"), boxNode.m_strBoxType) ;
					//long nHandleBoxBelong = 0 ;
					if(m_mysys.GetXdata(pEntFind, _T("BELONG_ENT"), boxNode.m_nHandleDev))
					{
						m_iCurDljlType = 1 ; //信号机
						//int iBoxSumCurXhj = 1,  iBoxIndexCurXhj = 1 ;
						if(this->GetBoxInfo_XHJ(pEntFind, boxNode.m_nHandleDev, boxNode.m_iBoxSum, boxNode.m_iBoxIndex, boxNode.m_strDevName, boxNode.m_strDevBlock, boxNode.m_strDevType))
						{
							if (boxNode.m_iBoxSum>1)
							{
								boxNode.m_strDevType.Insert(3, boxNode.m_iBoxIndex==1?_T('B'):_T('A')) ;
								boxNode.m_strDevType.AppendFormat(_T("_%s-%s"), boxNode.m_strBoxType, boxNode.m_strBoxType) ;
							}
							boxNode.m_iEntType = 1 ;
							//pBoxNode = new CBoxNodeXhj(strBoxType, strDevName, strDevType, iBoxSumCurXhj, iBoxIndexCurXhj, nHandleBox,  nHandleBoxBelong, strBlkNameBox, strBlkNameDev, _T(""), 0) ;
						}
					}
				}
				else if (strBlkNameBoxPart1==_T("HZ"))
				{
					int iHzNum = 0 ;
					m_mysys.GetAttValue(pRef, _T("HZ"), iHzNum) ;
					boxNode.m_strBoxType.Format(_T("HZ%d"), iHzNum) ;

					long nHandleBoxBelong = 0 ;
					if(m_mysys.GetXdata(pEntFind, _T("BELONG_ENT"), nHandleBoxBelong)) 
					{
						CString strBlkNameBoxBelong ;
						if (m_mysys.GetBlockName(nHandleBoxBelong, strBlkNameBoxBelong) )
						{
							CString strTypeBoxBelong = strBlkNameBoxBelong.Left(3) ;
							if (strTypeBoxBelong==_T("XHJ"))//信号机
							{
								m_iCurDljlType = 1 ; //信号机
								boxNode.m_nHandleDev = nHandleBoxBelong ;
								//int iBoxSumCurXhj = 1,  iBoxIndexCurXhj = 1 ;
								if(this->GetBoxInfo_XHJ(pEntFind, boxNode.m_nHandleDev, boxNode.m_iBoxSum, boxNode.m_iBoxIndex, boxNode.m_strDevName, boxNode.m_strDevBlock, boxNode.m_strDevType))
								{
									if (boxNode.m_iBoxSum>1)
									{
										boxNode.m_strDevType.Insert(3, boxNode.m_iBoxIndex==1?_T('B'):_T('A')) ;
										boxNode.m_strDevType.AppendFormat(_T("_%s-%s"), boxNode.m_strBoxType, boxNode.m_strBoxType) ;
									}
									boxNode.m_iEntType = 1 ;
									//pBoxNode = new CBoxNodeXhj(strBoxType, strDevName, strDevType, iBoxSumCurXhj, iBoxIndexCurXhj, nHandleBox,  nHandleBoxBelong, strBlkNameBox, strBlkNameDev, _T(""), 0) ;									
								}
							}
							else if (strTypeBoxBelong==_T("ZZJ"))//转辙机
							{
								m_iCurDljlType = 0 ; //道岔
								long iIsJxg = 1 ; //默认尖轨，=2是芯轨
								m_mysys.GetXdata(nHandleBoxBelong, _T("IS_JXG"), iIsJxg) ;
								boxNode.m_iJxg = iIsJxg ;
								//CString strDragType ;
								m_mysys.GetAttValue(nHandleBoxBelong, _T("TYPE"), boxNode.m_strDragType) ;
								m_mysys.GetAttValue(nHandleBoxBelong, _T("ACTINDEX"), boxNode.m_strActIndex) ;

								long nHandleDc = 0 ;								
								if(m_mysys.GetXdata(nHandleBoxBelong, _T("BELONG_ENT"), nHandleDc)) 
								{
									m_mysys.GetAttValue(nHandleDc, _T("BIANHAO"), boxNode.m_strDevName) ;

									CDcDATA dataDc ;
									this->GetEntData(nHandleDc, dataDc) ;
									CString strQyType = (boxNode.m_iJxg==1?dataDc.m_strJG_QyType:dataDc.m_strXG_QyType) ;
									this->GetZzjDevType(nHandleBox, boxNode.m_strDevType) ;
									//int iDragIndex = 1 ;
									if (boxNode.m_strDevType.GetLength()>0)
									{
										boxNode.m_iDragIndex = int(boxNode.m_strDevType.GetAt(boxNode.m_strDevType.GetLength()-1)-_T('A'))+1 ;
									}
									boxNode.m_iEntType = 0 ;
									//pBoxNode = new CBoxNodeZzj(strBoxType, strDevName, strDevType, strDragType, iDragIndex, strQyType, strActIndex, 0, nHandleBox, nHandleDc, strBlkNameBox) ;
								}
							}
						}
					}
				}
				else if (strBlkNameBoxPart1==_T("FSD"))
				{
					m_iCurDljlType = 2 ; //轨道电路
					long nHandleBoxBelong = 0 ;
					if(m_mysys.GetXdata(pEntFind, _T("BELONG_ENT"), nHandleBoxBelong)) //送受电
					{
						long nLeftOrRight = 0 ;
						m_mysys.GetXdata(pEntFind, _T("LEFT_RIGHT"), nLeftOrRight) ;	

						//m_mysys.GetBlockName(nHandleBoxBelong, boxNode.m_strDevBlock) ;						
						m_mysys.GetAttValue(nHandleBox, _T("GDNAME"), boxNode.m_strDevName) ;
						if (boxNode.m_strDevName.IsEmpty()) //以盒子属性设置的轨道名为准，但如果没有读到值，再次从轨道扩展数据读取
						{
							m_mysys.GetXdata(nHandleBoxBelong, _T("GUIDAONAME"), boxNode.m_strDevName) ;
							CString strForSTmp = _T("F") ;
							if (nLeftOrRight==1) //左
							{
								m_mysys.GetXdata(nHandleBoxBelong, _T("LEFT_FORS"), strForSTmp) ;	
							}
							else if (nLeftOrRight==0)//右
							{
								m_mysys.GetXdata(nHandleBoxBelong, _T("RIGHT_FORS"), strForSTmp) ;
							}
							if (strForSTmp==_T("S"))
							{
								boxNode.m_strDevName.Append(_T("J")) ;
							}
						}
						boxNode.m_strBoxType = _T("XB1") ;						
						CString strBlkNameBoxPart2 = strArrayBlkNameBox.GetAt(2) ;
						CString strBlkNameBoxPart3 = strArrayBlkNameBox.GetAt(3) ;
						//strDevType = _T("FSD_GD_0") ;
						boxNode.m_strDevType.Format(_T("FSD_GD_%s"), strBlkNameBoxPart2 ) ;
						if (strBlkNameBoxPart3==_T("1")) //电码化
						{
							m_iCurDljlType = 3 ; //电码化
							//strDevType = _T("FSD_DM_0") ;
							boxNode.m_strDevType.Format(_T("FSD_DM_%s"), strBlkNameBoxPart2 ) ;
							m_mysys.GetAttValue(nHandleBox, _T("FREQ"), boxNode.m_strFreq) ;
							if (boxNode.m_strFreq.IsEmpty()) //同理（读轨道名称）
							{
								if (nLeftOrRight==0) //左
								{
									m_mysys.GetXdata(nHandleBoxBelong, _T("FREQ_LEFT"), boxNode.m_strFreq) ;
								}
								else if (nLeftOrRight==1)//右
								{
									m_mysys.GetXdata(nHandleBoxBelong, _T("FREQ_RIGHT"), boxNode.m_strFreq) ;
								}
							}
							boxNode.m_iEntType = 3 ;
							//pBoxNode = new CBoxNodeDmh(strBoxType, strDevName, strDevType, strFreq, nHandleBox, 0, strBlkNameBox) ;
						}
						else  //轨道
						{
							boxNode.m_iEntType = 2 ;
							//pBoxNode = new CBoxNodeGd(strBoxType, strDevName, strDevType, nHandleBox, 0, strBlkNameBox) ;
						}
						if (strBlkNameBoxPart2.Find(_T("-"))!=-1)
						{
							boxNode.m_strBoxType = _T("XB2") ;
							long nHandleBoxBelong2 = 0 ;
							if(m_mysys.GetXdata(pEntFind, _T("BELONG_GD2"), nHandleBoxBelong2)) 
							{
								if (nHandleBoxBelong2>0)
								{
									CString strGuiDaoName2 ;
									m_mysys.GetXdata(nHandleBoxBelong2, _T("GUIDAONAME"), strGuiDaoName2) ;
									boxNode.m_strDevName.AppendFormat(_T(" %s"), strGuiDaoName2) ;
									//pBoxNode = new CBoxNodeGd(strBoxType, strDevName, strDevType, nHandleBox, 0, strBlkNameBox) ;
								}
							}
						}
					}
				}
				bRet = TRUE ;
			}
		}

		pEntFind->close() ;
	}

	return bRet ;
}

BOOL CZnSxt::GetBoxInfo_XHJ(AcDbEntity* pEnt, long nHandleBoxBelong, int &iBoxSumCurXhj, int &iBoxIndexCurXhj, CString & strDevName, CString & strBlkNameDev, CString & strDevType)
{
	BOOL bRet = FALSE ;

	m_mysys.GetXdata(pEnt, _T("BOX_SUM"), iBoxSumCurXhj) ;
	m_mysys.GetXdata(pEnt, _T("BOX_INDEX"), iBoxIndexCurXhj) ;
	m_mysys.GetAttValue(nHandleBoxBelong, _T("XHJNAME"), strDevName) ;

	m_mysys.GetBlockName(nHandleBoxBelong, strBlkNameDev) ;
	CStringArray strArrayDevName ;
	CBlkUtility::DivideString(strBlkNameDev, _T("_"), strArrayDevName) ;
	if (strArrayDevName.GetCount()>7)
	{
		strDevType.Format(_T("XHJ_%s_%s"), strArrayDevName.GetAt(2), strArrayDevName.GetAt(4)) ;
		bRet = TRUE ;
	}
	else
	{
		acutPrintf(_T("\n信号机 %s 块名 %s 不能分解成7块以上，请检查是否是标准块!"), strDevName, strBlkNameDev) ;
	}

	return bRet ;
}

// 取得一个设备的芯线
int CZnSxt::GetDevCore(CBoxNodeEnt &boxNode/*const CString& strDevType, const CString& strDevName, const CString& strFreq, vector<CCableCore>* pVec*/, int iDist)
{
	int iRet = 0 ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;

		//先取得加芯数据（主要是转辙机）
		CMapStringToString mapZzjPlusCore ;
		strSql.Format(_T("select top 1 * from define_zzj_plus where dev_for='%s' and cable_len>%d"), boxNode.m_strDevType, iDist) ;
		long nId = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("id"), nId) ;
		if (nId==0) //未找到 ，则去字典查找 strDevType匹配的
		{
			CString strSqlTmp ;
			strSqlTmp.Format(_T("select  * from define_dict where dict_for='ZZJ_PLUS' and source='%s'"), boxNode.m_strDevType) ;
			CString strDestination ;
			m_RecordSet.GetRecordValue(strSqlTmp, _T("destination"), strDestination) ;
			if (!strDestination.IsEmpty())
			{
				strSql.Format(_T("select top 1 * from define_zzj_plus where dev_for='%s' and cable_len>%d"), strDestination, iDist) ;
			}
		}
		CString strCoreList ;
		if (m_RecordSet.GetRecordValue(strSql, _T("core_list"), strCoreList) >0)
		{
			CStringArray strArrayCoreList ;
			CBlkUtility::DivideString(strCoreList, _T("_"), strArrayCoreList) ;
			for (int iIndexCoreList=0; iIndexCoreList<strArrayCoreList.GetCount(); iIndexCoreList++)
			{
				CString strCoreTmp = strArrayCoreList.GetAt(iIndexCoreList) ;
				int iFindLoc = strCoreTmp.Find(_T('-')) ;
				if (iFindLoc!=-1)
				{
					mapZzjPlusCore.SetAt(strCoreTmp.Left(iFindLoc), strCoreTmp.Right(strCoreTmp.GetLength()-iFindLoc-1)) ;
				}												
			}
		}

		CString strDevFor = _T("JY1") ;
		if (boxNode.m_iEntType==1)
		{
			if (m_iDsNumXhj==1)
			{
				strDevFor = _T("JY3") ;
			}
			if (boxNode.m_strDevBlock.Mid(4,2)==_T("DC")) //调车
			{
				if (m_iDcDsNum==1)
				{
					strDevFor = _T("JY2") ;
				}
				else if (m_iDcDsNum==2)
				{
					strDevFor = _T("JY3") ;
				}
				else
				{
					strDevFor = _T("JY1") ;
				}
			}
		}
		if (boxNode.m_iEntType==0)
		{
			if (m_iQkNum==1)
			{
				strDevFor = _T("JY4") ;
			}
		}

		//直接找不到dev，再通过字典找一遍
		strSql.Format(_T("select * from define_dev_use_cablecore where dev_id in(select id from define_dev where dev_type='%s' and dev_for='%s' ) order by id"), boxNode.m_strDevType, strDevFor) ;
		nId = 0 ;
		if (m_RecordSet.GetRecordValue(strSql, _T("id"), nId) == 0)
		{
			CString strSqlTmp ;
			strSqlTmp.Format(_T("select  * from define_dict where dict_for='ZZJ_CORE' and source='%s'"), boxNode.m_strDevType) ;
			CString strDestination ;
			if (m_RecordSet.GetRecordValue(strSqlTmp, _T("destination"), strDestination) ==1)
			{
				strSql.Format(_T("select * from define_dev_use_cablecore where dev_id in(select id from define_dev where dev_type='%s' and dev_for='%s' ) order by id"), strDestination, strDevFor) ;
			}
		}

		if (cSet.Open(strSql))
		{
			iRet = cSet.GetRecordCount() ;
			while (!cSet.IsEOF())
			{
				CString strCoreName, strCoreNameShow;
				int iIsInside = 0, iIsShare = 0, iIsAdd = 0 ;	
				cSet.GetFieldValue(_T("core_name"), strCoreName) ;
				cSet.GetFieldValue(_T("core_name_show"), strCoreNameShow) ;
				cSet.GetFieldValue(_T("is_inside"), iIsInside) ;
				cSet.GetFieldValue(_T("is_share"), iIsShare) ;
				cSet.GetFieldValue(_T("is_add"), iIsAdd) ;
				CCableCore cablecore(iIsShare==0?boxNode.m_strDevName:_T(""), strCoreName, strCoreNameShow, boxNode.m_strFreq, iIsInside, iIsShare, iIsAdd) ;
				boxNode.m_vecCableCore.push_back(cablecore) ;

				//begin 加芯
				CString strCoreNum ;
				if (mapZzjPlusCore.Lookup(strCoreName, strCoreNum) )
				{
					int iCoreNum = _tstoi(strCoreNum) ;
					if (iCoreNum>1)
					{
						for (int iPlusIndex=0; iPlusIndex<(iCoreNum-1); iPlusIndex++)
						{
							CString strCoreNamePlus = strCoreName ;
							CString strCoreNameShowPlus = strCoreNameShow ;

							strCoreNamePlus.AppendFormat(_T("+%d"), iPlusIndex+1) ;
							strCoreNameShowPlus.AppendFormat(_T("+%d"), iPlusIndex+1) ;
							//iIsNew = 1 ;
							//iIsInside = 0 ;
							iIsShare = 0 ;
							iIsAdd = 1 ;
							CCableCore cablecorePlus(iIsShare==0?boxNode.m_strDevName:_T(""), strCoreNamePlus, strCoreNameShowPlus, boxNode.m_strFreq, iIsInside, iIsShare, iIsAdd) ;
							boxNode.m_vecCableCore.push_back(cablecorePlus) ;
						}
					}
				}
				//end 加芯

				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in GetDevCore"));
	}
	return iRet;
}

 // 查找满足条件的电缆型号, iDevType=-1：通用（混合），0：道岔，1，信号机，2：轨道电路；3：电码化，iCableType=0,普通综合扭绞（数字）电缆，1，内屏蔽数字电缆
BOOL CZnSxt::GetUseCableType(int iUseCoreNum, int iNumNeed4PGroup, int& iBackupNum, CString& strCableTypeName, int iDevType, int iCableType)
{
	BOOL bRet = FALSE ;
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		if(m_iCurDljlType==3)//电码化
		{
			if (iUseCoreNum==2&&iNumNeed4PGroup==1)
			{
				strCableTypeName = _T("4") ;
				iBackupNum = 2 ;
			}
			else if (iUseCoreNum==4)
			{
				strCableTypeName = _T("8B") ;
				iBackupNum = 4 ;
			}
			else
			{
				strSql.Format(_T("SELECT cable_type_id, count(core_group_type) as num4p  from define_core_detail where core_group_type='4P' and backup=0 and (cable_type_id in(select id from define_cable_type where right(cable_name,1)='B'))  group by cable_type_id")) ;
				if (cSet.Open(strSql))
				{
					while(!cSet.IsEOF())
					{
						int iNum4p = 0 ;	
						cSet.GetFieldValue(_T("num4p"), iNum4p) ;
						if (iNum4p>=iNumNeed4PGroup*4)
						{
							int iCableTypId = 0 ;
							cSet.GetFieldValue(_T("cable_type_id"), iCableTypId) ;
							strSql.Format(_T("select * from define_cable_type where id=%d"), iCableTypId) ;
							m_RecordSet.GetRecordValue(strSql, _T("cable_name"), strCableTypeName) ;
							long iNumCore = 0 ;
							m_RecordSet.GetRecordValue(strSql, _T("core_num"), iNumCore) ;
							iBackupNum = iNumCore-iUseCoreNum ;

							bRet = TRUE ;
							break;
						}	
						cSet.MoveNext() ;
					}
					cSet.Close() ;
				}
			}
		}
		else
		{
			//strSql.Format(_T("select top 1 * from define_cable_type where cable_type=%d and fact_use_num>=%d %s order by id asc"), iCableType, iUseCoreNum, m_iCurDljlType==1?_T("and cable_name not in('14','14A','14B')"):_T("")) ;
			strSql.Format(_T("select top 1 * from define_cable_type where cable_type=%d and fact_use_num>=%d and right(cable_name,1)<>'A' order by id asc"), iCableType, iUseCoreNum) ;
			if (cSet.Open(strSql))
			{
				if (cSet.GetRecordCount()==1)
				{
					int iCoreNum = 0 ;	
					cSet.GetFieldValue(_T("cable_name"), strCableTypeName) ;
					cSet.GetFieldValue(_T("core_num"), iCoreNum) ;
					iBackupNum = iCoreNum-iUseCoreNum ;

					bRet = TRUE ;
				}
				cSet.Close() ;
			}
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in GetUseCableType"));
	}
	return bRet;
}

// 检查并顺正、合并轨道线（两头无任何连接的标示出来）
void CZnSxt::CheckGdLine(void)
{
	BOOL bHasErro = FALSE ;

	CString strMsg ;

	double dTolCheckGd = 0.001 ;
	CBlkUtility::QueryValueReg(_T("ZnOption"), _T("TolCheckGd"), dTolCheckGd) ;
	acutPrintf(_T("\n框选所有的轨道线。")) ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(-4,_T("<OR"),RTDXF0,_T("LINE"),RTDXF0,_T("LWPOLYLINE"), -4,_T("OR>"), 0) ;
	nLen = m_mysys.GetSSEntHandle(NULL, NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	int iRet = 0 ;
	//线顺正
	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEnt = NULL ;
		long nHandle = nArrayHandle.GetAt(i) ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{			
			if (m_mysys.ReverseLinePoint(pEnt, 0, -1) >0)
			{
				iRet++ ;
			}
			pEnt->close() ;
		}
	}
	acutPrintf(_T("\n顺正线%d条"), iRet) ;

	//单线转多段线
	iRet = 0 ;
	CLongArray nArrayHandleNew/*, nArrayHandleToDel*/ ;
	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEnt = NULL ;
		long nHandle = nArrayHandle.GetAt(i) ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbLine::desc()))
			{
				AcGePoint3d pt_3d_se[2] ;
				AcDbLine* pLine = AcDbLine::cast(pEnt) ;
				pLine->getStartPoint(pt_3d_se[0]) ;
				pLine->getEndPoint(pt_3d_se[1]) ;
				AcDbObjectId ojbId = m_mysys.AddAcDbPolyLine(pt_3d_se[0], pt_3d_se[1]) ;
				long nHandleNew = m_mysys.GetHandle(ojbId) ;
				nArrayHandleNew.Add(nHandleNew) ;
				pEnt->upgradeOpen() ;
				pEnt->erase() ; //删除单线
				iRet++ ;
			}
			else if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				nArrayHandleNew.Add(nHandle) ;
			}
			pEnt->close() ;
		}
	}
	acutPrintf(_T("\n转换直线到多段线%d条"), iRet) ;

	nArrayHandle.RemoveAll() ;
	nArrayHandle.Copy(nArrayHandleNew) ;

	//检查多个起点重合或多个终点重合的线
	for (int i=0; i<nLen; i++)
	{
		AcGePoint3d pt_3d_se[2] ;

		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbLine::desc()))
			{
				AcDbLine* pLine = AcDbLine::cast(pEnt) ;
				pLine->getStartPoint(pt_3d_se[0]) ;
				pLine->getEndPoint(pt_3d_se[1]) ;
			}
			else if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
				pPolyLine->getStartPoint(pt_3d_se[0]) ;
				pPolyLine->getEndPoint(pt_3d_se[1]) ;
			}
			pEnt->close() ;
		}
		for (int m=0; m<2; m++) //m=0：起点;m=1：终点
		{
			CLongArray nArrayHandleLine[2] ;
			m_mysys.GetLineOfStartOrEndInPt(pt_3d_se[m], nArrayHandle, nArrayHandleLine, dTolCheckGd) ; 
			for (int n=0; n<2; n++)
			{
				int iNum=nArrayHandleLine[n].GetCount() ;
				if (iNum>1) //多于1个起/终点位于pt_3d_start
				{
					bHasErro = TRUE ;

					CString strHandle ;
					for (int j=0; j<iNum; j++)
					{
						strHandle.AppendFormat(_T("%d,"), nArrayHandleLine[n].GetAt(j)) ;
						m_mysys.SetEntColorIndex(nArrayHandleLine[n].GetAt(j), 3) ;
					}
					strHandle.TrimRight(_T(",")) ;
					acutPrintf(_T("\n在点(%.3f,%.3f)处找到多于1个以此为%s点的线(%s)，已标示，请先处理后再运行此命令。"), pt_3d_se[m].x, pt_3d_se[m].y, n==0?_T("起"):_T("终"), strHandle) ;
				}
			}
		}
	}
	
	if (bHasErro==TRUE)
	{
		strMsg.Format(_T("发现多个起点/终点位于同一点的情况，已标示，请先处理后再运行此命令。")) ;
		AfxMessageBox(strMsg) ;
		return ;
	}

	CString strHasFinishPt ;  //记录形如1111-0/1111-1（线的handle-起点0/终点1）
	CLongArray nArrayHandleToJoin[2] ;
	//查找需要首尾相连的线
	for (int i=0; i<nLen; i++)
	{
		AcGePoint3d pt_3d_se[2] ;

		long nHandleCur = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandleCur, pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbLine::desc()))
			{
				AcDbLine* pLine = AcDbLine::cast(pEnt) ;
				pLine->getStartPoint(pt_3d_se[0]) ;
				pLine->getEndPoint(pt_3d_se[1]) ;
			}
			else if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
				pPolyLine->getStartPoint(pt_3d_se[0]) ;
				pPolyLine->getEndPoint(pt_3d_se[1]) ;
			}
			pEnt->close() ;
		}
		for (int m=0; m<2; m++) //m=0：起点m=1：终点
		{
			CString strCurPt ; 
			strCurPt.Format(_T("%d-%d"), nHandleCur, m) ;
			if (strHasFinishPt.Find(strCurPt)!=-1)
			{
				continue; 
			}
			CLongArray nArrayHandleLine[2] ;
			m_mysys.GetLineOfStartOrEndInPt(pt_3d_se[m], nArrayHandle, nArrayHandleLine, dTolCheckGd) ; 
			if (m==0&&nArrayHandleLine[1].GetCount()==1) //在起点找到1个以此为终点的线
			{
				nArrayHandleToJoin[0].Add(nArrayHandleLine[1].GetAt(0)) ;
				nArrayHandleToJoin[1].Add(nHandleCur) ;
				strHasFinishPt.AppendFormat(_T("%s,"), strCurPt) ;
				strCurPt.Format(_T("%d-%d"), nArrayHandleLine[1].GetAt(0), 1) ;
				strHasFinishPt.AppendFormat(_T("%s,"), strCurPt) ;
			}
			else if (m==1&&nArrayHandleLine[0].GetCount()==1) //在起点找到1个以此为起点的线
			{
				nArrayHandleToJoin[0].Add(nHandleCur) ;
				nArrayHandleToJoin[1].Add(nArrayHandleLine[0].GetAt(0)) ;				
				strHasFinishPt.AppendFormat(_T("%s,"), strCurPt) ;
				strCurPt.Format(_T("%d-%d"), nArrayHandleLine[0].GetAt(0), 0) ;
				strHasFinishPt.AppendFormat(_T("%s,"), strCurPt) ;
			}
		}
	}
	//合并首尾相连的线
	int iNumJoin = nArrayHandleToJoin[0].GetCount() ;
	int iNumJoinInfact = 0 ;
	for (int i=0; i<iNumJoin; i++)
	{
		long nHandleLeft = nArrayHandleToJoin[0].GetAt(i) ;
		long nHandleRight = nArrayHandleToJoin[1].GetAt(i) ;
		acutPrintf(_T("\n准备合并多段线  %d 和 %d "), nHandleLeft, nHandleRight) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandleLeft, pEnt))
		{
			AcDbPolyline* pPolyLineLeft = AcDbPolyline::cast(pEnt) ;
			int iNumVert1 = pPolyLineLeft->numVerts() ;

			AcDbEntity* pEnt2 = NULL ;
			if (m_mysys.OpenAcDbEntity(nHandleRight, pEnt2, AcDb::kForRead))
			{
				AcDbPolyline* pPolyLineRight = AcDbPolyline::cast(pEnt2) ;

				AcGePoint3d pt_3d_start_left, pt_3d_start_right ;
				pPolyLineLeft->getStartPoint(pt_3d_start_left) ;
				pPolyLineRight->getStartPoint(pt_3d_start_right) ;
				acutPrintf(_T("\n%d:准备合并起点为(%.3f, %.3f),和起点为(%.3f, %.3f)的线"), i, pt_3d_start_left.x, pt_3d_start_left.y, pt_3d_start_right.x, pt_3d_start_right.y) ;

				int iNumVert2 = pPolyLineRight->numVerts() ;
				for (int j=0; j<iNumVert2-1; j++)
				{
					AcGePoint2d pt_2d_add ;
					pPolyLineRight->getPointAt(j+1, pt_2d_add) ;
					double dWidthStart = 0, dWidthEnd = 0 ;
					pPolyLineRight->getWidthsAt(j, dWidthStart, dWidthEnd) ;
					pPolyLineLeft->addVertexAt(iNumVert1+j, pt_2d_add) ;
					pPolyLineLeft->setWidthsAt(iNumVert1+j-1, dWidthStart, dWidthEnd) ;
				}

				pEnt2->upgradeOpen() ;
				pEnt2->erase() ;
				pEnt2->close() ;

				pPolyLineLeft->setColorIndex(3) ;

				bHasErro = TRUE ;
				iNumJoinInfact++ ;
			}				

			pEnt->close() ;
		}
	}   

	if (bHasErro==TRUE)
	{
		strMsg.Format(_T("合并%d处多段线,已标示，请先确认后再运行此命令。"), iNumJoinInfact) ;
		AfxMessageBox(strMsg) ;
		return ;
	}

	//检查多个顶点位于同一坐标的多段线
	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEnt = NULL ;
		long nHandle = nArrayHandle.GetAt(i) ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
				int iNumVert = pPolyLine->numVerts() ;
				for (int j=0; j<iNumVert-1; j++)
				{
					AcGePoint2d pt_2d_cur, pt_2d_next ;
					pPolyLine->getPointAt(j, pt_2d_cur) ;
					pPolyLine->getPointAt(j+1, pt_2d_next) ;
					AcGeTol tol ;
					tol.setEqualPoint(dTolCheckGd) ;
					if (pt_2d_cur.isEqualTo(pt_2d_next, tol))
					{
						bHasErro = TRUE ;
						acutPrintf(_T("\n在点(%.3f,%.3f)处有重合点！"), pt_2d_cur.x, pt_2d_cur.y) ;
						pPolyLine->upgradeOpen() ;
						pPolyLine->setColorIndex(3) ;
						pPolyLine->downgradeOpen() ;
						break ;
					}
				}
			}

			pEnt->close() ;
		}
	}	
	if (bHasErro==TRUE)
	{
		strMsg.Format(_T("发现有重合顶点的多段线,已标示，请先确认后再运行此命令。")) ;
		AfxMessageBox(strMsg) ;
		return ;
	}

	//将多段线不必要的顶点删除（前后两段线方向一致）
	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEnt = NULL ;
		long nHandle = nArrayHandle.GetAt(i) ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
				int iNumVert = pPolyLine->numVerts() ;
				for (int j=0; j<iNumVert-2; j++)
				{
					AcGeLineSeg2d lineCur, lineNext ;
					pPolyLine->getLineSegAt(j, lineCur) ;
					pPolyLine->getLineSegAt(j+1, lineNext) ;
					AcGeTol tol ;
					tol.setEqualVector(dTolCheckGd) ;
					if (lineCur.isParallelTo(lineNext, tol))
					{
						bHasErro = TRUE ;
						acutPrintf(_T("\n多段线在点(%.3f,%.3f)处有多余顶点，已删除！"), lineCur.endPoint().x, lineCur.endPoint().y) ;
						pPolyLine->upgradeOpen() ;
						pPolyLine->removeVertexAt(j+1) ;
						pPolyLine->setColorIndex(3) ;
						pPolyLine->downgradeOpen() ;
						break ;
					}
				}
			}

			pEnt->close() ;
		}
	}	
	if (bHasErro==TRUE)
	{
		strMsg.Format(_T("发现有多余顶点的多段线,已标示，请先确认后再运行此命令。")) ;
		AfxMessageBox(strMsg) ;
		return ;
	}

	//首或尾不在道岔反位的线全部标示出来	
	strHasFinishPt = _T("") ;
	CLongArray nArrayHandleAllDc ;
	int iNumAllDc = this->GetAllDc(nArrayHandleAllDc) ;

	for (int i=0; i<iNumAllDc; i++)
	{
		long nHandleCurDc = nArrayHandleAllDc.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandleCurDc, pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;

				CLongArray nArrayHandleIntersectLine, nArrayIndexOfVertex ;
				AcGePoint3dArray pt3dArrayIntersect ;
				int iDwIntersectMoveToVertex = 0 ;

				this->GetDcConLineDFW(pRef, nArrayHandleIntersectLine, nArrayIndexOfVertex, pt3dArrayIntersect, iDwIntersectMoveToVertex) ;
				if(nArrayHandleIntersectLine.GetCount()>1&&nArrayIndexOfVertex.GetCount()>1)
				{
					long nHandleFw = nArrayHandleIntersectLine.GetAt(1) ;
					long nVertex = nArrayIndexOfVertex.GetAt(1) ;
					CString strCurPt ;
					strCurPt.Format(_T("%d-%d"), nHandleFw, nVertex==0?0:1) ;
					strHasFinishPt.AppendFormat(_T("%s,"), strCurPt) ;
				}
			}
			pEnt->close() ;
		}
	}
	int iNumNotConFw = 0 ;
	for (int i=0; i<nLen; i++)
	{
		long nHandleLine = nArrayHandle.GetAt(i) ;
		CString strCurPt ;
		for (int j=0; j<2; j++)
		{
			strCurPt.Format(_T("%d-%d"), nHandleLine, j) ;
			if (strHasFinishPt.Find(strCurPt)==-1)
			{
				m_mysys.SetEntColorIndex(nHandleLine, 3) ;
				iNumNotConFw++ ;
				break;
			}
		}
	}
	strMsg.Format(_T("发现%d条线，其起点或终点不在道岔反位，已标示，请注意检查。"), iNumNotConFw) ;
	acutPrintf(_T("\n%s"),strMsg) ;
	AfxMessageBox(strMsg) ;
}

// 当前图纸所有道岔
int CZnSxt::GetAllDc(CLongArray& nArrayHandleDc)
{
	int iRet = 0 ;

	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);
	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	CString strSql ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;	
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;

				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;				
				if (strBlkName.Left(2)==_T("DC"))
				{
					UINT nHandle = m_mysys.GetHandle(pEnt) ;
					nArrayHandleDc.Add(nHandle) ;
					iRet++ ;
				}
			}

			pEnt->close() ;
		}	
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return iRet ;
}

// 在相应的线段上绘制电缆信息字符串（从SetCableString分离出来，为复用）
int CZnSxt::DrawCableString(long nHandleCableLine, const CString& strCableString, int iStringType)
{
	AcDbEntity* pEnt = NULL ;
	if(m_mysys.OpenAcDbEntity(nHandleCableLine, pEnt, AcDb::kForRead))
	{
		AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
		AcGeLineSeg3d lineSeg ;
		int iIndexVertMax = 0 ;
		double dLenMax = 0 ;
		for (int i=0; i<pPolyLine->numVerts()-1; i++)
		{
			pPolyLine->getLineSegAt(i, lineSeg) ;
			double dLen = lineSeg.length() ;
			if (dLen>dLenMax)
			{
				dLenMax = dLen ;
				iIndexVertMax = i ;
			}
		}

		double dHeight = 2, dWidthFactor = 0.7, dDistToLine = 1.5 ;
		AcDb::TextHorzMode txthorzmod = AcDb::kTextMid ;
		AcDb::TextVertMode txtvertmod = AcDb::kTextBase ;

		pPolyLine->getLineSegAt(iIndexVertMax, lineSeg) ;
		AcGeVector3d vec = lineSeg.direction() ;
		AcGePoint3d pt_3d_mid = lineSeg.midPoint() ;
		AcGePoint3d pt_3d_midtmp = pt_3d_mid ;
		double dAngleTextToLine = vec.angleOnPlane(AcGePlane::kXYPlane)+PI/2 ;

		//连接线小于文本长度的（比如转辙机箱盒之间），需要特殊处理
		double dTextLen = m_mysys.GetTextLen(strCableString, 2, 0.7) ;
		if (dTextLen>dLenMax&&iStringType==0) 
		{
			//begin 本段代码一般对转辙机有用
			double dAngleJxg = 0.00012345 ;

			if (m_iCurDljlType==0)
			{
				long nHandleJXG = 0 ;
				AcGePoint3d pt_3d_start ;
				pPolyLine->getStartPoint(pt_3d_start) ;
				CLongArray nArrayHandleFindBox ;
				CZnUtility::FindConBox(pt_3d_start, nArrayHandleFindBox) ;
				int iNumFind = nArrayHandleFindBox.GetCount() ;
				if (iNumFind==1)
				{
					if (m_mysys.GetXdata(nArrayHandleFindBox.GetAt(0), _T("BELONG_ENT"), nHandleJXG))
					{
						dAngleJxg = m_mysys.GetAngle(nHandleJXG) ;
					}
				}
			}

			//end 本段代码一般对转辙机有用

			dHeight = 1 ;
			dWidthFactor = 0.5 ;
			dDistToLine = 2.5 ;
			pPolyLine->getStartPoint(pt_3d_midtmp) ;
			if (dAngleTextToLine>=PI/2&&dAngleTextToLine<=PI||dAngleTextToLine>=2*PI&&dAngleTextToLine<=5*PI/2)
			{
				txthorzmod = AcDb::kTextRight ;
				if (dAngleJxg>PI/2&&dAngleJxg<=3*PI/2)
				{
					txtvertmod = AcDb::kTextTop ;
				}
			}
			else
			{
				dAngleTextToLine-= PI ;
				txthorzmod = AcDb::kTextLeft ;
				if (dAngleJxg>PI/2&&dAngleJxg<=3*PI/2)
				{
					txtvertmod = AcDb::kTextTop ;
				}
			}
			if (dAngleJxg!=0.00012345)
			{
				if (dAngleJxg>PI/2&&dAngleJxg<=3*PI/2)
				{
					dDistToLine*=-1 ;
				}
			}

			//连接线小于文本长度的（比如转辙机箱盒之间），需要绘制标注线

			AcGePoint3d pt_3d_dim[4] ;				
			m_mysys.GetPoint(pt_3d_midtmp, dDistToLine>0?dDistToLine-0.2:dDistToLine+0.2, dAngleTextToLine, pt_3d_dim[1]) ;
			m_mysys.GetPoint(pt_3d_dim[1], 3, vec.angleOnPlane(AcGePlane::kXYPlane)+PI, pt_3d_dim[0]) ;
			pt_3d_dim[3] = pt_3d_mid ;
			pt_3d_dim[2] = m_mysys.GetMidPoint(pt_3d_dim[1], pt_3d_dim[3]) ;
			AcGePoint3dArray pt3dArrayDim ;
			for (int i=0; i<4; i++)
			{
				pt3dArrayDim.append(pt_3d_dim[i]) ;
			}
			//if (iStringType==0)
			//{
				AcDbObjectId objIdDim = m_mysys.AddAcDbPolyLine(pt3dArrayDim, 0, m_iColorIndexCur) ;
				this->SetLayer(objIdDim, m_iCurDljlType) ;
				AcDbEntity* pEntDim = NULL ;
				if (m_mysys.OpenAcDbEntity(objIdDim, pEntDim))
				{
					AcDbPolyline* pPolyLineDim = AcDbPolyline::cast(pEntDim) ;
					pPolyLineDim->setWidthsAt(2, 0.2, 0) ;
					m_mysys.SetXdata(pEntDim, _T("IS_CABLESTRING_DIM"), 1) ;
					m_mysys.SetXdata(pEntDim, _T("BELONG_ENT"), nHandleCableLine) ;
					pEntDim->close() ;
				}
			//}
		}

		AcGePoint3d pt_3d_text ;
		m_mysys.GetPoint(pt_3d_midtmp, dDistToLine, dAngleTextToLine, pt_3d_text) ;
		if (iStringType==0)
		{
			AcDbObjectId objId ;
			m_mysys.AddText(objId, strCableString, pt_3d_text, pt_3d_text, txthorzmod, txtvertmod, dHeight, dWidthFactor, vec.angleOnPlane(AcGePlane::kXYPlane), m_iColorIndexCur) ;
			this->SetLayer(objId, m_iCurDljlType) ;
			m_mysys.SetXdata(objId, _T("IS_CABLEINFO"), 1) ;
			m_mysys.SetXdata(objId, _T("BELONG_ENT"), nHandleCableLine) ;
		}
		else if (iStringType==1)
		{
			AcGeVector3d vec2 = pt_3d_midtmp - pt_3d_text ;
			vec2*=2 ;
			AcGePoint3d pt_3d_text2 ;
			m_mysys.GetPoint(pt_3d_text, vec2.length(), vec2.angleOnPlane(AcGePlane::kXYPlane), pt_3d_text2) ;
			AcDbObjectId objId2 ;
			m_mysys.AddText(objId2, strCableString, pt_3d_text2, pt_3d_text2, txthorzmod, txtvertmod, 1.5, dWidthFactor, vec.angleOnPlane(AcGePlane::kXYPlane), m_iColorIndexCur) ;
			this->SetLayer(objId2, m_iCurDljlType) ;
			m_mysys.SetXdata(objId2, _T("IS_CABLESN"), 1) ;
			m_mysys.SetXdata(objId2, _T("BELONG_ENT"), nHandleCableLine) ;
		}

		pEnt->close() ;
	}

	return 0;
}

// 找出属于指定handle的实体，并递归属于他的所有实体, iType=0不递归，只找一层
void CZnSxt::FindEntBelong(long nHandleSrc, CLongArray& nArrayHandleBelong, int iType)
{
	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			long nHandleTmp = 0 ;
			m_mysys.GetXdata(pEnt, _T("BELONG_ENT"), nHandleTmp) ;
			if (nHandleTmp==nHandleSrc)
			{
				long nHandleCur = m_mysys.GetHandle(pEnt) ;
				nArrayHandleBelong.Add(nHandleCur) ;
				if (iType>0)
				{
					FindEntBelong(nHandleCur, nArrayHandleBelong, iType) ;
				}
			}
			pEnt->close() ;
		}
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;
}

 // 取得信号机/道岔/轨道属性数据
void CZnSxt::GetEntData(AcDbEntity* pEnt, CEntDATA& entData)
{
	CString strClassName = entData.GetClassType() ;
	CString strXdata ;
	long iXdata = 0 ;
	double dXdata=0;
	if (strClassName==_T("CXhjDATA"))
	{
		CXhjDATA *pDATA = dynamic_cast<CXhjDATA*>(&entData) ;
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.GetAttValue(pRef, _T("XHJNAME"), pDATA->m_strName) ;
		if(m_mysys.GetXdata(pEnt, _T("LOC"), iXdata))
		{
			pDATA->m_nLoc = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISTRAIN"), iXdata ) )
		{
			pDATA->m_bIsTrain = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("BOXTYPE"), strXdata) )
		{
			pDATA->m_strBoxType = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("CORENUMI"), iXdata) )
		{
			pDATA->m_nCoreNumI = iXdata ;
		}						
		if (m_mysys.GetXdata(pEnt, _T("CORENUMII"), iXdata) )
		{
			pDATA->m_nCoreNumII= iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("BOXPOS"), strXdata))
		{
			pDATA->m_strBoxPos= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("HASDDUNIT"), iXdata))
		{
			pDATA->m_bHasDDUnit = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISALU"), iXdata) )
		{
			pDATA->m_bIsAlu = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("HASPHONE"), iXdata) )
		{
			pDATA->m_bHasPhone = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("DSNUM"), iXdata) )
		{
			pDATA->m_nDSNum = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("SPEEDLIMIT"), iXdata) )
		{
			pDATA->m_nSpeedLimit = iXdata ;
		}
	}
	else 	if (strClassName==_T("CGdDATA"))
	{
		CGdDATA *pDATA = dynamic_cast<CGdDATA*>(&entData) ;
		if(m_mysys.GetXdata(pEnt, _T("GUIDAONAME"), strXdata))
		{
			pDATA->m_strName = strXdata ;
		}						
		if(m_mysys.GetXdata(pEnt, _T("CIRCUITTYPE"), strXdata))
		{
			pDATA->m_strCircuitType = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("DEVOFHZ25"), strXdata ) )
		{
			pDATA->m_str25HzDev = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISDH"), iXdata) )
		{
			pDATA->m_bIsDH = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISDMH"), iXdata) )
		{
			pDATA->m_bIsDMH = iXdata ;
		}						
		if (m_mysys.GetXdata(pEnt, _T("IS4LINE"), iXdata) )
		{
			pDATA->m_bIs4Line= iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISSHAREXB"), iXdata))
		{
			pDATA->m_bIsShareXB= iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("IS1FNJ"), iXdata))
		{
			pDATA->m_bIs1FnJ = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISMAINSEC"), iXdata) )
		{
			pDATA->m_bIsMainSec = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("LEFT_FORS"), strXdata) )
		{
			pDATA->m_strLeftFOrS= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("RIGHT_FORS"), strXdata) )
		{
			pDATA->m_strRightFOrS= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("FREQ_LEFT"), strXdata) )
		{
			pDATA->m_strFreqLeft = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("FREQ_RIGHT"), strXdata) )
		{
			pDATA->m_strFreqRight = strXdata ;
		}
	}
	else 	if (strClassName==_T("CDcDATA"))
	{
		CDcDATA *pDATA = dynamic_cast<CDcDATA*>(&entData) ;
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.GetAttValue(pRef, _T("BIANHAO"), pDATA->m_strName) ;
		if(m_mysys.GetXdata(pEnt, _T("LOC"), iXdata))
		{
			pDATA->m_nLoc = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("JGQYNUM"), iXdata ) )
		{
			if (iXdata==0)
			{
				iXdata = 1 ;
			}
			pDATA->m_nJG_QyNum = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("JGQYTYPE"), strXdata) )
		{
			if (!strXdata.IsEmpty())
			{
				pDATA->m_strJG_QyType = strXdata ;
			}
		}
		if (m_mysys.GetXdata(pEnt, _T("JGHASMT"), iXdata) )
		{
			pDATA->m_bJG_HasMT = iXdata ;
		}						
		if (m_mysys.GetXdata(pEnt, _T("JGQYPOS"), strXdata) )
		{
			pDATA->m_strJG_QyPos= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("XGQYNUM"), iXdata ) )
		{
			pDATA->m_nXG_QyNum = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("XGQYTYPE"), strXdata) )
		{
			pDATA->m_strXG_QyType = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("XGHASMT"), iXdata) )
		{
			pDATA->m_bXG_HasMT = iXdata ;
		}						
		if (m_mysys.GetXdata(pEnt, _T("XGQYPOS"), strXdata) )
		{
			pDATA->m_strXG_QyPos= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("BOXTYPE"), strXdata))
		{
			pDATA->m_strBoxType= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISSINGLEACT"), iXdata))
		{
			pDATA->m_bIsSingleAct = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISCOMPLEX"), iXdata) )
		{
			pDATA->m_bIsComplex = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("RELATIONDC"), strXdata) )
		{
			pDATA->m_strRelationDc = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ACTINDEX"), iXdata) )
		{
			pDATA->m_nActIndex = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("HASPHONE"), iXdata) )
		{
			pDATA->m_bHasPhone = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("HASDCQKBJ"), iXdata) )
		{
			pDATA->m_bHasDCQKBJ = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("SPEEDLIMIT"), iXdata) )
		{
			pDATA->m_nSpeedLimit = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("CUTJYJPOS"), iXdata) )
		{
			pDATA->m_iCutJYJPos= iXdata ;
		}
	}
	else 	if (strClassName==_T("CDtDcDATA"))
	{
		CDtDcDATA *pDATA = dynamic_cast<CDtDcDATA*>(&entData) ;
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.GetAttValue(pRef, _T("BIANHAO"), pDATA->m_strName) ;
		if(m_mysys.GetXdata(pEnt, _T("LOC"), iXdata))
		{
			pDATA->m_nLoc = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("JGQYNUM"), iXdata ) )
		{
			if (iXdata==0)
			{
				iXdata = 1 ;
			}
			pDATA->m_nJG_QyNum = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("JGQYTYPE"), strXdata) )
		{
			if (!strXdata.IsEmpty())
			{
				pDATA->m_strJG_QyType = strXdata ;
			}
		}					
		if (m_mysys.GetXdata(pEnt, _T("JGQYPOS"), strXdata) )
		{
			pDATA->m_strJG_QyPos= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("DCHASBOX"), iXdata) )
		{
			pDATA->m_bDC_HasBox = iXdata ;
		}	
		if (m_mysys.GetXdata(pEnt, _T("DCSHAREBOX"), iXdata ) )
		{
			pDATA->m_bDC_ShareBox = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("DCBOXTYPE"), strXdata) )
		{
			pDATA->m_strDC_BoxType = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("DCBOXPOS"), strXdata) )
		{
			pDATA->m_strDC_BoxPos = strXdata ;
		}						
		if (m_mysys.GetXdata(pEnt, _T("CABLESTRING"), strXdata) )
		{
			pDATA->m_strCableString= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("BOXTYPE"), strXdata))
		{
			pDATA->m_strBoxType= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISSINGLEACT"), iXdata))
		{
			pDATA->m_bIsSingleAct = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ISCOMPLEX"), iXdata) )
		{
			pDATA->m_bIsComplex = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("RELATIONDC"), strXdata) )
		{
			pDATA->m_strRelationDc = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("ACTINDEX"), iXdata) )
		{
			pDATA->m_nActIndex = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("HASPHONE"), iXdata) )
		{
			pDATA->m_bHasPhone = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("HASDCQKBJ"), iXdata) )
		{
			pDATA->m_bHasDCQKBJ = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("SPEEDLIMIT"), iXdata) )
		{
			pDATA->m_nSpeedLimit = iXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("CUTJYJPOS"), iXdata) )
		{
			pDATA->m_iCutJYJPos= iXdata ;
		}
	}
	else 	if (strClassName==_T("CCableImportDATA"))
	{
		CCableImportDATA *pDATA = dynamic_cast<CCableImportDATA*>(&entData) ;
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.GetBlockName(pRef,pDATA->m_strBlkName);
		if(m_mysys.GetXdata(pEnt, _T("LOCATION"), dXdata))
		{
			//this->GetBoxLocation()
			pDATA->d_nLoc = dXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("STATIONNAME"), strXdata ) )
		{
			pDATA->m_strStation = strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("DISTTOROOM"), dXdata) )
		{
			pDATA->m_dDistToDeviceRoom = dXdata ;	
		}
		if (m_mysys.GetXdata(pEnt, _T("ISJZ"), iXdata) )
		{
			pDATA->m_bIsJZ = iXdata ;
		}						
		if (m_mysys.GetXdata(pEnt, _T("ISUP"), iXdata) )
		{
			pDATA->m_IsUP= iXdata ;
		}
	}
	else if(strClassName==_T("CXhDevice"))
	{
		CXhDevice *pDATA = dynamic_cast<CXhDevice*>(&entData) ;
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		
		m_mysys.GetAttValue(pRef, _T("NAME"), pDATA->m_strName) ;
		if(pDATA->m_strName==_T(""))
		{
			m_mysys.GetAttValue(pRef, _T("NAME"), pDATA->m_strName) ;
		}
		m_mysys.GetBlockName(pRef,pDATA->m_strBlkName);
		if(m_mysys.GetXdata(pEnt, _T("LOCATION"), dXdata))
		{
			//this->GetBoxLocation()
			pDATA->m_dLoc= dXdata ;
		}
		if(m_mysys.GetXdata(pEnt,_T("HAS_BOX"),iXdata))
		{
			pDATA->m_bHasBox=iXdata;
		}
		if(m_mysys.GetXdata(pEnt,_T("BOX_TYPE"),strXdata))
		{
			pDATA->m_strBoxType=strXdata;
		}
		if(m_mysys.GetXdata(pEnt,_T("BOX_POS"),strXdata))
		{
			pDATA->m_strBoxPos=strXdata;
		}
		if(m_mysys.GetXdata(pEnt,_T("CABLE_NUM"),iXdata))
		{
			pDATA->m_nCable=iXdata;
		}
		if(m_mysys.GetXdata(pEnt,_T("CORE_NUM1"),iXdata))
		{
			pDATA->m_nCoreNum1=iXdata;
		}
		if(m_mysys.GetXdata(pEnt,_T("CORE_NUM2"),iXdata))
		{
			pDATA->m_nCoreNum2=iXdata;
		}
		if (m_mysys.GetXdata(pEnt, _T("CABLESTRING1"), strXdata ) )
		{
			pDATA->m_strCable1= strXdata ;
		}
		if (m_mysys.GetXdata(pEnt, _T("CABLESTRING2"), strXdata ) )
		{
			pDATA->m_strCable2= strXdata ;
		}
		if(m_mysys.GetXdata(pEnt,_T("HAS_PHONE"),iXdata))
		{
			pDATA->m_bHasPhone=iXdata;
		}
		if(m_mysys.GetXdata(pEnt,_T("HAS_GT"),iXdata))
		{
			pDATA->m_bHasGT=iXdata;
		}
		if(m_mysys.GetXdata(pEnt,_T("HAS_GROUND"),iXdata))
		{
			pDATA->m_bHasGround=iXdata;
		}
	}
}
// 取得信号机/道岔/轨道属性数据
void CZnSxt::GetEntData(long nHandle, CEntDATA& entData)
{
	if (nHandle>0)
	{
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			this->GetEntData(pEnt, entData) ;
			pEnt->close() ;
		}
	}
}

// 设置信号机/道岔/轨道属性数据
void CZnSxt::SetEntData(AcDbEntity* pEnt, CEntDATA* pEntData)
{
	CString strClassName = pEntData->GetClassType() ;
	CString strXdata ;
	long iXdata = 0 ;
	if (strClassName==_T("CXhjDATA"))
	{
		CXhjDATA *pDATA = dynamic_cast<CXhjDATA*>(pEntData);
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.SetAttValue(pRef, _T("XHJNAME"),  pDATA->m_strName ) ;
		m_mysys.SetXdata(pEnt, _T("LOC"), pDATA->m_nLoc) ;							
		m_mysys.SetXdata(pEnt, _T("ISTRAIN"), (long)(pDATA->m_bIsTrain)) ;
		m_mysys.SetXdata(pEnt, _T("BOXTYPE"), pDATA->m_strBoxType) ;
		m_mysys.SetXdata(pEnt, _T("CORENUMI"), pDATA->m_nCoreNumI) ;
		m_mysys.SetXdata(pEnt, _T("CORENUMII"), pDATA->m_nCoreNumII) ;				
		m_mysys.SetXdata(pEnt, _T("BOXPOS"), pDATA->m_strBoxPos) ;
		m_mysys.SetXdata(pEnt, _T("HASDDUNIT"), (long)(pDATA->m_bHasDDUnit));
		m_mysys.SetXdata(pEnt, _T("ISALU"), (long)(pDATA->m_bIsAlu)) ;
		m_mysys.SetXdata(pEnt, _T("HASPHONE"), (long)(pDATA->m_bHasPhone)) ;
		m_mysys.SetXdata(pEnt, _T("DSNUM"), pDATA->m_nDSNum) ;
		m_mysys.SetXdata(pEnt, _T("SPEEDLIMIT"), pDATA->m_nSpeedLimit) ;
	}
	else 	if (strClassName==_T("CGdDATA"))
	{
		CGdDATA *pDATA = dynamic_cast<CGdDATA*>(pEntData) ;
		m_mysys.SetXdata(pEnt, _T("GUIDAONAME"), pDATA->m_strName) ;
		m_mysys.SetXdata(pEnt, _T("CIRCUITTYPE"), pDATA->m_strCircuitType) ;
		m_mysys.SetXdata(pEnt, _T("DEVOFHZ25"), pDATA->m_str25HzDev) ;
		m_mysys.SetXdata(pEnt, _T("ISDH"), (long)(pDATA->m_bIsDH)) ;
		m_mysys.SetXdata(pEnt, _T("ISDMH"), (long)(pDATA->m_bIsDMH)) ;
		m_mysys.SetXdata(pEnt, _T("IS4LINE"), (long)(pDATA->m_bIs4Line)) ;
		m_mysys.SetXdata(pEnt, _T("ISSHAREXB"), (long)(pDATA->m_bIsShareXB)) ;
		m_mysys.SetXdata(pEnt, _T("IS1FNJ"), (long)(pDATA->m_bIs1FnJ));
		m_mysys.SetXdata(pEnt, _T("ISMAINSEC"), (long)(pDATA->m_bIsMainSec)) ;
		m_mysys.SetXdata(pEnt, _T("LEFT_FORS"), pDATA->m_strLeftFOrS) ;
		m_mysys.SetXdata(pEnt, _T("RIGHT_FORS"), pDATA->m_strRightFOrS) ;
		m_mysys.SetXdata(pEnt, _T("FREQ_LEFT"), pDATA->m_strFreqLeft) ;
		m_mysys.SetXdata(pEnt, _T("FREQ_RIGHT"), pDATA->m_strFreqRight) ;
	}
	else 	if (strClassName==_T("CDcDATA"))
	{
		CDcDATA *pDATA = dynamic_cast<CDcDATA*>(pEntData) ;
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.SetAttValue(pRef, _T("BIANHAO"), pDATA->m_strName) ;
		m_mysys.SetXdata(pEnt, _T("LOC"), pDATA->m_nLoc) ;	
		m_mysys.SetXdata(pEnt, _T("JGQYNUM"), pDATA->m_nJG_QyNum ) ;
		m_mysys.SetXdata(pEnt, _T("JGQYTYPE"), pDATA->m_strJG_QyType) ;
		m_mysys.SetXdata(pEnt, _T("JGHASMT"), (long)(pDATA->m_bJG_HasMT)) ;
		m_mysys.SetXdata(pEnt, _T("JGQYPOS"), pDATA->m_strJG_QyPos) ;
		m_mysys.SetXdata(pEnt, _T("XGQYNUM"), pDATA->m_nXG_QyNum ) ;
		m_mysys.SetXdata(pEnt, _T("XGQYTYPE"), pDATA->m_strXG_QyType) ;
		m_mysys.SetXdata(pEnt, _T("XGHASMT"), (long)pDATA->m_bXG_HasMT) ;
		m_mysys.SetXdata(pEnt, _T("XGQYPOS"), pDATA->m_strXG_QyPos) ;
		m_mysys.SetXdata(pEnt, _T("BOXTYPE"), pDATA->m_strBoxType) ;
		m_mysys.SetXdata(pEnt, _T("ISSINGLEACT"), (long)(pDATA->m_bIsSingleAct)) ;
		m_mysys.SetXdata(pEnt, _T("ISCOMPLEX"), (long)(pDATA->m_bIsComplex)) ;
		m_mysys.SetXdata(pEnt, _T("RELATIONDC"), pDATA->m_strRelationDc) ;
		m_mysys.SetXdata(pEnt, _T("ACTINDEX"), pDATA->m_nActIndex) ;
		m_mysys.SetXdata(pEnt, _T("HASPHONE"), (long)(pDATA->m_bHasPhone)) ;
		m_mysys.SetXdata(pEnt, _T("HASDCQKBJ"), (long)(pDATA->m_bHasDCQKBJ)) ;
		m_mysys.SetXdata(pEnt, _T("SPEEDLIMIT"), pDATA->m_nSpeedLimit) ;
		m_mysys.SetXdata(pEnt, _T("CUTJYJPOS"), pDATA->m_iCutJYJPos) ;
	}
	else if(strClassName==_T("CDtDcDATA"))
	{
		CDtDcDATA *pDATA = dynamic_cast<CDtDcDATA*>(pEntData) ;
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.SetAttValue(pRef, _T("BIANHAO"), pDATA->m_strName) ;
		m_mysys.SetXdata(pEnt, _T("LOC"), pDATA->m_nLoc) ;	
		m_mysys.SetXdata(pEnt, _T("JGQYNUM"), pDATA->m_nJG_QyNum ) ;
		m_mysys.SetXdata(pEnt, _T("JGQYTYPE"), pDATA->m_strJG_QyType) ;
		m_mysys.SetXdata(pEnt, _T("JGQYPOS"), pDATA->m_strJG_QyPos) ;
		m_mysys.SetXdata(pEnt, _T("DCHASBOX"), pDATA->m_bDC_HasBox) ;
		m_mysys.SetXdata(pEnt, _T("DCSHAREBOX"), pDATA->m_bDC_ShareBox) ;
		m_mysys.SetXdata(pEnt, _T("DCBOXTYPE"), pDATA->m_strDC_BoxType) ;
		m_mysys.SetXdata(pEnt, _T("DCBOXPOS"), pDATA->m_strDC_BoxPos) ;
		m_mysys.SetXdata(pEnt, _T("CABLESTRING"), pDATA->m_strCableString) ;
		m_mysys.SetXdata(pEnt, _T("BOXTYPE"), pDATA->m_strBoxType) ;
		m_mysys.SetXdata(pEnt, _T("ISSINGLEACT"), (long)(pDATA->m_bIsSingleAct)) ;
		m_mysys.SetXdata(pEnt, _T("ISCOMPLEX"), (long)(pDATA->m_bIsComplex)) ;
		m_mysys.SetXdata(pEnt, _T("RELATIONDC"), pDATA->m_strRelationDc) ;
		m_mysys.SetXdata(pEnt, _T("ACTINDEX"), pDATA->m_nActIndex) ;
		m_mysys.SetXdata(pEnt, _T("HASPHONE"), (long)(pDATA->m_bHasPhone)) ;
		m_mysys.SetXdata(pEnt, _T("HASDCQKBJ"), (long)(pDATA->m_bHasDCQKBJ)) ;
		m_mysys.SetXdata(pEnt, _T("SPEEDLIMIT"), pDATA->m_nSpeedLimit) ;
		m_mysys.SetXdata(pEnt, _T("CUTJYJPOS"), pDATA->m_iCutJYJPos) ;
	}
	else if (strClassName==_T("CCableImportDATA"))
	{
		CCableImportDATA *pDATA = dynamic_cast<CCableImportDATA*>(pEntData);
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		m_mysys.SetXdata(pEnt, _T("STATIONNAME"), pDATA->m_strStation) ;							
		m_mysys.SetXdata(pEnt, _T("ISJZ"), (long)(pDATA->m_bIsJZ)) ;
		m_mysys.SetXdata(pEnt, _T("ISUP"), pDATA->m_IsUP) ;
		m_mysys.SetXdata(pEnt, _T("DISTTOROOM"), pDATA->m_dDistToDeviceRoom) ;
		m_mysys.SetXdata(pEnt, _T("LOCATION"), pDATA->d_nLoc) ;				
	}
	else if (strClassName==_T("CXhDevice"))
	{
		CXhDevice *pDATA = dynamic_cast<CXhDevice*>(pEntData);
		AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
		long nhandle_dev=0;
		m_mysys.GetXdata(pEnt,_T("CloneFrom"),nhandle_dev);
// 		if(nhandle_dev==0)
// 		{
// 			nhandle_dev=m_mysys.GetHandle(pRef);
// 		}
		//long nhandle_dev=m_mysys.GetHandle(pRef);
 		CString strloc;
		GetBoxLocationForPMT(nhandle_dev,strloc,2);
		CBlkUtility::CStringToDouble(strloc,pDATA->m_dLoc);
		m_mysys.SetXdata(pEnt, _T("LOCATION"), pDATA->m_dLoc) ;
		m_mysys.SetXdata(pEnt,_T("HAS_BOX"),(long)pDATA->m_bHasBox);
		m_mysys.SetXdata(pEnt,_T("BOX_TYPE"),pDATA->m_strBoxType);
		m_mysys.SetXdata(pEnt,_T("BOX_POS"),pDATA->m_strBoxPos);
		m_mysys.SetXdata(pEnt,_T("CABLE_NUM"),(long)pDATA->m_nCable);
		m_mysys.SetXdata(pEnt,_T("CORE_NUM1"),(long)pDATA->m_nCoreNum1);
		m_mysys.SetXdata(pEnt,_T("CORE_NUM2"),(long)pDATA->m_nCoreNum2);
		m_mysys.SetXdata(pEnt, _T("CABLESTRING2"), pDATA->m_strCable2) ;
		m_mysys.SetXdata(pEnt, _T("CABLESTRING1"), pDATA->m_strCable1) ;	
		m_mysys.SetXdata(pEnt,_T("HAS_PHONE"),(long)pDATA->m_bHasPhone);
		m_mysys.SetXdata(pEnt,_T("HAS_GT"),(long)pDATA->m_bHasGT);
		m_mysys.SetXdata(pEnt,_T("HAS_GROUND"),(long)pDATA->m_bHasGround);
	}
}
// 设置信号机/道岔/轨道属性数据
void CZnSxt::SetEntData(long nHandle, CEntDATA* pEntData)
{
	if (nHandle>0)
	{
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForWrite))
		{
			this->SetEntData(pEnt, pEntData) ;
			pEnt->close() ;
		}
	}
}

// 清除BELONG_ENT属性为nHandleBelong的实体, iType=0不递归，只找一层
int CZnSxt::ClearEnt(long nHandleBelong, int iType)
{
	int iRet = 0 ;
	CLongArray nArrayHandle ;
	this->FindEntBelong(nHandleBelong, nArrayHandle, iType) ;
	iRet = m_mysys.Erase(nArrayHandle) ;

	return iRet ;
}

//更新关联道岔属性,iMode=1时，是删除关联（将strConDc的关联道岔删除strDc）
int CZnSxt::UpdataConDc(const CString& strDc, const CString& strConDc, int iMode)
{
	CString strMsg ;

	CStringArray strArrayDc ;
	CBlkUtility::DivideString(strConDc, _T(","), strArrayDc) ;
	//strArrayDc.InsertAt(0, strCurDc) ;

	if (iMode==1)
	{
		for (int i=0; i<strArrayDc.GetCount(); i++)
		{
			CString strDcTmp = strArrayDc.GetAt(i) ;
			CLongArray nArrayHandleTmp ;
			int iNumDc = this->FindDc(strDcTmp, nArrayHandleTmp) ;
			if (iNumDc==1) //找到一个名称为strDcTmp的道岔
			{
				CString strConDcTmp ;
				m_mysys.GetXdata(nArrayHandleTmp.GetAt(0), _T("RELATIONDC"), strConDcTmp) ;
				if (!strConDcTmp.IsEmpty())
				{
					CStringArray strArray1 ;
					CBlkUtility::DivideString(strConDcTmp, _T(","), strArray1) ;
					int iLoc = CBlkUtility::FindStrInArray(strArray1, strDc) ;
					if (iLoc!=-1)
					{
						strArray1.RemoveAt(iLoc) ;
						CString strRet ;
						CBlkUtility::ConvertArrayToString(strArray1, _T(","), strRet) ;
                        m_mysys.SetXdata(nArrayHandleTmp.GetAt(0), _T("RELATIONDC"), strRet) ;
						if (strRet.IsEmpty())
						{
							 m_mysys.SetXdata(nArrayHandleTmp.GetAt(0), _T("ISSINGLEACT"), (long)1) ;
						}
					}
				}
			}
		}
	}
	else
	{
		CLongArray nArrayHandleConDc ;
		for (int i=0; i<strArrayDc.GetCount(); i++)
		{
			CString strDcTmp = strArrayDc.GetAt(i) ;
			if (strDcTmp==strDc)
			{
				strMsg.Format(_T("%s中有和%s相同编号的道岔，将不更新其多动关联字段！"), strConDc, strDcTmp) ;
				AfxMessageBox(strMsg) ;
				return 0 ;
			}
			CLongArray nArrayHandleTmp ;
			int iNumDc = this->FindDc(strDcTmp, nArrayHandleTmp) ;
			if (iNumDc==0)
			{
				strMsg.Format(_T("没有找到编号为%s的道岔，将不更新其多动关联字段！"), strDcTmp) ;
				AfxMessageBox(strMsg) ;
				return 0 ;
			}
			else if (iNumDc>1)
			{
				strMsg.Format(_T("找到多于1个编号为%s的道岔，将不更新其多动关联字段！"), strDcTmp) ;
				AfxMessageBox(strMsg) ;
				return 0 ;
			}
			else if (iNumDc==1)
			{
				nArrayHandleConDc.Add(nArrayHandleTmp.GetAt(0)) ;
			}
		}

		CString strDcSrc = strDc ;
		for (int i=0; i<strArrayDc.GetCount(); i++)
		{
			CString strDcTmp = strArrayDc.GetAt(i) ;
			strArrayDc.SetAt(i, strDcSrc) ;
			strDcSrc = strDcTmp ;
			CString strConDcTmp  ;
			CBlkUtility::ConvertArrayToString(strArrayDc, _T(","), strConDcTmp) ;	
			m_mysys.SetXdata(nArrayHandleConDc.GetAt(i), _T("RELATIONDC"), strConDcTmp) ;		
			m_mysys.SetXdata(nArrayHandleConDc.GetAt(i), _T("ISSINGLEACT"), (long)0) ;	
		}
	}
	
	return 1;
}

// 查找指定名称的道岔
int CZnSxt::FindDc(const CString& strName , CLongArray& nArrayHandle)
{
	int iRet = 0 ;

	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;

				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;				
				if (strBlkName.Left(2)==_T("DC"))
				{
					CString strBianHao ;
					if (m_mysys.GetAttValue(pRef, _T("BIANHAO"), strBianHao))
					{
						if (strBianHao==strName)
						{
							long nHandle = m_mysys.GetHandle(pRef) ;
							nArrayHandle.Add(nHandle) ;
							iRet++ ;
						}
					}
				}
			}

			pEnt->close() ;
		}
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;

	return iRet ;
}

// 道岔几动，共几动（此处的几动是对于网络图而言，非实际）
int CZnSxt::GetDcActionInfo(const CString& strQyType, int iActionNum, int iActionIndex, CString& strActionInfo)
{
		if (strQyType==_T("ZD6-D"))
		{
			strActionInfo.Format(_T("%d/%d"), iActionIndex, iActionNum) ;
		}
		else if (strQyType==_T("ZD6-EJ"))
		{
			strActionInfo.Format(_T("%d/%d"), iActionIndex, iActionNum) ;
		}
		else if (strQyType==_T("ZD6-EJJ"))
		{
			strActionInfo.Format(_T("1/1")) ;
		}
		else if (strQyType==_T("ZYJ7+SH6"))
		{
			strActionInfo.Format(_T("1/1")) ;
		}
		else if (strQyType==_T("ZYJ7+SH6+SH6"))
		{
			strActionInfo.Format(_T("1/1")) ;
		}
		else	if (strQyType==_T("ZYJ7")||strQyType==_T("S700K")||strQyType==_T("ZDJ9"))
		{
			strActionInfo.Format(_T("1/1")) ;
		}

	return 0;
}

// 根据当前径路类型，设置其所在层
int CZnSxt::SetLayer(const AcDbObjectId& objId, int iType)
{
	long nHandle = m_mysys.GetHandle(objId) ;
	this->SetLayer(nHandle, iType) ;
	return 0;
}

// 根据当前径路类型，设置其所在层
int CZnSxt::SetLayer(long nHandle, int iType)
{
	if (iType==0)
	{
		m_mysys.SetLayer(nHandle, _T("CRSC_SJY_XTS_SXT_DLJL_ZZJ")) ;
	}
	else if (iType==1)
	{
		m_mysys.SetLayer(nHandle, _T("CRSC_SJY_XTS_SXT_DLJL_XHJ")) ;
	}
	else if (iType==2)
	{
		m_mysys.SetLayer(nHandle, _T("CRSC_SJY_XTS_SXT_DLJL_GD")) ;
	}
	else if (iType==3)
	{
		m_mysys.SetLayer(nHandle, _T("CRSC_SJY_XTS_SXT_DLJL_DMH")) ;
	}
	return 0;
}

// 显示轨道名称（辅助信息）
int CZnSxt::ShowGdName(AcDbEntity* pEnt)
{
	AcDbObjectId objId = m_pDb->clayer() ;
	m_mysys.SetCLayer( _T("PMT_SHOWINFO_GUIDAONAME")) ;

	if (pEnt->isKindOf(AcDbPolyline::desc()))
	{
		CString strGuiDaoName ;
		if (m_mysys.GetXdata(pEnt, _T("GUIDAONAME"), strGuiDaoName))
		{
			long nHandleGd = m_mysys.GetHandle(pEnt) ;
			CLongArray nArrayBelong ;
			this->FindEntBelong(nHandleGd, nArrayBelong) ;
			//long nHandShowGdName  = 0 ;
			int iNumFind = 0 ;
			for (int i=0; i<nArrayBelong.GetCount(); i++)
			{
				long iXdata = 0 ;
				if (m_mysys.GetXdata(nArrayBelong.GetAt(i), _T("PMT_SHOWINFO_GUIDAONAME"), iXdata)&&iXdata==1)
				{
					//nHandShowGdName = nArrayBelong.GetAt(i) ;
					m_mysys.Erase(nArrayBelong.GetAt(i)) ;
					iNumFind++ ;
				}
			}

			AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;
			AcGePoint3d pt_3d_left(0,0,0), pt_3d_right(0,0,0) ;
			pPolyLine->getPointAt(0, pt_3d_left) ;
			pPolyLine->getPointAt(1, pt_3d_right) ;
			AcGeLineSeg3d lineSeg3d(pt_3d_left, pt_3d_right) ;
			AcGePoint3d pt_3d_mid = lineSeg3d.midPoint() ;
			AcGeVector3d vec = lineSeg3d.direction() ;

			AcDbObjectId objIdGuiDaoName ;
			m_mysys.AddText(objIdGuiDaoName, strGuiDaoName, pt_3d_mid, pt_3d_mid, AcDb::kTextCenter, AcDb::kTextVertMid, 2.5, 0.8, vec.angleOnPlane(AcGePlane::kXYPlane)) ;
			m_mysys.SetXdata(objIdGuiDaoName, _T("PMT_SHOWINFO_GUIDAONAME"), (long)1) ;
			m_mysys.SetXdata(objIdGuiDaoName, _T("BELONG_ENT"), nHandleGd) ;	
		}						
	}

	m_pDb->setClayer(objId) ;

	return 0;
}


// 显示轨道送受电（辅助信息）
int CZnSxt::ShowGdFS(AcDbEntity* pEnt)
{
	AcDbObjectId objId = m_pDb->clayer() ;
	m_mysys.SetCLayer( _T("PMT_SHOWINFO_FSD")) ;

	if (pEnt->isKindOf(AcDbPolyline::desc()))
	{
		CString strGuiDaoName ;
		if (m_mysys.GetXdata(pEnt, _T("GUIDAONAME"), strGuiDaoName))
		{
			long nHandleGd = m_mysys.GetHandle(pEnt) ;
			CLongArray nArrayBelong ;
			this->FindEntBelong(nHandleGd, nArrayBelong) ;
			int iNumFind = 0 ;
			for (int i=0; i<nArrayBelong.GetCount(); i++)
			{
				long iXdata = 0 ;
				if (m_mysys.GetXdata(nArrayBelong.GetAt(i), _T("PMT_SHOWINFO_FSD"), iXdata)&&iXdata==1)
				{
					m_mysys.Erase(nArrayBelong.GetAt(i)) ;
					iNumFind++ ;
				}
			}

			AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;
			AcGePoint3d pt_3d_left(0,0,0), pt_3d_right(0,0,0) ;
			pPolyLine->getStartPoint(pt_3d_left) ;
			pPolyLine->getEndPoint(pt_3d_right) ;
			CString strLeftFORS, strRightFORS  ;
			CString strBlockName ;
			AcDbBlockReference *pRef = NULL ;
			//左侧
			if (m_mysys.GetXdata(pEnt, _T("LEFT_FORS"), strLeftFORS))
			{
				if (strLeftFORS!=_T(""))
				{
					strBlockName.Format(_T("BOX_FSD_%s_0"), strLeftFORS) ;
					pt_3d_left.x+=2.2 ;
					if(m_mysys.InsertBlock(pRef, strBlockName, pt_3d_left))
					{
						m_mysys.SetXdata(pRef, _T("PMT_SHOWINFO_FSD"), 1) ;
						m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleGd) ;
						pRef->close() ;
						pRef = NULL ;
					}
				}
			}	
			//右侧
			if (m_mysys.GetXdata(pEnt, _T("RIGHT_FORS"), strRightFORS))
			{
				if (strRightFORS!=_T(""))
				{
					strBlockName.Format(_T("BOX_FSD_%s_0"), strRightFORS) ;
					pt_3d_right.x-=2.2 ;
					if(m_mysys.InsertBlock(pRef, strBlockName, pt_3d_right))
					{
						m_mysys.SetXdata(pRef, _T("PMT_SHOWINFO_FSD"), 1) ;
						m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleGd) ;	
						pRef->close() ;
						pRef = NULL ;
					}
				}
			}
		}						
	}

	m_pDb->setClayer(objId) ;

	return 0;
}
// 显示隐藏道岔关联的切割绝缘节
int CZnSxt::ShowCutJYJ(long nHandle, bool bShow)
{
	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;	
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;

				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;				
				if (strBlkName.Left(5)==_T("JYJ_JX"))
				{
					int nHandleBelongTmp= 0 ;
					m_mysys.GetXdata(pEnt, _T("BELONGDC"), nHandleBelongTmp) ;
					if (nHandleBelongTmp==nHandle)
					{
						pEnt->upgradeOpen() ;
						pEnt->setVisibility(bShow==true?AcDb::kVisible:AcDb::kInvisible) ;
						pEnt->downgradeOpen() ;
					}
				}
			}

			pEnt->close() ;
		}	
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;
	return 0;
}

// 显示隐藏所有切割绝缘节
int CZnSxt::ShowCutJYJ(bool bShow)
{
	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;	
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference *pRef = AcDbBlockReference::cast(pEnt) ;

				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;				
				if (strBlkName.Left(5)==_T("JYJ_JX"))
				{
					pEnt->upgradeOpen() ;
					pEnt->setVisibility(bShow==true?AcDb::kVisible:AcDb::kInvisible) ;
					pEnt->downgradeOpen() ;
				}
			}

			pEnt->close() ;
		}	
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;
	return 0;
}

int CZnSxt::Test2(void)
{
	CString strSql ;
	CADORecordset cSet(&m_AdoDb) ;
	//begin 计算盒子内芯线的编号 core_sn
	//////////////////////////////////////////////////////////////////////////
	//获取连接电缆的字符串，类似35-12(4)
	CString strCableStr = _T("") ;
	strSql.Format(_T("select dev_name from cable_core_draw where belong_box_handle_low=251 group by dev_name order by Max(pt_y) desc")) ;
	if (cSet.Open(strSql))
	{
		int iCout= cSet.GetRecordCount() ;
		acutPrintf(_T("\n%d"), iCout) ;
		while (!cSet.IsEOF())
		{
			CString strDevName ;
			cSet.GetFieldValue(_T("dev_name"), strDevName) ;
			acutPrintf(_T("\n%s"), strDevName) ;
			cSet.MoveNext() ;
		}
		cSet.Close() ;
	}
	return 0;
}

// 填充电缆芯线定义数据表
int CZnSxt::FillDefineCoreDetail(void)
{
	m_AdoDb.ClearTable(_T("define_core_detail")) ;

	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from define_cable_type ")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nId = 0 ;
				cSet.GetFieldValue(_T("id"), nId) ;

				CString strCoreDetail ;
				cSet.GetFieldValue(_T("core_detail"), strCoreDetail) ;
				int iCoreDetail[4] = {0, 0, 0, 0} ; //分别对应普通芯、对绞，星绞，屏蔽星绞
				CStringArray strArrayCoreDetail ;
				CBlkUtility::DivideString(strCoreDetail, _T("+"), strArrayCoreDetail) ;
				int iCount = strArrayCoreDetail.GetCount() ;
				for (int i=(iCount-1); i>=0; i--)
				{
					CString strCorePart, strPre, strBack ;
					strCorePart = strArrayCoreDetail.GetAt(i) ;
					int iLoc = strCorePart.Find(_T("*")) ;
					if (iLoc!=-1)
					{
						strPre = strCorePart.Left(iLoc) ;
						strBack = strCorePart.Right(strCorePart.GetLength()-iLoc-1) ;
						if (strBack==_T("1"))
						{
							iCoreDetail[0] = _tstoi(strPre) ;
							cSet.Edit(); 
							cSet.SetFieldValue(_T("single_core_num"), iCoreDetail[0]) ;
							cSet.Update() ;
							for (int j=0; j<iCoreDetail[0]; j++)
							{
								strSql.Format(_T("insert into define_core_detail (cable_type_id, core_sn, core_group_type, index_in_group) values(%d, '%d', '1', %d)"), nId, j+1, j) ;
								m_AdoDb.Execute(strSql) ;
							}
						}
						else if (strBack==_T("2"))
						{
							iCoreDetail[1] = _tstoi(strPre) ;
							cSet.Edit(); 
							cSet.SetFieldValue(_T("double_core_num"), iCoreDetail[1]) ;
							cSet.Update() ;
							for (int j=0; j<iCoreDetail[1]; j++)
							{
								for (int m=0; m<2; m++)
								{
									strSql.Format(_T("insert into define_core_detail (cable_type_id, core_sn, core_group_type, index_in_group) values(%d, 'D%d-%d', '2', %d)"), nId, j+1, m+1, j) ;
									m_AdoDb.Execute(strSql) ;
								}
							}
						}
						else if (strBack==_T("4"))
						{
							iCoreDetail[2] = _tstoi(strPre) ;
							cSet.Edit(); 
							cSet.SetFieldValue(_T("four_core_num"), iCoreDetail[2]) ;
							cSet.Update() ;
							for (int j=0; j<iCoreDetail[2]; j++)
							{
								CString strRoman ;
								CBlkUtility::DigToRoman(j+1, strRoman, 0) ;
								for (int m=0; m<4; m++)
								{
									strSql.Format(_T("insert into define_core_detail (cable_type_id, core_sn, core_group_type, index_in_group) values(%d, '%s-%d', '4', %d)"), nId, strRoman,  m+1, j) ;
									m_AdoDb.Execute(strSql) ;
								}
							}
						}
						else if (strBack==_T("4P"))
						{
							iCoreDetail[3] = _tstoi(strPre) ;
							cSet.Edit(); 
							cSet.SetFieldValue(_T("fourp_core_num"), iCoreDetail[3]) ;
							cSet.Update() ;
							for (int j=0; j<iCoreDetail[3]; j++)
							{
								CString strRoman ;
								CBlkUtility::DigToRoman(j+1, strRoman, 0) ;
								for (int m=0; m<4; m++)
								{
									strSql.Format(_T("insert into define_core_detail (cable_type_id, core_sn, core_group_type, index_in_group) values(%d, '%sP-%d', '4P', %d)"), nId, strRoman,  m+1, j) ;
									m_AdoDb.Execute(strSql) ;
								}
							}
						}
					}

				}

				cSet.MoveNext() ;
			}
			cSet.MoveFirst() ;

			while(!cSet.IsEOF())
			{
				int iBackupNum = 0 ;
				long nId = 0 ;
				cSet.GetFieldValue(_T("id"), nId) ;

				CString  strBackupDetail ;			
				cSet.GetFieldValue(_T("backup_detail"), strBackupDetail) ;
				CStringArray strArrayBackupDetail ;
				CBlkUtility::DivideString(strBackupDetail, _T("+"), strArrayBackupDetail) ;
				int iCount = strArrayBackupDetail.GetCount() ;
				for (int i=0; i<iCount; i++)
				{
					CString strCorePart, strPre, strBack ;
					strCorePart = strArrayBackupDetail.GetAt(i) ;
					int iLoc = strCorePart.Find(_T("*")) ;
					if (iLoc!=-1)
					{
						strPre = strCorePart.Left(iLoc) ;
						strBack = strCorePart.Right(strCorePart.GetLength()-iLoc-1) ;
						if (strBack==_T("1"))
						{
							iBackupNum+=_tstoi(strPre) ;
						}
						else if (strBack==_T("2"))
						{
							iBackupNum+=(_tstoi(strPre)*2) ;
						}
						else if (strBack==_T("4"))
						{
							iBackupNum+=(_tstoi(strPre)*4) ;
						}
						else if (strBack==_T("4P"))
						{
							iBackupNum+=(_tstoi(strPre)*4) ;
						}
					}
				}

				//更新define_core_detail:backup
				strSql.Format(_T("update define_core_detail set backup=1 where id in(select top %d id from define_core_detail where cable_type_id=%d order by id desc)"), iBackupNum, nId) ;
				m_AdoDb.Execute(strSql) ;

				cSet.MoveNext() ;
			}


			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in test"));
	}
	return 0;
}

// 更新送受电盒子
int CZnSxt::UpdateFsBox(AcDbEntity* pEnt)
{
	CString strSql ;
	long nHandleLine = m_mysys.GetHandle(pEnt) ;
	for (int i=0; i<2; i++)
	{
		strSql.Format(_T("select * from sxt_ent where sxt_ent_conline_%s=%d and sxt_ent_type\\100=2"), i==0?_T("right"):_T("left"), nHandleLine) ;
		long nHandleJyj = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleJyj) ;
		if (nHandleJyj!=0)
		{
			this->DrawBoxFsE(nHandleJyj) ;
		}
	}
	return 0;
}

// 更新电码化盒子
int CZnSxt::UpdateDmhBox(AcDbEntity* pEnt)
{
	CString strSql ;
	long nHandleLine = m_mysys.GetHandle(pEnt) ;
	for (int i=0; i<2; i++)
	{
		strSql.Format(_T("select * from sxt_ent where sxt_ent_conline_%s=%d and sxt_ent_type\\100=2"), i==0?_T("right"):_T("left"), nHandleLine) ;
		long nHandleJyj = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_handle"), nHandleJyj) ;
		if (nHandleJyj!=0)
		{
			this->DrawBoxDMH(nHandleJyj) ;
		}
	}
	return 0;
}

// 绘制指定绝缘节两端的电码化箱盒
void CZnSxt::DrawBoxDMH(long nHandleJyj)
{
	//先清除原来的
	this->ClearBoxDMH(nHandleJyj) ;

	AcGePoint3d pt_3d_up, pt_3d_down ;
	BOOL bGetPtOfSxtJyj = this->GetPtOfSxtJyj(nHandleJyj, pt_3d_up, pt_3d_down) ;
	if (bGetPtOfSxtJyj==FALSE) //没有双线部分，直接退出，解决了没有双线图时运行此函数会导致平面图信号机等移位问题
	{
		return ;
	}

	//保存当前图层，设置要绘制的图层
	AcDbObjectId ojbIdLayerOld = m_pDb->clayer() ;
	m_mysys.SetCLayer(_T("CRSC_SJY_XTS_SXT_DLJL_DMH"), 4) ;
	m_iColorIndexCur = 256 ;

	// begin 绘制盒子
	CString strSql ;
	strSql.Format(_T("select * from sxt_ent where sxt_ent_handle=%d"), nHandleJyj) ;
	int iUpOrDown = 0 ;
	m_RecordSet.GetRecordValue(strSql, _T("sxt_ent_upordown"), iUpOrDown) ;

	///begin 绝缘节所在线段信息	
	AcGeVector2d vecOnLine = GetAngleOfJyjOnLine(nHandleJyj) ;
	double dAngleOnLine = 0 ;
	dAngleOnLine = vecOnLine.angle() ;
	//end  绝缘节所在线段信息

	//左右轨道
	long nHandleLeftLine = 0, nHandleRightLine = 0 ;
	this->GetGdBesideJyj(nHandleJyj, nHandleLeftLine, nHandleRightLine) ;

	CGdDATA dataGdLeft, dataGdRight ;
	this->GetEntData(nHandleLeftLine, dataGdLeft) ;
	this->GetEntData(nHandleRightLine, dataGdRight) ;

	AcDbBlockReference* pRef = NULL ;
	AcGePoint3d pt_3d_cen; //双线中间线和绝缘节交点位置，此处为厄流插入点
	pt_3d_cen = m_mysys.GetMidPoint(pt_3d_up, pt_3d_down) ;
	double dEntX = pt_3d_cen.x, dEntY = pt_3d_cen.y ;
	CString strBlkName ;
	AcGePoint3d pt_3d_xb(0,0,0) ; //送受电盒子

	//双线中线一段
	AcGePoint2d pt_2d_cen1 = pt_3d_cen.convert2d(AcGePlane::kXYPlane)+vecOnLine ;
	AcGePoint3d pt_3d_cen1(0,0,0) ;
	m_mysys.ConvertPoint(pt_2d_cen1, pt_3d_cen1) ;

	CStringArray strArrayAtt ;

	//绝缘节左边
	if (dataGdLeft.m_strRightFOrS!=_T("N")&&dataGdLeft.m_bIsDMH)
	{
		CString strGd, strFreq ;
		m_mysys.GetXdata(nHandleLeftLine, _T("GUIDAONAME"), strGd) ;
		if (dataGdLeft.m_strRightFOrS!=_T("F"))
		{
			strGd.Append(_T("J")) ;
		}
		m_mysys.GetXdata(nHandleLeftLine, _T("FREQ_RIGHT"), strFreq) ;
		strArrayAtt.Add(strGd) ;
		strArrayAtt.Add(strFreq) ;
		strBlkName.Format(_T("BOX_FSD_%s_1"), dataGdLeft.m_strRightFOrS) ;
		double dBoxX = dEntX-4 ;
		double dBoxY = dEntY+(iUpOrDown==0?(dataGdLeft.m_bIsDH?10:6):-(dataGdLeft.m_bIsDH?10:6)) ;
		pt_3d_xb.x = dBoxX ;
		pt_3d_xb.y = dBoxY ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_xb, &strArrayAtt, FALSE))
		{
			m_mysys.RotationEnt(pRef, dAngleOnLine, pt_3d_cen) ;
			m_mysys.SetXdata(pRef, _T("IS_FOR"), (long)1) ; //0:for轨道电路，1:for电码化
			m_mysys.SetXdata(pRef, _T("BELONG_JYJ"), nHandleJyj) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleLeftLine) ;
			m_mysys.SetXdata(pRef, _T("LEFT_RIGHT"), (long)0) ;
			//m_mysys.SetAttValue(pRef, _T("FREQ"), strFreq, FALSE) ;
			pRef->close() ;
			pRef = NULL ;
		}
	}
	//绝缘节右边
	if (dataGdRight.m_strLeftFOrS!=_T("N")&&dataGdRight.m_bIsDMH)
	{
		CString strGd, strFreq ;
		m_mysys.GetXdata(nHandleRightLine, _T("GUIDAONAME"), strGd) ;
		if (dataGdRight.m_strLeftFOrS!=_T("F"))
		{
			strGd.Append(_T("J")) ;
		}
		m_mysys.GetXdata(nHandleRightLine, _T("FREQ_LEFT"), strFreq) ;
		strArrayAtt.RemoveAll() ;
		strArrayAtt.Add(strGd) ;
		strArrayAtt.Add(strFreq) ;
		strBlkName.Format(_T("BOX_FSD_%s_1"), dataGdRight.m_strLeftFOrS) ;
		double dBoxX = dEntX+4 ;
		double dBoxY = dEntY+(iUpOrDown==0?(dataGdRight.m_bIsDH?10:6):-(dataGdRight.m_bIsDH?10:6)) ;
		pt_3d_xb.x = dBoxX ;
		pt_3d_xb.y = dBoxY ;
		if (m_mysys.InsertBlock(pRef, strBlkName, pt_3d_xb, &strArrayAtt, FALSE))
		{
			m_mysys.RotationEnt(pRef, dAngleOnLine, pt_3d_cen) ;
			m_mysys.SetXdata(pRef, _T("IS_FOR"), (long)1) ; //0:for轨道电路，1:for电码化
			m_mysys.SetXdata(pRef, _T("BELONG_JYJ"), nHandleJyj) ;
			m_mysys.SetXdata(pRef, _T("BELONG_ENT"), nHandleRightLine) ;
			m_mysys.SetXdata(pRef, _T("LEFT_RIGHT"), (long)1) ;
			//m_mysys.SetAttValue(pRef, _T("FREQ"), strFreq, FALSE) ;

			pRef->close() ;
			pRef = NULL ;
		}
	}		
	//end 绘制盒子

	m_pDb->setClayer(ojbIdLayerOld) ; //恢复当前图层
}

// 清除指定的绝缘节两侧电码化箱盒
int CZnSxt::ClearBoxDMH(long nHandleJyj)
{
	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(1001, _T("BELONG_JYJ"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		long nHandle = nArrayHandle.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			long nHandleBelong = 0 ;
			m_mysys.GetXdata(pEnt, _T("BELONG_JYJ"), nHandleBelong) ;
			if (nHandleBelong==nHandleJyj)
			{
				long iIsFor = 0 ;
				m_mysys.GetXdata(pEnt, _T("IS_FOR"), iIsFor) ;
				if (iIsFor==1) //is_for=0表示是轨道电路送受电的盒子等，=1表示电码化的
				{
					pEnt	->upgradeOpen() ;
					pEnt->erase() ;
				}
			}
			pEnt->close() ;
		}		
	}
	return 0 ;
}

void CZnSxt::DrawBoxDMH(void)
{
	try
	{
		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type\\100=2 and ((sxt_ent_type mod 100)\\10)<>5 and sxt_ent_type<>201")) ; //整除为2的,且不是250、251之类的切割绝缘节，也不是201之类的渡线绝缘节
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				this->DrawBoxDMH(nHandle) ;
				cSet.MoveNext() ;
			}
			cSet.Close() ;
		}
	}
	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in DrawBoxDMH"));
	}
}

//取得所有信号楼电缆径路和电缆径路
int CZnSxt::GetCable(CLongArray (&nArrayHandle)[2])
{
	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		//遍历并打开实体 ;	
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			//不可见的实体（自身隐藏或者所在层隐藏）放弃
			if (pEnt->visibility()==AcDb::kInvisible||m_mysys.IsOnOffLayer(pEnt))
			{
				pEnt->close() ;
				continue;
			}

			if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				long nIsCable = 0 ;
				if(m_mysys.GetXdata(pEnt, _T("IS_CABLE"), nIsCable)&&nIsCable==1)
				{
					long nHandleCur = m_mysys.GetHandle(pEnt) ;
					long nIsXhlCable = 0 ;
					if(m_mysys.GetXdata(pEnt, _T("IS_XHLCABLE"), nIsXhlCable)&&nIsXhlCable==1)
					{
						//读取所有信号楼主电缆
						nArrayHandle[0].Add(nHandleCur) ;
					}
					else
					{
						nArrayHandle[1].Add(nHandleCur) ;
					}
				}
			}

			pEnt->close() ;
		}	
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;
	return 0;
}

int CZnSxt::ModEntData(int iType)
{
	if (iType==2)
	{
		struct resbuf* pRb ;
		int nLen = 0 ;
		CLongArray nArrayHandle ;
		pRb = acutBuildList(RTDXF0,_T("LWPOLYLINE"), 1001, _T("ISGUIDAO"),  0) ;
		nLen = m_mysys.GetSSEntHandle(NULL, NULL, NULL, pRb, nArrayHandle) ;
		acutRelRb(pRb) ;
		if (nLen==0)
		{
			return 0 ;
		}

		CEntPropertyDlg dlg ;
		CGdDATA *pDATA = new CGdDATA ;		
		for (int i=0; i<nLen; i++)
		{
			AcDbEntity* pEnt = NULL ;
			long nTemp = nArrayHandle.GetAt(i) ;
			if (m_mysys.OpenAcDbEntity(nTemp, pEnt, AcDb::kForRead))
			{		
				int iTmp = 0 ;
				CString strTmp =_T("") ;

				if(dlg.m_grid.m_iItemIsNull[0]==0&&m_mysys.GetXdata(pEnt, _T("GUIDAONAME"), strTmp))
				{
					if (i==0)
					{
						pDATA->m_strName = strTmp ;
					}
					else if (pDATA->m_strName!=strTmp)
					{
						dlg.m_grid.m_iItemIsNull[0] = 1 ;
					}
				}						
				if(dlg.m_grid.m_iItemIsNull[1]==0&&m_mysys.GetXdata(pEnt, _T("CIRCUITTYPE"), strTmp))
				{				
					if (i==0)
					{
						pDATA->m_strCircuitType = strTmp ;
					}
					else if (pDATA->m_strCircuitType!=strTmp)
					{
						dlg.m_grid.m_iItemIsNull[1] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[2]==0&&m_mysys.GetXdata(pEnt, _T("DEVOFHZ25"), strTmp ) )
				{
					if (i==0)
					{
						pDATA->m_str25HzDev = strTmp ;
					}
					else if (pDATA->m_str25HzDev!=strTmp)
					{
						dlg.m_grid.m_iItemIsNull[2] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[3]==0&&m_mysys.GetXdata(pEnt, _T("ISDH"), iTmp) )
				{
					if (i==0)
					{
						pDATA->m_bIsDH = iTmp ;
					}
					else if (pDATA->m_bIsDH!=iTmp)
					{
						dlg.m_grid.m_iItemIsNull[3] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[4]==0&&m_mysys.GetXdata(pEnt, _T("ISDMH"), iTmp) )
				{
					if (i==0)
					{
						pDATA->m_bIsDMH = iTmp ;
					}
					else if (pDATA->m_bIsDMH!=iTmp)
					{
						dlg.m_grid.m_iItemIsNull[4] = 1 ;
					}
				}						
				if (dlg.m_grid.m_iItemIsNull[5]==0&&m_mysys.GetXdata(pEnt, _T("IS4LINE"), iTmp) )
				{
					if (i==0)
					{
						pDATA->m_bIs4Line = iTmp ;
					}
					else if (pDATA->m_bIs4Line!=iTmp)
					{
						dlg.m_grid.m_iItemIsNull[5] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[6]==0&&m_mysys.GetXdata(pEnt, _T("ISSHAREXB"), iTmp))
				{
					if (i==0)
					{
						pDATA->m_bIsShareXB = iTmp ;
					}
					else if (pDATA->m_bIsShareXB!=iTmp)
					{
						dlg.m_grid.m_iItemIsNull[6] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[7]==0&&m_mysys.GetXdata(pEnt, _T("IS1FNJ"), iTmp))
				{
					if (i==0)
					{
						pDATA->m_bIs1FnJ = iTmp ;
					}
					else if (pDATA->m_bIs1FnJ!=iTmp)
					{
						dlg.m_grid.m_iItemIsNull[7] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[8]==0&&m_mysys.GetXdata(pEnt, _T("ISMAINSEC"), iTmp) )
				{
					if (i==0)
					{
						pDATA->m_bIsMainSec = iTmp ;
					}
					else if (pDATA->m_bIsMainSec!=iTmp)
					{
						dlg.m_grid.m_iItemIsNull[8] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[9]==0&&m_mysys.GetXdata(pEnt, _T("LEFT_FORS"), strTmp) )
				{
					if (i==0)
					{
						pDATA->m_strLeftFOrS = strTmp ;
					}
					else if (pDATA->m_strLeftFOrS!=strTmp)
					{
						dlg.m_grid.m_iItemIsNull[9] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[10]==0&&m_mysys.GetXdata(pEnt, _T("RIGHT_FORS"), strTmp) )
				{
					if (i==0)
					{
						pDATA->m_strRightFOrS = strTmp ;
					}
					else if (pDATA->m_strRightFOrS!=strTmp)
					{
						dlg.m_grid.m_iItemIsNull[10] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[11]==0&&m_mysys.GetXdata(pEnt, _T("FREQ_RIGHT"), strTmp) )
				{				
					if (i==0)
					{
						pDATA->m_strFreqRight = strTmp ;
					}
					else if (pDATA->m_strFreqRight!=strTmp)
					{
						dlg.m_grid.m_iItemIsNull[11] = 1 ;
					}
				}
				if (dlg.m_grid.m_iItemIsNull[12]==0&&m_mysys.GetXdata(pEnt, _T("FREQ_LEFT"), strTmp) )
				{
					if (i==0)
					{
						pDATA->m_strFreqLeft = strTmp ;
					}
					else if (pDATA->m_strFreqLeft!=strTmp)
					{
						dlg.m_grid.m_iItemIsNull[12] = 1 ;
					}
				}

				pEnt->close() ;
			}
		}
		dlg.m_grid.m_pEntDATA = pDATA ;
		if (dlg.DoModal()==IDOK)
		{
			CString strCellText = _T(""), strTmp = _T("") ;
			int iNumber = 0 ;
			for (int i=0; i<nArrayHandle.GetCount(); i++)
			{	
				long nHandleCur = nArrayHandle.GetAt(i) ;
				AcDbEntity* pEnt = NULL ;
				if (m_mysys.OpenAcDbEntity(nHandleCur, pEnt))
				{
					strCellText = dlg.m_grid.QuickGetText(0, 0) ;
					if (strCellText!=_T("*多种*"))
					{
						m_mysys.SetXdata(pEnt, _T("GUIDAONAME"), strCellText) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 1) ;		
					if (strCellText!=_T("*多种*"))
					{
						m_mysys.SetXdata(pEnt, _T("CIRCUITTYPE"), strCellText) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 2) ;	
					if (strCellText!=_T("*多种*"))
					{
						m_mysys.SetXdata(pEnt, _T("DEVOFHZ25"), strCellText) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 3) ;	
					if (strCellText!=_T("*多种*"))
					{
						iNumber = 0 ;
						iNumber = dlg.m_grid.QuickGetBool(0, 3) ;
						m_mysys.SetXdata(pEnt, _T("ISDH"), iNumber) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 4) ;	
					if (strCellText!=_T("*多种*"))
					{
						iNumber = 0 ;
						iNumber = dlg.m_grid.QuickGetBool(0, 4) ;
						m_mysys.SetXdata(pEnt, _T("ISDMH"), iNumber) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 5) ;	
					if (strCellText!=_T("*多种*"))
					{
						iNumber = 0 ;
						iNumber = dlg.m_grid.QuickGetBool(0, 5) ;
						m_mysys.SetXdata(pEnt, _T("IS4LINE"), iNumber) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 6) ;	
					if (strCellText!=_T("*多种*"))
					{
						iNumber = 0 ;
						iNumber = dlg.m_grid.QuickGetBool(0, 6) ;
						m_mysys.SetXdata(pEnt, _T("ISSHAREXB"), iNumber) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 7) ;	
					if (strCellText!=_T("*多种*"))
					{
						iNumber = 0 ;
						iNumber = dlg.m_grid.QuickGetBool(0, 7) ;
						m_mysys.SetXdata(pEnt, _T("IS1FNJ"), iNumber) ;
					}	
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 8) ;	
					if (strCellText!=_T("*多种*"))
					{
						iNumber = 0 ;
						iNumber = dlg.m_grid.QuickGetBool(0, 8) ;
						m_mysys.SetXdata(pEnt, _T("ISMAINSEC"), iNumber) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 9) ;	
					if (strCellText!=_T("*多种*"))
					{
						strTmp = _T("N") ;
						if (strCellText==_T("送电"))
						{
							strTmp = _T("F") ;
						}
						else if (strCellText==_T("受电"))
						{
							strTmp = _T("S") ;
						}
						m_mysys.SetXdata(pEnt, _T("LEFT_FORS"), strTmp) ;					
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 10) ;	
					if (strCellText!=_T("*多种*"))
					{
						strTmp = _T("N") ;
						if (strCellText==_T("送电"))
						{
							strTmp = _T("F") ;
						}
						else if (strCellText==_T("受电"))
						{
							strTmp = _T("S") ;
						}
						m_mysys.SetXdata(pEnt, _T("RIGHT_FORS"), strTmp) ;					
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 11) ;	
					if (strCellText!=_T("*多种*"))
					{
						m_mysys.SetXdata(pEnt, _T("FREQ_LEFT"), strCellText) ;
					}
					strCellText = _T("") ;
					strCellText = dlg.m_grid.QuickGetText(0, 12) ;	
					if (strCellText!=_T("*多种*"))
					{
						m_mysys.SetXdata(pEnt, _T("FREQ_RIGHT"), strCellText) ;
					}
					//同步更新相关信息
					pEnt->downgradeOpen() ;
					int iShowOrHide = 0 ;
					//if (m_mysys.ReadFromRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_GUIDAONAME"), iShowOrHide) &&iShowOrHide==0)
					CLongArray nArrayShowGdName ;
					if (FindShowGdName(pEnt, nArrayShowGdName)>0)
					{
						this->ShowGdName(pEnt) ; //更新轨道名称（辅助信息）
					}
					if (m_mysys.ReadFromRec(_T("PMT_SHOWINFO"), _T("PMT_SHOWINFO_FSD"), iShowOrHide) &&iShowOrHide==0)
					{
						this->ShowGdFS(pEnt) ; //更新送受电（辅助信息）
					}
					this->UpdateFsBox(pEnt) ; //更新送受电盒子（在双线中）
					this->UpdateDmhBox(pEnt) ; //更新电码化盒子（在双线中）

					pEnt->close() ;
				}
			}
		}
	}
	return 0;
}


int CZnSxt::GetCableParam(double& dCoeff,  vector<int > &vecCableParam)
{
	vecCableParam.resize(6) ;
	CZnUtility::GetCableParam(dCoeff, vecCableParam[0], vecCableParam[1],  vecCableParam[2], vecCableParam[3], vecCableParam[4], vecCableParam[5]) ;
	return 0;
}

// 查找辅助显示的轨道名称
int CZnSxt::FindShowGdName(AcDbEntity* pEntGd, CLongArray& nArrayHandle)
{
	int iRet = 0 ;

	long nHandleGd = m_mysys.GetHandle(pEntGd) ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandleTmp ;
	pRb = acutBuildList(RTDXF0,_T("TEXT"), 1001, _T("PMT_SHOWINFO_GUIDAONAME"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandleTmp) ;
	acutRelRb(pRb) ;
	for (int i=0; i<nLen; i++)
	{
		long nHandleCur = nArrayHandleTmp.GetAt(i) ;
		long nHandleBelong = 0 ;
		if (m_mysys.GetXdata(nHandleCur, _T("BELONG_ENT"), nHandleBelong))
		{
			if (nHandleBelong == nHandleGd )
			{
				nArrayHandle.Add(nHandleCur) ;
				iRet++ ;
			}
		}
	}

	return iRet ;
}

// 以绝缘节或信号机基点为中心构造一个小圆来求和其相交的多段线（左右轨道）
//返回值：0，错误；2，表示信号机/绝缘节在直线上；15，表示左右侧连线都有；5，只有右侧线；10，只有左侧线
int CZnSxt::GetIntersectPLine2(AcDbEntity *pEntSrc, CLongArray &nArrayHandleIntersect, CLongArray &nArrayIndexOfVertex, AcGePoint3dArray &pt3dArrayIntersect)
{
	int iRet = 0 ;

	if (pEntSrc->isKindOf(AcDbCircle::desc()))
	{
		AcDbCircle* pCircle = AcDbCircle::cast(pEntSrc) ;
		AcGePoint3d pt_3d_cen = pCircle->center() ; //圆心，同时也是绝缘节或者信号机基点

		AcDbBlockTable * pBlkTbl = NULL ;
		m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

		AcDbBlockTableRecord * pBlkTblRec = NULL ;
		pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
		pBlkTbl->close() ;
		AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
		pBlkTblRec->newIterator( pBlkTblRecIter );
		pBlkTblRec->close();

		int iNumTwoIntersectPt = 0, iNumOneLeftIntersectPt = 0, iNumOneRightIntersectPt = 0 ;
		AcDbEntity * pEnt = NULL ;
		for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
		{
			if (iRet==0)
			{
				break;
			}
			//遍历并打开实体 ;
			if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
			{
				//只查询相交的多段线
				if (!pEnt->isEqualTo(pEntSrc)&&pEnt->isKindOf(AcDbPolyline::desc()))
				{
					AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;

					AcDb::Visibility bIsVisible  = pPolyLine->visibility() ;
					double dLenOfPline = m_mysys.GetLineLen(pPolyLine) ;
					if (bIsVisible==AcDb::kInvisible||m_mysys.IsOnOffLayer(pEnt)||dLenOfPline<1)
					{
						pEnt->close() ;
						continue;
					}

					AcGePoint3dArray arrayPt3d ;
					Acad::ErrorStatus es = pEntSrc->intersectWith(pEnt, AcDb::kOnBothOperands, arrayPt3d);
					int iPtNum = arrayPt3d.length() ;

					if(es==Acad::eOk && iPtNum>0&&iPtNum<3)
					{
						UINT nIndexOfVertex = 0 ;
						UINT nHandleIntersectEnt = 0 ;
						nHandleIntersectEnt = m_mysys.GetHandle(pEnt) ;
						AcGePoint3d pt_3d_intersect ;
// 						pt_3d_intersect = arrayPt3d.at(0) ;

						if (iPtNum==2) //和圆有2个交点的,
						{
							if ((iNumTwoIntersectPt+iNumOneLeftIntersectPt+iNumOneRightIntersectPt)==0)
							{
								iNumTwoIntersectPt++ ;
								nArrayHandleIntersect.Add(nHandleIntersectEnt) ;
								AcGeLineSeg3d linseg(arrayPt3d.at(0), arrayPt3d.at(1)) ;
								pt_3d_intersect = linseg.closestPointTo(pt_3d_cen) ; //此处交点以绝缘节或信号机基点到直线垂足为宜
								int iPtOnPLine = m_mysys.IsPointOnPolyLine(pPolyLine, pt_3d_intersect, nIndexOfVertex) ;
								nArrayIndexOfVertex.Add(iPtOnPLine) ;
								pt3dArrayIntersect.append(pt_3d_intersect) ;
								iRet = 2 ;
							}
							else
							{
								iRet = 0 ;
							}
						}
						else if (iPtNum==1) //和圆有1个交点的
						{
							pt_3d_intersect = arrayPt3d.at(0) ;
							if (pt_3d_intersect.x<pt_3d_cen.x) //交点在圆心左侧
							{
								if (iNumTwoIntersectPt==0&&iNumOneLeftIntersectPt==0)
								{
									iNumOneLeftIntersectPt++ ;
									nArrayHandleIntersect.Add(nHandleIntersectEnt) ;
									nArrayIndexOfVertex.Add(pPolyLine->numVerts()-1) ;
									AcGePoint3d pt_3d_end ;
									pPolyLine->getEndPoint(pt_3d_end) ;
									pt3dArrayIntersect.append(pt_3d_end) ;
									if (iRet==-1)
									{
										iRet = 1 ;
									}
									else if (iRet==5)
									{
										iRet = 1*10+5 ;
									}
									else
									{
										iRet = 0 ;
									}
								}
								else
								{
									iRet = 0 ;
								}
							}
							else
							{
								if (iNumTwoIntersectPt==0&&iNumOneRightIntersectPt==0)
								{
									iNumOneRightIntersectPt++ ;
									nArrayHandleIntersect.Add(nHandleIntersectEnt) ;
									nArrayIndexOfVertex.Add(0) ;
									AcGePoint3d pt_3d_start ;
									pPolyLine->getStartPoint(pt_3d_start) ;
									pt3dArrayIntersect.append(pt_3d_start) ;
									if (iRet==-1)
									{
										iRet = 5 ;
									}
									else if (iRet==1)
									{
										iRet = 5*10+1 ;
									}
									else
									{
										iRet = 0 ;
									}
								}
								else
								{
									iRet = 0 ;
								}
							}
						}
					}
				}

				pEnt->close() ;
			}		
		}
		delete pBlkTblRecIter ;
		pBlkTblRecIter = NULL ;
	}

	return iRet ;
}


// 取得属于指定道岔的多段线（在双线中）
int CZnSxt::GetSxBelongDc(long nHandleDc, long &nHandlePline)
{
	int iRet = 0 ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandleTmp ;
	pRb = acutBuildList(RTDXF0, _T("LWPOLYLINE"), 1001, _T("ForZZJ"),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandleTmp) ;
	acutRelRb(pRb) ;

	for (int i=0; i<nLen; i++)
	{
		long nHandleCur = nArrayHandleTmp.GetAt(i) ;
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandleCur, pEnt, AcDb::kForRead))
		{
			CString strForZZJ ;
			m_mysys.GetXdata(pEnt, _T("ForZZJ"), strForZZJ) ;
			CStringArray strArrayForZZJ ;
			CBlkUtility::DivideString(strForZZJ, _T("-"), strArrayForZZJ) ;
			if (strArrayForZZJ.GetCount()==5)
			{
				CString strTmp = strArrayForZZJ.GetAt(0) ;
				long nHandleDcTmp = 0 ;
				nHandleDcTmp = _ttol(strTmp) ;
				if (nHandleDcTmp==nHandleDc)
				{
					nHandlePline = nHandleCur ;
					iRet++ ;
					pEnt->close() ;
					break;
				}
			}
			pEnt->close() ;
		}		
	}
	return iRet ;
}

// 当前盒子对应的ZZJ类型，直接对应define_dev
int CZnSxt::GetZzjDevType(long nHandleCurBox, CString& strZzjDevType)
{
	CString strMsg ;

	long nHandleZzj = 0, nHandleDc = 0 ;
	if(m_mysys.GetXdata(nHandleCurBox, _T("BELONG_ENT"), nHandleZzj)) 
	{
		CString strNameTmp, strDragType, strIndexDrag  ; 
		m_mysys.GetAttValue(nHandleZzj, _T("NAME"), strNameTmp) ; //例： J1 J2 J3 X1 X2 X3
		m_mysys.GetAttValue(nHandleZzj, _T("TYPE"), strDragType) ; //例：ZD6-E
		//m_mysys.GetAttValue(nHandleZzj, _T("ACTINDEX"), strIndexDrag) ; //例：1/2

		if(m_mysys.GetXdata(nHandleZzj, _T("BELONG_ENT"), nHandleDc)) 
		{
			long iIsJxg = 1 ; //默认尖轨，=2是芯轨
			m_mysys.GetXdata(nHandleZzj, _T("IS_JXG"), iIsJxg) ;
			CDcDATA dataDc ;
			this->GetEntData(nHandleDc, dataDc) ;
			CString strQyTypeCur = (iIsJxg==1?dataDc.m_strJG_QyType:dataDc.m_strXG_QyType) ;
			CString strAppNameQyType = (iIsJxg==1?_T("JGQYTYPE"):_T("XGQYTYPE")) ;
			//if (iIsJxg==1) //尖轨
			{
				if (strQyTypeCur==_T("ZD6-D"))
				{
					if (dataDc.m_strRelationDc.IsEmpty()) //单动
					{
						strZzjDevType = _T("ZD6-D--1A") ;
					}
					else //多动
					{
						CStringArray strArrayRelationDc ;
						CBlkUtility::DivideString(dataDc.m_strRelationDc, _T(","), strArrayRelationDc) ;
						int iNumRelDc = strArrayRelationDc.GetCount() ;
						if (iNumRelDc==1) //双动
						{
							//另一动
							CLongArray nArrayHandleRecDc ;
							if(this->FindDc(dataDc.m_strRelationDc, nArrayHandleRecDc )==1)
							{
								//另一动牵引方式
								CString strQyTypeRelDc = _T("ZD6-D") ;
								m_mysys.GetXdata(nArrayHandleRecDc.GetAt(0),  strAppNameQyType, strQyTypeRelDc) ;
								if (strQyTypeRelDc==_T("ZD6-D"))
								{
									strZzjDevType.Format(_T("ZD6-D_ZD6-D--%dA"), dataDc.m_nActIndex) ; 
								}
								else if (strQyTypeRelDc==_T("ZD6-EJ"))
								{
									if (dataDc.m_nActIndex==1)
									{
										strZzjDevType.Format(_T("ZD6-D_ZD6-E+ZD6-J--1A")) ;
									}
									else if (dataDc.m_nActIndex==2)
									{
										strZzjDevType.Format(_T("ZD6-E+ZD6-J_ZD6-D--2A")) ;
									}
								}
							}
							else
							{
								strMsg.Format(_T("道岔%s设置了双动道岔%s，但是当前平面图未搜到%s"), dataDc.m_strName, dataDc.m_strRelationDc, dataDc.m_strRelationDc) ;
								AfxMessageBox(strMsg) ;
							}
						}
						else if (iNumRelDc==2) //3动，暂只处理ZD6-D_ZD6-D_ZD6-D和ZD6-D_ZD6-D_ZD6-EJ
						{
							CLongArray nArrayHandleRecDc1, nArrayHandleRecDc2 ;
							int iFind1 = this->FindDc(strArrayRelationDc.GetAt(0), nArrayHandleRecDc1 ) ;
							int iFind2 = this->FindDc(strArrayRelationDc.GetAt(1), nArrayHandleRecDc2 ) ;
							if (iFind1==1&&iFind2==1)
							{
								//另2动牵引方式
								CString strQyTypeRelDc1, strQyTypeRelDc2 ;
								m_mysys.GetXdata(nArrayHandleRecDc1.GetAt(0),  strAppNameQyType, strQyTypeRelDc1) ;
								m_mysys.GetXdata(nArrayHandleRecDc2.GetAt(0),  strAppNameQyType, strQyTypeRelDc2) ;
								if (strQyTypeRelDc1==_T("ZD6-D")&&strQyTypeRelDc2==_T("ZD6-D"))
								{
									strZzjDevType.Format(_T("ZD6-D_ZD6-D_ZD6-D--%dA"), dataDc.m_nActIndex) ;
								}
								else if (dataDc.m_nActIndex<3&&strQyTypeRelDc1==_T("ZD6-D")&&strQyTypeRelDc2==_T("ZD6-EJ") || dataDc.m_nActIndex<3&&strQyTypeRelDc1==_T("ZD6-EJ")&&strQyTypeRelDc2==_T("ZD6-D"))
								{
									strZzjDevType.Format(_T("ZD6-D_ZD6-D_ZD6-EJ--%dA"), dataDc.m_nActIndex) ;
								}
							}
							else
							{
								strMsg.Format(_T("道岔%s设置了多动道岔%s，但是当前平面图未搜全%s"), dataDc.m_strName, dataDc.m_strRelationDc, dataDc.m_strRelationDc) ;
								AfxMessageBox(strMsg) ;
							}
						}
						else if (iNumRelDc==3) //4动，暂只处理4个ZD6-D
						{
							CLongArray nArrayHandleRecDc1, nArrayHandleRecDc2, nArrayHandleRecDc3 ;
							int iFind1 = this->FindDc(strArrayRelationDc.GetAt(0), nArrayHandleRecDc1 ) ;
							int iFind2 = this->FindDc(strArrayRelationDc.GetAt(1), nArrayHandleRecDc2 ) ;
							int iFind3 = this->FindDc(strArrayRelationDc.GetAt(2), nArrayHandleRecDc3 ) ;
							if (iFind1==1&&iFind2==1&&iFind3==1)
							{
								//另3动牵引方式
								CString strQyTypeRelDc1, strQyTypeRelDc2, strQyTypeRelDc3 ;
								m_mysys.GetXdata(nArrayHandleRecDc1.GetAt(0), strAppNameQyType, strQyTypeRelDc1) ;
								m_mysys.GetXdata(nArrayHandleRecDc2.GetAt(0), strAppNameQyType, strQyTypeRelDc2) ;
								m_mysys.GetXdata(nArrayHandleRecDc3.GetAt(0), strAppNameQyType, strQyTypeRelDc3) ;
								if (strQyTypeRelDc1==_T("ZD6-D")&&strQyTypeRelDc2==_T("ZD6-D")&&strQyTypeRelDc3==_T("ZD6-D"))
								{
									strZzjDevType.Format(_T("ZD6-D_ZD6-D_ZD6-D_ZD6-D--%dA"), dataDc.m_nActIndex) ;
								}
							}
							else
							{
								strMsg.Format(_T("道岔%s设置了多动道岔%s，但是当前平面图未搜全%s"), dataDc.m_strName, dataDc.m_strRelationDc, dataDc.m_strRelationDc) ;
								AfxMessageBox(strMsg) ;
							}
						}
					}
				}
				else if (strQyTypeCur==_T("ZD6-EJ"))
				{
					if (dataDc.m_strRelationDc.IsEmpty()) //单动
					{
						strZzjDevType.Format(_T("ZD6-E+ZD6-J--1%s"), strDragType==_T("ZD6-E")?_T("A"):_T("B") ) ;
					}
					else //多动
					{
						CStringArray strArrayRelationDc ;
						CBlkUtility::DivideString(dataDc.m_strRelationDc, _T(","), strArrayRelationDc) ;
						int iNumRelDc = strArrayRelationDc.GetCount() ;
						if (iNumRelDc==1) //双动
						{
							//另一动
							CLongArray nArrayHandleRecDc ;
							if(this->FindDc(strArrayRelationDc.GetAt(0), nArrayHandleRecDc )==1)
							{
								//另一动牵引方式
								CString strQyTypeRelDc = _T("ZD6-D") ;
								m_mysys.GetXdata(nArrayHandleRecDc.GetAt(0), strAppNameQyType, strQyTypeRelDc) ;
								if (strQyTypeRelDc==_T("ZD6-D"))
								{
									if (dataDc.m_nActIndex==1)
									{
										strZzjDevType.Format(_T("ZD6-E+ZD6-J_ZD6-D--1%s"), strDragType==_T("ZD6-E")?_T("A"):_T("B")) ;
									}
									else if (dataDc.m_nActIndex==2)
									{
										strZzjDevType.Format(_T("ZD6-D_ZD6-E+ZD6-J--2%s"), strDragType==_T("ZD6-E")?_T("A"):_T("B")) ;
									}
								}
								else if (strQyTypeRelDc==_T("ZD6-EJ"))
								{
									strZzjDevType.Format(_T("ZD6-E+ZD6-J_ZD6-E+ZD6-J--%d%s"), dataDc.m_nActIndex, strDragType==_T("ZD6-E")?_T("A"):_T("B")) ; 
								}
							}
							else
							{
								strMsg.Format(_T("道岔%s设置了双动道岔%s，但是当前平面图未搜到%s"), dataDc.m_strName, strArrayRelationDc.GetAt(0), strArrayRelationDc.GetAt(0)) ;
								AfxMessageBox(strMsg) ;
							}
						}
						else if (iNumRelDc==2) //3动，暂只处理ZD6-D_ZD6-D_ZD6-EJ
						{
							CLongArray nArrayHandleRecDc1, nArrayHandleRecDc2 ;
							int iFind1 = this->FindDc(strArrayRelationDc.GetAt(0), nArrayHandleRecDc1 ) ;
							int iFind2 = this->FindDc(strArrayRelationDc.GetAt(1), nArrayHandleRecDc2 ) ;
							if (iFind1==1&&iFind2==1)
							{
								//另2动牵引方式
								CString strQyTypeRelDc1, strQyTypeRelDc2 ;
								m_mysys.GetXdata(nArrayHandleRecDc1.GetAt(0), strAppNameQyType, strQyTypeRelDc1) ;
								m_mysys.GetXdata(nArrayHandleRecDc2.GetAt(0), strAppNameQyType, strQyTypeRelDc2) ;
								if (dataDc.m_nActIndex==3&&strQyTypeRelDc1==_T("ZD6-D")&&strQyTypeRelDc2==_T("ZD6-D"))
								{
									strZzjDevType.Format(_T("ZD6-D_ZD6-D_ZD6-EJ--%d%s"), dataDc.m_nActIndex, strDragType==_T("ZD6-E")?_T("A"):_T("B")) ;
								}
							}
							else
							{
								strMsg.Format(_T("道岔%s设置了多动道岔%s，但是当前平面图未搜全%s"), dataDc.m_strName, dataDc.m_strRelationDc, dataDc.m_strRelationDc) ;
								AfxMessageBox(strMsg) ;
							}
						}
					}
				}
				else if (strQyTypeCur==_T("ZD6-EJJ"))
				{
					strZzjDevType.Format(_T("%s--1A"), strDragType) ;
				}
				else if (strQyTypeCur==_T("ZYJ7"))
				{
					strZzjDevType.Format(_T("ZYJ7--1A") ) ;
				}
				else if (strQyTypeCur==_T("ZYJ7+SH6"))
				{
					strZzjDevType.Format(_T("ZYJ7+SH6--1%s"), strDragType==_T("ZYJ7")?_T("A"):_T("B")) ;
				}
				else if (strQyTypeCur==_T("ZYJ7+SH6+SH6"))
				{
					if (strDragType==_T("ZYJ7"))
					{
						strZzjDevType.Format(_T("ZYJ7+SH6+SH6--1A")) ;
					}
					else
					{
						if (strNameTmp==_T("J2")||strNameTmp==_T("X2"))
						{
							strZzjDevType.Format(_T("ZYJ7+SH6+SH6--1B")) ;
						}
						else if (strNameTmp==_T("J3")||strNameTmp==_T("X3"))
						{
							strZzjDevType.Format(_T("ZYJ7+SH6+SH6--1C")) ;
						}				
					}
				}
				
			}
// 			else if (iIsJxg==2) //芯轨
// 			{
// 			}


		}

	}


	return 0;
}

// 更新双线图中轨道名称(仅更新已有，若没有则不绘制)
int CZnSxt::UpdateSxtGdName(AcDbEntity* pEnt, const CString strGdName)
{
	int iRet = 0 ;

	long nHandle = m_mysys.GetHandle(pEnt) ;
	CLongArray nArrayHandleBelong ;
	this->FindEntBelong(nHandle, nArrayHandleBelong) ;
    long nHandleGdName = 0 ;
	for (int i=0; i<nArrayHandleBelong.GetCount(); i++)
	{
		long iXdata = 0 ;
		if (m_mysys.GetXdata(nArrayHandleBelong.GetAt(i), _T("TYPE"), iXdata)&&iXdata==101)
		{
			nHandleGdName = nArrayHandleBelong.GetAt(i) ;
			iRet++ ;
		}
	}
	if (iRet==1)
	{
		AcDbEntity* pEntGd = NULL ;
		if (m_mysys.OpenAcDbEntity(nHandleGdName, pEntGd))
		{
			if (pEntGd->isKindOf(AcDbText::desc()))
			{
				AcDbText* pText = AcDbText::cast(pEntGd) ;
				pText->setTextString(strGdName) ;
			}
			pEntGd->close() ;
		}
	}
	else if (iRet>1)
	{
		AfxMessageBox(_T("注意：在更新该轨道在双线图中的名称时，发现不止一个名称文本，可能双线中轨道名称文本有复制！")) ;
	}
	return iRet ;
}

// 检查重合绝缘节、信号机、漏绝缘节
int CZnSxt::CheckJyjXhj(void)
{
	BOOL bHasErro = FALSE ;

	CString strMsg ;

	double dTolToGd = 0.5, dTolOverlap = 0.5 ;
	CBlkUtility::QueryValueReg(_T("ZnOption"), _T("TolJyjXhjToGd"), dTolToGd) ;
	CBlkUtility::QueryValueReg(_T("ZnOption"), _T("TolJyjXhjOverlap"), dTolOverlap) ;

	AcGeTol tolOverlap ;
	tolOverlap.setEqualPoint(dTolOverlap) ;

	acutPrintf(_T("\n本操作将：\n\t1、将离轨道线距离在误差范围 %.3f 的绝缘节信号机移至轨道线；\n\t2、将距离信号机在误差范围 %.3f 的绝缘节移至和信号机同点；\n\t3、检查漏绝缘节、重合绝缘节、信号机(误差%.3f)；\n\t4、欲改变上述误差允许值，请运行菜单【车站室外】→【参数设置】."), dTolToGd, dTolOverlap, dTolOverlap) ;
	acutPrintf(_T("\n请框选范围:")) ;

	struct resbuf* pRb ;
	int nLen = 0 ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0,_T("INSERT"),0) ;
	nLen = m_mysys.GetSSEntHandle(NULL, NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;

	int iRet = 0 ;
	AcGePoint3dArray pt3dArrayJyj, pt3dArrayXhj ;
	CLongArray nArrayHandleJyj, nArrayHandleXhj ;
	CStringArray strArrayXhjName ;
	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEnt = NULL ;
		long nHandle = nArrayHandle.GetAt(i) ;
		if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;
				if (strBlkName.GetLength()>3)
				{
					AcGePoint3d pt_3d_blk = pRef->position() ;
					if (strBlkName.Left(3)==_T("JYJ"))
					{
						this->MoveXhjJyj(pRef, dTolToGd) ;
						pt_3d_blk = pRef->position() ;
						pt3dArrayJyj.append(pt_3d_blk) ;
						nArrayHandleJyj.Add(nHandle) ;
					}
					else if (strBlkName.Left(3)==_T("XHJ"))
					{
						this->MoveXhjJyj(pRef, dTolToGd) ;
						pt_3d_blk = pRef->position() ;
						pt3dArrayXhj.append(pt_3d_blk) ;
						CString strXhjName ;
						m_mysys.GetAttValue(pRef, _T("XHJNAME"), strXhjName) ;
						strArrayXhjName.Add(strXhjName) ;
						nArrayHandleXhj.Add(nHandle) ;
					}
				}
			}

			pEnt->close() ;
		}
	}

	//将相近的绝缘节move到信号机基点
	for (int i=0; i<nArrayHandleJyj.GetCount(); i++)
	{
		AcGePoint3d pt_3d_jyj = pt3dArrayJyj.at(i) ;
		for (int j=0; j<nArrayHandleXhj.GetCount(); j++)
		{
			AcGePoint3d pt_3d_xhj = pt3dArrayXhj.at(j) ;
			if ((pt_3d_jyj.x!=pt_3d_xhj.x||pt_3d_jyj.y!=pt_3d_xhj.y)&&pt_3d_jyj.isEqualTo(pt_3d_xhj, tolOverlap))
			{
				AcDbEntity* pEnt = NULL ;
				if (m_mysys.OpenAcDbEntity(nArrayHandleJyj.GetAt(i), pEnt))
				{
					AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
					pRef->setPosition(pt_3d_xhj) ;
					double dDist = pt_3d_xhj.distanceTo(pt_3d_jyj) ;
					acutPrintf(_T("\n%s绝缘节移动到误差范围 %.3f 内视为同点的信号机%s处，从(%.3f,%.3f) 移动到 (%.3f,%.3f)，距离 %.3f"), dDist>0.1?_T("*****"):_T(""), dTolOverlap, strArrayXhjName.GetAt(j), pt_3d_jyj.x, pt_3d_jyj.y, pt_3d_xhj.x, pt_3d_xhj.y, dDist) ;

					pEnt->close() ;
				}
				pt3dArrayJyj.setAt(i, pt_3d_xhj) ;
				break;
			}
		}
	}
	
	//检查信号机、绝缘节重合
	for (int n=0; n<2; n++)
	{
		AcGePoint3dArray& pt3dArray = (n==0?pt3dArrayJyj:pt3dArrayXhj) ;
		int iNum = pt3dArray.length() ;
		vector<int> vec ;
		for (int i=0; i<iNum; i++)
		{
			vec.push_back(0) ;
		}
		for (int i=0; i<iNum; i++)
		{
			if (vec[i]==0)
			{
				AcGePoint3d pt_3d_cur1 = pt3dArray.at(i) ;
				int iNumFind = 0 ; //找到重合次数
				for (int j=(i+1); j<iNum; j++)
				{
					if (vec[j]==0)
					{
						AcGePoint3d pt_3d_cur2 = pt3dArray.at(j) ;
						if (pt_3d_cur1.isEqualTo(pt_3d_cur2,tolOverlap ))
						{
							iNumFind++ ;
							vec[j] = 1 ;
						}
					}
				}
				if (iNumFind>0)
				{
					bHasErro = TRUE ;
					acutPrintf(_T("\n*****在点 (%.3f,%.3f) 处找到%d次重合%s,请删除多余的!"), pt_3d_cur1.x, pt_3d_cur1.y, iNumFind, n==0?_T("绝缘节"):_T("信号机")) ;
				}
			}
		}
	}

	if (bHasErro)
	{
		strMsg.Format(_T("\n!!!发现重合绝缘节或信号机，请按提示处理！")) ;
		acutPrintf(strMsg) ;
		AfxMessageBox(strMsg) ;
// 		return 0 ;
	}
	else
	{
		acutPrintf(_T("\n***********未发现重合绝缘节或信号机！")) ;
	}

    bHasErro = FALSE ;
	//检查漏绝缘节
	int iNumXhj = pt3dArrayXhj.length() ;
	int iNumJyj = pt3dArrayJyj.length() ;
	for (int i=0; i<iNumXhj; i++)
	{
		AcGePoint3d pt_3d_curxhj = pt3dArrayXhj.at(i) ;
		int iNumFind = 0 ; //找到绝缘节次数
		for (int j=0; j<iNumJyj; j++)
		{
			AcGePoint3d pt_3d_curjyj = pt3dArrayJyj.at(j) ;
			if (pt_3d_curxhj.isEqualTo(pt_3d_curjyj,tolOverlap ))
			{
				iNumFind++ ;
			}
		}
		if (iNumFind==0)
		{
			bHasErro = TRUE ;
			acutPrintf(_T("\n*****信号机%s在点 (%.3f,%.3f) 处未找到绝缘节!"), strArrayXhjName.GetAt(i), pt_3d_curxhj.x, pt_3d_curxhj.y ) ;
		}
	}
	if (bHasErro)
	{
		strMsg.Format(_T("\n!!!发现漏绝缘节，按提示处理！")) ;
		acutPrintf(strMsg) ;
		AfxMessageBox(strMsg) ;
		//return 0 ;
	}
	else
	{
		acutPrintf(_T("\n***********未发现漏绝缘节！")) ;
	}

	return 0;
}

// 将信号机或绝缘节移至线上
void CZnSxt::MoveXhjJyj(AcDbBlockReference *pRef,	 double dTol)
{
	AcGePoint3d pt_3d_ref = pRef->position() ;
	CString strBlkName ;
	m_mysys.GetBlockName(pRef, strBlkName) ;
	CString strXhjName ;
	if (strBlkName.Left(3)==_T("XHJ"))
	{
		m_mysys.GetAttValue(pRef, _T("XHJNAME"), strXhjName) ;
	}

	pRef->upgradeOpen() ;

	AcDbBlockTable * pBlkTbl = NULL ;
	m_pDb->getBlockTable(pBlkTbl,AcDb::kForRead);

	AcDbBlockTableRecord * pBlkTblRec = NULL ;
	pBlkTbl->getAt ( ACDB_MODEL_SPACE, pBlkTblRec,AcDb::kForRead )  ;
	pBlkTbl->close() ;
	AcDbBlockTableRecordIterator *pBlkTblRecIter = NULL ;
	pBlkTblRec->newIterator( pBlkTblRecIter );
	pBlkTblRec->close();

	AcDbEntity * pEnt = NULL ;
	for(pBlkTblRecIter->start();!pBlkTblRecIter->done();pBlkTblRecIter->step())
	{
		if (Acad::eOk == pBlkTblRecIter->getEntity(pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbPolyline::desc()))
			{
				AcDbPolyline *pPolyLine = AcDbPolyline::cast(pEnt) ;

				//只查询显示的多段线
				AcDb::Visibility bIsVisible  = pPolyLine->visibility() ;
				double dLenOfPline = m_mysys.GetLineLen(pPolyLine) ;
				if (bIsVisible==AcDb::kInvisible||m_mysys.IsOnOffLayer(pEnt)||dLenOfPline<1)
				{
					pEnt->close() ;
					continue;
				}

				long nHandleCur = m_mysys.GetHandle(pEnt) ;
				AcGePoint3d pt_3d_intersect ; //交点
				UINT nIndexOfVertex = 0 ;
				int iPtOnPLine = m_mysys.IsPointOnPolyLine(pPolyLine, pt_3d_ref, nIndexOfVertex, dTol) ;
				if (iPtOnPLine!=-1)
				{
                    pPolyLine->getClosestPointTo(pt_3d_ref, pt_3d_intersect) ;
					double dDist = pt_3d_ref.distanceTo(pt_3d_intersect) ;
					if (dDist!=0)
					{
						pRef->setPosition(pt_3d_intersect) ;
						CString strTypeName ;
						strTypeName.Format(_T("%s:%s"), strBlkName, strXhjName.IsEmpty()?_T(""):strXhjName) ;
						acutPrintf(_T("\n%s%s: 从(%.3f,%.3f)移动到(%.3f, %.3f)，距离 %.3f"), dDist>0.1?_T("*****"):_T(""), strTypeName, pt_3d_ref.x, pt_3d_ref.y, pt_3d_intersect.x, pt_3d_intersect.y, dDist) ;
					}
					pEnt->close() ;
					break;
				}
			}

			pEnt->close() ;
		}		
	}
	delete pBlkTblRecIter ;
	pBlkTblRecIter = NULL ;	

	pRef->downgradeOpen() ;
}

// 建立一个电缆径路数（节点是箱盒）
void CZnSxt::BuildCableBoxTree(long nHandleBox, long nHandleCable, sequential_tree<CBoxNodeEnt>& cableTree)
{
	CBoxNodeEnt boxNode ;
	if (nHandleBox==0) //入楼端（连接到主电缆的径路）
	{
		this->GetCableName(nHandleCable, boxNode.m_strDevName) ;
		boxNode.m_strBoxType = _T("XHL") ;
		boxNode.m_iEntType = -2 ; //信号楼
		cableTree.set(boxNode) ;

		this->FindBox(nHandleCable, nHandleCable, &cableTree) ;
	}
	else
	{
		if(GetBoxInfo(nHandleBox, boxNode) )
		{
			boxNode.m_nHandleCable = nHandleCable ;
			long nHandleCableString = 0 ;
			if (this->GetCableString(nHandleCable, nHandleCableString) ==1)
			{
				m_mysys.GetTextString(nHandleCableString, boxNode.m_strCableString) ;
			}
			cableTree.set(boxNode) ;

			CLongArray nArrayCable ;
			int iNumCable = this->FindCable(nHandleBox, nHandleCable, nArrayCable) ;
			for (int i=0; i<iNumCable; i++)
			{
				this->FindBox(nArrayCable.GetAt(i), nHandleBox, &cableTree) ;
			}
		}
	}

}


// 重新计算转辙机电缆
int CZnSxt::ReComputeZzjCable(long nHandleCableIntoXhl)
{
	vector<sequential_tree<CBoxNodeEnt>* > vec ; // 起头的节点（没有children）
	sequential_tree<CBoxNodeEnt> cableTree ;
	this->BuildCableBoxTree(0, nHandleCableIntoXhl, cableTree) ;
	sequential_tree<CBoxNodeEnt>::post_order_iterator pit = cableTree.post_order_begin();	//先遍历子节点	
	sequential_tree<CBoxNodeEnt>::post_order_iterator pit_end = cableTree.post_order_end();	
	for (; pit != pit_end; ++pit)
	{
		//acutPrintf(_T("\n:%s, %s, %s,capaticy:%d"), pit->m_strBoxType, pit->m_strDevName, pit->m_strDevType, pit.node()->capacity() ) ;
		if (pit.node()->size()==0) 
		{
			vec.push_back(pit.node()) ;			
		}
	}

	vector<sequential_tree<CBoxNodeEnt>*>::iterator it = vec.begin() ;
	for (;it!=vec.end();++it)
	{
		//sequential_tree<CBoxNodeEnt> * treeTmp = *it ;
		//acutPrintf(_T("\n***:%s, %s, %s, disttoxhl:%i"), (*it)->get()->m_strBoxType, (*it)->get()->m_strDevName, (*it)->get()->m_strDevType, this->DistanceToXhl(*it) ) ;
		CString strPreDcName ;
		vector<sequential_tree<CBoxNodeEnt>* > vec2 ;
		LoopZzj(*it,strPreDcName, vec2) ;
	}

	int iOkCancel = AfxMessageBox(_T("点确定自动计算并更新该径路各电缆，否则仅计算数据但不更新电缆字符串，随后可人工逐步编辑各段电缆！"),MB_OKCANCEL) ;

	for (pit = cableTree.post_order_begin(); pit != pit_end; ++pit)
	{
		//acutPrintf(_T("\n:%s, %s, %s,加芯长度：%d, 加芯Id:%d"), pit->m_strDevName, pit->m_strBoxType, pit->m_strDevType, pit->m_iLenForPlus, pit->m_nIdPlus ) ;
		if (pit->m_nHandleCable!=0)
		{
			m_mysys.SetXdata(pit->m_nHandleCable, _T("LENFORPLUS"), pit->m_iLenForPlus) ;
		}
		if (iOkCancel==IDOK)
		{
			CString strCableTypeName ;
			int iBackup = 0 ;
			int iNumCore = this->GetCableCoreNum(*(pit.node()), strCableTypeName, iBackup, pit->m_iLenForPlus) ; 

			//原电缆字符
			long nHandleCableString = 0 ; //电缆（箱盒之间的多段线）对应的文本
			this->GetCableString(pit->m_nHandleCable, nHandleCableString, 0) ;
			CString strCableString ;
			if (nHandleCableString!=0)
			{
				m_mysys.GetTextString(nHandleCableString, strCableString) ;
				CString strCableStringNew ;
				strCableStringNew.Format(_T("%s-%s%s(%d)"), strCableString.Left(strCableString.Find(_T("-"))), strCableTypeName, strCableString.Find(_T("L"))==-1?_T(""):_T("L"), iBackup) ;
				m_mysys.SetTextString(nHandleCableString, strCableStringNew) ;
			}
		}
	}

	return 0;
}


// 对应CCableNet中的LoopZZJ
void CZnSxt::LoopZzj(sequential_tree<CBoxNodeEnt>* pTree, CString& strPreDcName, vector<sequential_tree<CBoxNodeEnt>* >& vec)
{
	if (pTree->get()->m_iEntType!=0) //遇到非转辙机即停止此次loop
	{
		//处理一次nArrayHandleZzj里面存储的一个组合
		SetZzjDevType(vec) ;
	}
	else
	{
		if (!strPreDcName.IsEmpty()&&strPreDcName!=pTree->get()->m_strDevName)
		{
			////处理一次nArrayHandleZzj里面存储的一个组合
			SetZzjDevType(vec) ;

			vec.clear() ;
			//strArrayZzjType.RemoveAll() ;
			//strArrayZzjIndex.RemoveAll() ;
			strPreDcName = _T("") ;
			LoopZzj(pTree, strPreDcName, vec) ;
		}
		else
		{
			vec.push_back(pTree) ;
			//strArrayZzjType.Add(pTree->get()->m_strDragType) ;
			//strArrayZzjIndex.Add(pTree->get()->m_strActIndex) ;
			long nHandleNext = 0 ;

			if (pTree->get()->m_strActIndex.Left(1)==_T("1")) // 1/1或者1/2......
			{
				strPreDcName = pTree->get()->m_strDevName ;
			}
			LoopZzj(pTree->parent(), strPreDcName, vec) ;
		}
	}


}

// 设置转辙机的组合类型
void CZnSxt::SetZzjDevType(vector<sequential_tree<CBoxNodeEnt>* >& vec)
{
	int iCount = vec.size() ;
	if (iCount==0)
	{
		return ;
	}

	//计算至信号楼的最远距离（此组合第一个（远离信号楼）设备）
	int iDistance = this->DistanceToXhl(vec.at(0)) ;

	CString strZzjCombination ; //转辙机组合，例ZD6-D_ZD6-D
	CStringArray strArrayActAndDrag ; //第几动几牵引，例1动A(1A)
	TCHAR chDrag = _T('A') ;

	for (int i=(iCount-1); i>=0; i--)
	{
		CString strZzjType = vec.at(i)->get()->m_strDragType ;
		CString strZzjIndex = vec.at(i)->get()->m_strActIndex ;
		CString strActAndDrag ;
		if (i==(iCount-1))
		{
			strZzjCombination = strZzjType ;			
		}
		else
		{
			CString strPreZzjIndex = vec.at(i+1)->get()->m_strActIndex ;
			CString strConStr = _T("+") ;
			if (strPreZzjIndex!=strZzjIndex)
			{
				strConStr = _T("_") ;
				chDrag = _T('A') ;
			}
			else
			{
				chDrag++ ;
			}
			strZzjCombination.AppendFormat(_T("%s%s"), strConStr, strZzjType) ;
		}
		strActAndDrag.Format(_T("%s%c"), strZzjIndex.Left(1), chDrag)  ;
		strArrayActAndDrag.Add(strActAndDrag) ;
	}

	for (int i=(iCount-1); i>=0; i--)
	{
		CString strZzjDevType ;
		strZzjDevType.Format(_T("%s--%s"), strZzjCombination, strArrayActAndDrag.GetAt(iCount-1-i)) ;
		CString strSql ;
		//先从加芯表中直接查找dev_for=strZzjDevType，并距信号楼距离符合iDistance的记录
		strSql.Format(_T("select top 1 * from define_zzj_plus where dev_for='%s' and cable_len>%d"), strZzjDevType, iDistance) ;
		long nId = 0 ;
		m_RecordSet.GetRecordValue(strSql, _T("id"), nId) ;
		if (nId==0) //未找到 ，则去字典查找 strZzjDevType匹配的
		{
			strSql.Format(_T("select  * from define_dict where dict_for='ZZJ_PLUS' and source='%s'"), strZzjDevType) ;
			CString strDestination ;
			m_RecordSet.GetRecordValue(strSql, _T("destination"), strDestination) ;
			if (!strDestination.IsEmpty())
			{
				strSql.Format(_T("select top 1 * from define_zzj_plus where dev_for='%s' and cable_len>%d"), strDestination, iDistance) ;
				m_RecordSet.GetRecordValue(strSql, _T("id"), nId) ;
			}
		}
		if (vec.at(i)->get()->m_strDevType!=strZzjDevType)
		{
			acutPrintf(_T("\n重新计算的转折机类型 %s 和 原来计算的类型 %s 不一致!"), strZzjDevType, vec.at(i)->get()->m_strDevType) ;
		}
		vec.at(i)->get()->m_iLenForPlus = iDistance ;
		vec.at(i)->get()->m_nIdPlus = nId ;
		//strSql.Format(_T("update cablenet_ent set devtype='%s', zzj_plus_id=%d where handle_low=%d"), strZzjDevType, nId, nArrayHandle.GetAt(i)) ;
		//m_AdoDb.Execute(strSql) ;

		if (nId==0)
		{
			acutPrintf(_T("\nhandle=%d， devtype=%s 的记录未找到加芯记录"), vec.at(i)->get()->m_nHandleBox, strZzjDevType) ;
		}
	}
}

// 到信号楼的距离
int CZnSxt::DistanceToXhl(sequential_tree<CBoxNodeEnt>* pTree)
{
	int iDistance = 0 ;
	while (!pTree->is_root())
	{
		CString strCableString = pTree->get()->m_strCableString ;
		int iLen = _ttoi(strCableString.Left(strCableString.Find(_T("-")))) ;
		iDistance+=iLen ;
		pTree = pTree->parent() ;
	}

	return iDistance ;
}

//取得信号机、道岔到信号楼的距离
int CZnSxt::GetDistToXhl(const AcGePoint3d& pt_3d_toxhlheader, vector<vector<vector<CString> > >& vecSection)
{
	double dY1 = pt_3d_toxhlheader.y ; //距离栏最上一条线
	double dY2 = pt_3d_toxhlheader.y - m_dRowHeightToXhl ;
	double dY3 = pt_3d_toxhlheader.y - m_dRowHeightToXhl*2 ;
	double dY4 = pt_3d_toxhlheader.y - m_dRowHeightToXhl*3 ;
	double dY5 = pt_3d_toxhlheader.y - m_dRowHeightToXhl*4 ;

	double dWidthOfTextExt = 2.2 ; //文本区域所占的宽（此时文本旋转了90度，故此值应等于文本高度）
	struct resbuf* pRb ;
	long nLen = 0 ;
	CLongArray nArrayHandle;
	//将距离栏的所有文本都设为中间对齐（不移动其位置）
	ads_point pt1 = {pt_3d_toxhlheader.x+20, pt_3d_toxhlheader.y-40, 0} ;
	ads_point pt2 = {pt_3d_toxhlheader.x+m_dMaxLenPmt, pt_3d_toxhlheader.y, 0} ; 
	pRb = acutBuildList(RTDXF0, _T("TEXT"), -4,  _T(">,>,*"), 10,  pt1, -4,  _T("<,<,*"), 10,  pt2, 0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;
	for (int i=0; i<nLen; i++)
	{
		AcDbEntity* pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt))
		{
			AcDbText* pText = AcDbText::cast(pEnt) ;
			m_mysys.ConvertTextMode(pText, AcDb::kTextMid) ;
			pEnt->close() ;
		}
	}

	//先直接在距离栏搜索一遍信号机/道岔名称并搜索其匹配的距离
	CMapStringToString mapToXhl1 ;//距离信号楼距离
	for (int n=0; n<2; n++)
	{
		//所有在距离栏的信号机/道岔名称
		nLen = 0 ;
		nArrayHandle.RemoveAll() ;
		ads_point pt1 = {pt_3d_toxhlheader.x+20, pt_3d_toxhlheader.y-m_dRowHeightToXhl*2*(n+1), 0} ;
		ads_point pt2 = {pt_3d_toxhlheader.x+m_dMaxLenPmt, pt1[1]+m_dRowHeightToXhl, 0} ; 
		pRb = acutBuildList(RTDXF0, _T("TEXT"), -4,  _T(">,>,*"), 10,  pt1, -4,  _T("<,<,*"), 10,  pt2, 0) ;
		nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
		acutRelRb(pRb) ;
		acutPrintf(_T("\nall name in header:%d"), nLen) ;

		for (int j=0; j<nLen; j++)
		{
			AcDbEntity * pEnt = NULL ;
			if(m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(j), pEnt, AcDb::kForRead))
			{
				if (pEnt->isKindOf(AcDbText::desc()))
				{
					AcDbText* pText = AcDbText::cast(pEnt) ;
					CString strName = pText->textString() ;
					acutPrintf(_T("\n第1次搜索:%s："), strName) ;

					//找出位置和对起点均在指定范围的文本（距离）
					AcDbExtents ext ;
					pText->getGeomExtents(ext) ;
					AcGePoint3d pt_3d_min = ext.minPoint() ;
					AcGePoint3d pt_3d_max = ext.maxPoint() ;
					AcGePoint3d pt_3d_mid = m_mysys.GetMidPoint(pt_3d_min, pt_3d_max) ;
					dWidthOfTextExt = abs(pt_3d_max.x-pt_3d_min.x) ;
					pt1[0] = pt_3d_min.x-dWidthOfTextExt/8 ;
					pt1[1] = pt_3d_toxhlheader.y-m_dRowHeightToXhl-m_dRowHeightToXhl*2*n ;
					pt2[0] = pt_3d_max.x+dWidthOfTextExt/8 ;
					pt2[1] = pt_3d_toxhlheader.y-m_dRowHeightToXhl*2*n ;

					pRb = acutBuildList(RTDXF0, _T("TEXT"), -4,  _T(">,>,*"), 10,  pt1, -4,  _T("<,<,*"), 10,  pt2, -4,  _T(">,>,*"), 11,  pt1, -4,  _T("<,<,*"), 11,  pt2, 0) ;
					CLongArray nArrayHandleVal ;
					long nLenVal = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandleVal) ;
					if (nLenVal==1)
					{
						CString strVal ;
						m_mysys.GetTextString(nArrayHandleVal.GetAt(0), strVal) ;
						if (!strVal.IsEmpty())
						{
							acutPrintf(_T("%s："), strVal) ;
							mapToXhl1.SetAt(strName, strVal) ;
						}
					}
					acutRelRb(pRb) ;
				}
				pEnt->close() ;
			}
		}
	}
	//再利用图形中信号机/道岔的x坐标搜一遍对应的距离栏中的距离文本，对比取值
	//在header下面的实体集合
	nLen = 0 ;
	nArrayHandle.RemoveAll() ;
	pRb = acutBuildList(RTDXF0, _T("INSERT"), -4,  _T("*,<,*"), 10,  asDblArray(pt_3d_toxhlheader),  0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;
	//按x坐标增大顺序排序
	m_mysys.SortEnt(nArrayHandle, 1) ;
	vector<vector<CString> > vecToXhl ;
	for(int i=0; i<nLen; i++)
	{
		UINT nHandle =nArrayHandle.GetAt(i) ;
		AcDbEntity * pEnt = NULL ;
		if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
		{
			if (pEnt->isKindOf(AcDbBlockReference::desc()))
			{
				AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
				AcGePoint3d pt_3d_base = pRef->position() ;
				CString strBlkName ;
				m_mysys.GetBlockName(pRef, strBlkName) ;
				CString strDevName ; //信号机名称或道岔编号
				ads_point pt1 = {pt_3d_base.x-dWidthOfTextExt/2-dWidthOfTextExt/8, 0, 0}, pt2 = {pt_3d_base.x+dWidthOfTextExt/2+dWidthOfTextExt/8, 0, 0} ; //距离数值查找区域的对角点,左下角，右上角
				CString strType ;
				if (strBlkName.Left(3)==_T("XHJ"))
				{
					m_mysys.GetAttValue(pRef, _T("XHJNAME"), strDevName) ;
					pt1[1] = dY2 ;
					pt2[1] = dY1 ;
					strType = _T("信号机") ;
				}
				else if (strBlkName.Left(2)==_T("DC"))
				{
					m_mysys.GetAttValue(pRef, _T("BIANHAO"), strDevName) ;
					pt1[1] = dY4 ;
					pt2[1] = dY3 ;
					strType = _T("道岔") ;
				}

				if (!strDevName.IsEmpty())
				{
					pRb = acutBuildList(RTDXF0, _T("TEXT"), -4,  _T(">,>,*"), 10,  pt1, -4,  _T("<,<,*"), 10,  pt2, -4,  _T(">,>,*"), 11,  pt1, -4,  _T("<,<,*"), 11,  pt2, 0) ;
					int nLen2 = 0 ;
					CLongArray nArrayHandle2 ;
					nLen2 = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle2) ;
					acutRelRb(pRb) ;

					CString strDist, strDist1, strDist2;	
					mapToXhl1.Lookup(strDevName, strDist1) ;	
					strDist = strDist1 ;

					if (nLen2==1)
					{
						m_mysys.GetTextString(nArrayHandle2.GetAt(0), strDist2) ;
						if (!strDist1.IsEmpty()&&strDist1.Compare(strDist2)!=0)
						{
							acutPrintf(_T("\n在查找名为%s的设备距信号楼距离时，2次找到的结果不一致：%s-%s,暂时取一个值%s"), strDevName, strDist1, strDist2, strDist1) ;
						}
						else if (strDist1.IsEmpty())
						{
							strDist = strDist2 ;
						}
					}
					else if (nLen2==0)
					{
						if(strDist1.IsEmpty())
						{
							acutPrintf(_T("\n在查找名为%s的设备距信号楼距离时，未找到结果"), strDevName) ;
						}
					}

					vector<CString > vec ;
					vec.push_back(strType) ;
					vec.push_back(strDevName) ;
					vec.push_back(strDist) ;
				
					vecToXhl.push_back(vec) ;
					acutPrintf(_T("\n第2次搜索:%s：%s"), strDevName, strDist) ;
				}
			}

			pEnt->close() ;
		}
	}

	vecSection.push_back(vecToXhl) ;
	return 0;
}

// 取得平面图距离信号楼标题栏块的基点
//返回1，表示是大铁平面图，返回10/20表示是地铁
int CZnSxt::GetToXhlHeaderPos(AcGePoint3d& pt_3d_toxhlheader1, AcGePoint3d& pt_3d_toxhlheader2)
{
	int iRet = 0 ;
	//找到距离信号楼距离框的图块 
	int nLen = 0 ;
	struct resbuf* pRb ;
	CLongArray nArrayHandle ;
	pRb = acutBuildList(RTDXF0, _T("INSERT"), 2, _T("PMT_TOXHL_HEADER"), 0) ;
	nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
	acutRelRb(pRb) ;
	acutPrintf(_T("\n发现%d个PMT_TOXHL_HEADER块"), nLen) ;
	if (nLen>0)
	{
		for (int i=0; i<nLen; i++)
		{
			AcDbEntity* pEnt = NULL ;
			if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
			{
				AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
				CString strDwgType ;
				m_mysys.GetAttValue(pRef, _T("DWGTYPE"), strDwgType) ;
				if (strDwgType == _T("PMT"))
				{					
					pt_3d_toxhlheader1 = pRef->position() ;	
					iRet = 1 ;
					pEnt->close() ;
					break;
				}
				pEnt->close() ;
			}
		}
		if (iRet==0) //若没有找到dwgtype为PMT的，则采用靠下的
		{
			acutPrintf(_T("\n没有发现属性DWGTYPE为PMT的PMT_TOXHL_HEADER块，采用最靠下的块.")) ;
			m_mysys.SortEnt(nArrayHandle, 2) ;
			m_mysys.GetEntPosition(nArrayHandle.GetAt(nArrayHandle.GetCount()-1), pt_3d_toxhlheader1) ;	
			iRet = 1 ;
		}
	}
	else
	{
		//AfxMessageBox(_T("没有发现距信号楼距离框的平面图图块,请处理后再继续！")) ;
		CString strBlkName[2] = {_T("DT_LOC_HEADER_000"), _T("DT_LOC_HEADER_100")} ;
		AcGePoint3d pt_3d_toxhlheader ;
		iRet = 10 ;
		for (int n=0; n<2; n++)
		{
			nArrayHandle.RemoveAll() ;
			nLen = 0 ;
			pRb = acutBuildList(RTDXF0, _T("INSERT"), 2, strBlkName[n], 0) ;
			nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
			acutRelRb(pRb) ;
			acutPrintf(_T("\n发现%d个%s块"), nLen, strBlkName[n]) ;
			BOOL bFind = 0 ;
			if (nLen>0)
			{
				for (int i=0; i<nLen; i++)
				{
					AcDbEntity* pEnt = NULL ;
					if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
					{
						AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
						CString strDwgType ;
						m_mysys.GetAttValue(pRef, _T("DWGTYPE"), strDwgType) ;
						if (strDwgType == _T("PMT"))
						{					
							pt_3d_toxhlheader = pRef->position() ;	
							if (n==0)
							{
								pt_3d_toxhlheader1 = pt_3d_toxhlheader ;
								iRet+=1 ;								
							}
							else
							{
								pt_3d_toxhlheader2 = pt_3d_toxhlheader ;
								iRet+=2 ;
							}
							bFind = TRUE ;
							pEnt->close() ;
							break;
						}
						pEnt->close() ;
					}
				}
				if (bFind==FALSE) //若没有找到dwgtype为PMT的，则采用靠下的
				{
					acutPrintf(_T("\n没有发现属性DWGTYPE为PMT的%s块，采用最靠下的块."),strBlkName[n]) ;
					m_mysys.SortEnt(nArrayHandle, 2) ;
					m_mysys.GetEntPosition(nArrayHandle.GetAt(nArrayHandle.GetCount()-1), pt_3d_toxhlheader) ;	
					if (n==0)
					{
						pt_3d_toxhlheader1 = pt_3d_toxhlheader ;
						iRet+=1 ;	
					}
					else
					{
						pt_3d_toxhlheader2 = pt_3d_toxhlheader ;
						iRet+=2 ;	
					}
				}
			}
		}
	}

	if (iRet!=1&&iRet!=13)
	{
		iRet = 0 ;
		AfxMessageBox(_T("本图未发现任何符合要求的距离（公里标）栏图块（大铁平面图是1个距离栏，地铁是上下2个公里标栏）！")) ;
	}
	return iRet;
}

// 用当前面板的距离更新图上的距离（仅平面图）
int CZnSxt::ApplyDistToXhlToDwg(vector<vector<vector<CString> > > vecSection)
{
	
	AcGePoint3d pt_3d_toxhlheader1, pt_3d_toxhlheader2 ;
	int iGetHeader = GetToXhlHeaderPos(pt_3d_toxhlheader1, pt_3d_toxhlheader2) ;
	if(iGetHeader==0)
	{
		AfxMessageBox(_T("未找到距离栏！")) ;
		return 0;
	}

	struct resbuf* pRb ;
	long nLen = 0 ;
	CLongArray nArrayHandle;

	if (iGetHeader==1) //大铁
	{
		if (vecSection.size()==0)
		{
			AfxMessageBox(_T("没有数据！")) ;
			return 0 ;
		}
		vector<vector<CString> > vecToXhl ;
		vecToXhl = vecSection.at(0) ;

		double dYs[5] = {pt_3d_toxhlheader1.y, pt_3d_toxhlheader1.y - m_dRowHeightToXhl, pt_3d_toxhlheader1.y - m_dRowHeightToXhl*2, pt_3d_toxhlheader1.y - m_dRowHeightToXhl*3, pt_3d_toxhlheader1.y - m_dRowHeightToXhl*4} ;

		//距离栏下的信号机/道岔图块
		pRb = acutBuildList(RTDXF0, _T("INSERT"), -4,  _T("*,<,*"), 10,  asDblArray(pt_3d_toxhlheader1),  0) ;
		nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
		acutRelRb(pRb) ;
		//按x坐标增大顺序排序
		m_mysys.SortEnt(nArrayHandle, 1) ;

		vector<CString> vecName[2] ; //信号机/道岔名称
		vector<double> vecX[2 ]; //信号机/道岔的x坐标
		vector<int> vecHasDone[2] ;//用于记录当前信号机/道岔已经处理过了(1)
		double dMaxX = pt_3d_toxhlheader1.x ; //距离栏最右边
		for(int i=0; i<nLen; i++)
		{
			UINT nHandle =nArrayHandle.GetAt(i) ;
			AcDbEntity * pEnt = NULL ;
			if (m_mysys.OpenAcDbEntity(nArrayHandle.GetAt(i), pEnt, AcDb::kForRead))
			{
				if (pEnt->isKindOf(AcDbBlockReference::desc()))
				{
					AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
					AcGePoint3d pt_3d_base = pRef->position() ;
					CString strBlkName ;
					m_mysys.GetBlockName(pRef, strBlkName) ;
					CString strDevName ; //信号机名称或道岔编号

					CString strType ;
					if (strBlkName.Left(3)==_T("XHJ"))
					{
						m_mysys.GetAttValue(pRef, _T("XHJNAME"), strDevName) ;
						dMaxX = pt_3d_base.x ;
						vecName[0].push_back(strDevName) ;
						vecX[0].push_back(pt_3d_base.x) ;
						vecHasDone[0].push_back(0) ;
					}
					else if (strBlkName.Left(2)==_T("DC"))
					{
						m_mysys.GetAttValue(pRef, _T("BIANHAO"), strDevName) ;
						dMaxX = pt_3d_base.x ;
						vecName[1].push_back(strDevName) ;
						vecX[1].push_back(pt_3d_base.x) ;
						vecHasDone[1].push_back(0) ;
					}
				}
				pEnt->close() ;
			}
		}
		//清除距离栏
		ads_point pt1 = {pt_3d_toxhlheader1.x,0,0}, pt2 = {dMaxX+10,0,0} ;
		for (int i=0; i<4; i++)
		{
			pt1[1] = dYs[i+1] ;
			pt2[1] = dYs[i] ;
			nLen = 0 ;
			nArrayHandle.RemoveAll() ;
			pRb = acutBuildList(RTDXF0, _T("TEXT"), -4,  _T(">,>,*"), 10,  pt1, -4,  _T("<,<,*"), 10,  pt2, 0) ;
			nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
			acutRelRb(pRb) ;
			m_mysys.Erase(nArrayHandle) ;
		}

		//将vecToXhl中存储的值转换为map，便于查找
		CMapStringToString mapToXhl ;
		for (int i=0; i<vecToXhl.size(); i++)
		{
			CString strDist = vecToXhl[i][2] ;
			int iLoc = strDist.Find(_T("__")) ;
			if (iLoc!=-1)
			{
				strDist = strDist.Left(iLoc) ;
			}
			mapToXhl.SetAt(vecToXhl[i][1], strDist) ;
		}

		for (int i=0; i<2; i++)
		{
			for (int j=0; j<vecName[i].size(); j++) 
			{
				if (vecHasDone[i][j]==0)
				{
					vector<CString> vecCurName ;
					CString strDistCur ;
					if(mapToXhl.Lookup(vecName[i][j], strDistCur) )
					{
						vecCurName.push_back(vecName[i][j]) ;
						for (int n=j+1; n<vecName[i].size(); n++) //从此位置往后寻是否有并置的（x相同）
						{
							if (vecHasDone[i][n]==0)
							{
								CString strDistTmp ;
								if(mapToXhl.Lookup(vecName[i][n], strDistTmp) )
								{
									if (strDistTmp==strDistCur&&abs(vecX[i][j]-vecX[i][n])<5)
									{
										vecCurName.push_back(vecName[i][n]) ;
										vecHasDone[i][n] = 1 ;
										if (vecX[i][j]!=vecX[i][n])
										{
											acutPrintf(_T("\n注意：%s和%s距信号楼距离(%d)一致，但未并置"), vecName[i][j], vecName[i][n], strDistCur ) ;
										}
									}
								}
							}
						}

						//绘制当前距离
						//距离数值文本
						AcGePoint3d pt_3d_insert_text(vecX[i][j], dYs[0]-m_dRowHeightToXhl/2-i*m_dRowHeightToXhl*2, 0) ;
						m_mysys.AddText(strDistCur, pt_3d_insert_text, pt_3d_insert_text, AcDb::kTextCenter, AcDb::kTextVertMid, m_dHeightOfDistText, 0.8, PI/2) ;
						//信号机名称或道岔编号
						pt_3d_insert_text.y-=m_dRowHeightToXhl ;
						int iNum = vecCurName.size() ;
						for (int m=0; m<iNum; m++)
						{
							pt_3d_insert_text.x = vecX[i][j]-iNum*2.4/2+2.4/2+m*2.4 ;
							m_mysys.AddText(vecCurName[m], pt_3d_insert_text, pt_3d_insert_text, AcDb::kTextCenter, AcDb::kTextVertMid, m_dHeightOfDistText, 0.8, PI/2) ;
						}
					}
					vecHasDone[i][j] = 1 ;
				}
			}
		}

	}
	else if (iGetHeader==13) //地铁
	{
		//搜索出所有车站标，以此将整条线分成多段，每区段的设备（信号机）不会重名？
		long nNumStation = 0 ;
		CLongArray nArrayHandleStation;
		ads_point pt1 = {pt_3d_toxhlheader2.x, pt_3d_toxhlheader2.y, 0} ;
		ads_point pt2 = {pt_3d_toxhlheader1.x+m_dMaxLenPmt, pt_3d_toxhlheader1.y, 0} ; 
		pRb = acutBuildList(RTDXF0, _T("INSERT"), 2, _T("PMT_STATION"), -4,  _T(">,>,*"), 10,  pt1, -4,  _T("<,<,*"), 10,  pt2, 0) ;
		nNumStation = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandleStation) ;
		acutRelRb(pRb) ;
		//begin 各区段x
		m_mysys.SortEnt(nArrayHandleStation,1, 0) ;
		CDoubleArray dArrayX ;
		dArrayX.Add(pt_3d_toxhlheader1.x) ;
		for(int iIndexStation=0; iIndexStation<nNumStation; iIndexStation++)
		{
			AcGePoint3d pt_3d_insert ;
			m_mysys.GetEntPosition(nArrayHandleStation.GetAt(iIndexStation), pt_3d_insert) ;
			dArrayX.Add(pt_3d_insert.x) ;
		}
		dArrayX.Add(pt_3d_toxhlheader1.x+m_dMaxLenPmt) ;
		//end 各区段x
		int iNumSection = dArrayX.GetCount()-1 ;
		if (iNumSection!=vecSection.size())
		{
			AfxMessageBox(_T("当前面板中的区段数和图纸中的区段数不一致，不能将面板上的数据绘制到图中！")) ;
			return 0 ;
		}

		for(int iIndexSection=0; iIndexSection<iNumSection; iIndexSection++)
		{
			//查询该区段的设备
			pt1[0] = dArrayX.GetAt(iIndexSection) ;
			pt1[1] =pt_3d_toxhlheader2.y;
			pt2[0] = dArrayX.GetAt(iIndexSection+1)  ;
			pt2[1] = pt_3d_toxhlheader1.y;
			CLongArray nArrayHandleDev ;
			pRb = acutBuildList(RTDXF0, _T("INSERT"), -4,  _T("<,<,*"), 10,  pt2, -4,  _T(">,>,*"), 10,  pt1, 0) ;
			nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandleDev) ;
			acutRelRb(pRb) ;
			//按x坐标增大顺序排序
			m_mysys.SortEnt(nArrayHandleDev, 1) ;
			//清除距离栏
			for (int i=0; i<2; i++)
			{
				pt1[1] = (i==0?pt_3d_toxhlheader1.y:pt_3d_toxhlheader2.y-35 );
				pt2[1] = (i==0?pt_3d_toxhlheader1.y+35:pt_3d_toxhlheader2.y );
				nLen = 0 ;
				nArrayHandle.RemoveAll() ;
				pRb = acutBuildList(RTDXF0, _T("TEXT"), -4,  _T(">,>,*"), 10,  pt1, -4,  _T("<,<,*"), 10,  pt2, 0) ;
				nLen = m_mysys.GetSSEntHandle(_T("X"), NULL, NULL, pRb, nArrayHandle) ;
				acutRelRb(pRb) ;
				m_mysys.Erase(nArrayHandle) ;
			}
			//将vecToXhl中存储的值转换为map，便于查找
			CMapStringToString mapToXhl ;
			vector<vector<CString> > vecToXhl ;
			vecToXhl = vecSection.at(iIndexSection) ;
			for (int i=0; i<vecToXhl.size(); i++)
			{
				mapToXhl.SetAt(vecToXhl[i][1], vecToXhl[i][2]) ;
			}

			//处理每一个设备
			for(int i=0; i<nArrayHandleDev.GetCount(); i++)
			{
				UINT nHandle =nArrayHandleDev.GetAt(i) ;
				AcDbEntity * pEnt = NULL ;
				if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
				{
					if (pEnt->isKindOf(AcDbBlockReference::desc()))
					{
						AcDbBlockReference* pRef = AcDbBlockReference::cast(pEnt) ;
						AcGePoint3d pt_3d_base = pRef->position() ;
						CString strBlkName ;
						m_mysys.GetBlockName(pRef, strBlkName) ;
						CString strDevName ; //信号机名称或道岔编号
						CString strType ;

						if (strBlkName.Left(3)==_T("XHJ"))
						{
							m_mysys.GetAttValue(pRef, _T("XHJNAME"), strDevName) ;
						}
						else if (strBlkName.Left(2)==_T("DC"))
						{
							m_mysys.GetAttValue(pRef, _T("BIANHAO"), strDevName) ;
						}
						else 
						{
							m_mysys.GetAttValue(pRef, _T("NAME"), strDevName) ;
						}

						if (!strDevName.IsEmpty())
						{
							CString strDist;	
							if (mapToXhl.Lookup(strDevName, strDist) )
							{							
								int iLoc = strDist.Find(_T("__")) ;
								CString strDistInfact = strDist ;
								CString strUpOrDown = _T("上") ;
								if (iLoc!=-1)
								{
									strDistInfact = strDist.Left(iLoc) ;
									strUpOrDown = strDist.Mid(iLoc+2) ;
								}

								//设备名称
								AcGePoint3d pt_3d_insert_text(pt_3d_base.x, strUpOrDown==_T("上")?(pt_3d_toxhlheader1.y+6.5):(pt_3d_toxhlheader2.y-6.5), 0) ;
								m_mysys.AddText(strDevName, pt_3d_insert_text, pt_3d_insert_text, AcDb::kTextCenter, AcDb::kTextVertMid, m_dHeightOfDistText_DT, 0.55, PI/2) ;
								//公里标
								pt_3d_insert_text.y = (strUpOrDown==_T("上")?(pt_3d_toxhlheader1.y+24):(pt_3d_toxhlheader2.y-24)) ;
								m_mysys.AddText(strDistInfact, pt_3d_insert_text, pt_3d_insert_text, AcDb::kTextCenter, AcDb::kTextVertMid, m_dHeightOfDistText_DT, 0.55, PI/2) ;
							}
							else
							{
								acutPrintf(_T("\n***注意，未找到名称为%s的设备的公里标，请自行加上！")) ;
							}
						}
					}

					pEnt->close() ;
				}
			}
		}
	}
	
	return 0;
}

// 多段线起头或末段线的向量角（iStartOrEnd=0，起点为起点，=1，末端为起点），默认相对x轴(dAngleTo=0)
double CZnSxt::GetPlineAngle(long nHandle, double dAngleTo, int iStartOrEnd)
{
	double	dAngle = 0.000 ;
	AcGeVector2d vec ;
	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
	{
		if (pEnt->isKindOf(AcDbPolyline::desc()))
		{
			AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
			AcGePoint2d pt_2d_first, pt_2d_second ;
			if (iStartOrEnd==0)
			{
				pPolyLine->getPointAt(0, pt_2d_first) ;
				pPolyLine->getPointAt(1, pt_2d_second) ;
			}
			else
			{
				pPolyLine->getPointAt(pPolyLine->numVerts()-1, pt_2d_first) ;
				pPolyLine->getPointAt(pPolyLine->numVerts()-2, pt_2d_second) ;
			}
			vec = pt_2d_second-pt_2d_first ;
			dAngle = vec.angle()-dAngleTo ;
			if (dAngle<0)
			{
				dAngle += PI*2 ;
			}
		}
		pEnt->close() ;
	}	

	return dAngle;
}

// 多段线起点或终点（iStartOrEnd=0，起点，=1，终点）
void CZnSxt::GetPlinePoint(long nHandle, AcGePoint3d& pt_3d_ret, int iStartOrEnd)
{
	AcDbEntity* pEnt = NULL ;
	if (m_mysys.OpenAcDbEntity(nHandle, pEnt, AcDb::kForRead))
	{
		if (pEnt->isKindOf(AcDbPolyline::desc()))
		{
			AcDbPolyline* pPolyLine = AcDbPolyline::cast(pEnt) ;
			if (iStartOrEnd==0)
			{
				pPolyLine->getPointAt(0, pt_3d_ret) ;
				
			}
			else
			{
				pPolyLine->getPointAt(pPolyLine->numVerts()-1, pt_3d_ret) ;				
			}
		}
		pEnt->close() ;
	}	
}
// 判断两条轨道区段是否在同一水平线的轨道上
// BOOL CZnSxt::IsOntheHorizonline( long nHandleLineStart,long nHandleLineEnd,bool bForward )
// {
// 	long nHandLineTemp=nHandleLineStart;
// 	if(nHandleLineStart==273234||nHandleLineEnd==273230)
// 	{
// 		nHandleLineEnd=nHandleLineEnd;
// 	}
// 	if(nHandleLineEnd==0)
// 		return FALSE;
// 	while(nHandLineTemp!=0)
// 	{
// 		long nHandleNextLine=0;
// 		FindNextLineByJYJ(nHandLineTemp,nHandleNextLine,bForward);
// 		if (nHandleNextLine==0)
// 		{
// 			return FALSE;
// 		}
// 		else if(nHandleNextLine==nHandleLineEnd)
// 		{
// 			return TRUE;
// 		}
// 		else
// 		{
// 			nHandLineTemp=nHandleNextLine;
// 		}
// 	}
// 	return FALSE;
// }
// 
// void CZnSxt::LsbTest()
// {
// 	CString strFilePath ;
// 	strFilePath.Format(_T("%s\\support\\xls\\平面图联锁表模板.xls"), g_strBasePath);
// 	acutPrintf(_T("\n g_strBasePath=%s"),g_strBasePath);
// 	oper_excel.OpenExcel(strFilePath);
// 	//清空excel表
// 	oper_excel.ClearAllCells(1);
// 	//oper_excel.SaveExcel();
// 	CString strFilter = _T("(文件(*.xls)|*.xls|");
// 	CFileDialog fileDlg(FALSE, NULL, NULL, OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR, strFilter);
// 	fileDlg.m_ofn.lpstrTitle = _T("输出联锁表至");
// 	TCHAR szBuf[256];
// 	memset(szBuf, 0, sizeof(szBuf));
// 	GetCurrentDirectory(sizeof(szBuf), szBuf);
// 	fileDlg.m_ofn.lpstrInitialDir = szBuf;
// 	CString strFileSave;
// 	if(IDOK==fileDlg.DoModal())
// 	{
// 		strFileSave = fileDlg.GetPathName();
// 		CString strFilePathTmp = strFileSave;
// 		strFilePathTmp.MakeLower() ;
// 		if (strFilePathTmp.Right(4)!=_T(".xls"))
// 		{
// 			strFileSave.Append(_T(".xls")) ;
// 		}
// 	}	
// 	
// 	strFileSave.Format(_T("%s\\support\\xls\\平面图联锁表.xls"), g_strBasePath) ;
// 	oper_excel.SaveasXSLFile(strFileSave);
// 	oper_excel.RealseExcel();
// 
// }

BOOL CZnSxt::DT_IsFromSameDevice( long nHandleDev1,long nHandleDev2 )
{
	//先判断Dev1是否属于Dev2
	long nHandle_BelongEnt1=0;
	long nHandle_BelongEnt2=0;
	if(nHandleDev1==0||nHandleDev2==0)
	{
		return FALSE;
	}
	if(m_mysys.GetXdata(nHandleDev1,_T("CloneFrom"),nHandle_BelongEnt1))
	{
		if(nHandleDev2==nHandle_BelongEnt1)
		{
			return TRUE;
		}
	}
	if(m_mysys.GetXdata(nHandleDev1,_T("CloneFrom"),nHandle_BelongEnt1))
	{
		if(nHandleDev2==nHandle_BelongEnt1)
		{
			return TRUE;
		}
	}
	if(m_mysys.GetXdata(nHandleDev1,_T("BELONG_ENT_PMT"),nHandle_BelongEnt1))
	{
		if(nHandleDev2==nHandle_BelongEnt1)
		{
			return TRUE;
		}
	}
	if(m_mysys.GetXdata(nHandleDev1,_T("BELONG_ENT"),nHandle_BelongEnt1))
	{
		if(nHandleDev2==nHandle_BelongEnt1)
		{
			return TRUE;
		}
	}
	if(m_mysys.GetXdata(nHandleDev2,_T("BELONG_ENT_PMT"),nHandle_BelongEnt2))
	{
		if(nHandleDev1==nHandle_BelongEnt2)
		{
			return TRUE;
		}
	}
	if(m_mysys.GetXdata(nHandleDev2,_T("Clone_From"),nHandle_BelongEnt2))
	{
		if(nHandleDev1==nHandle_BelongEnt2)
		{
			return TRUE;
		}
	}
	if(m_mysys.GetXdata(nHandleDev2,_T("BELONG_ENT"),nHandle_BelongEnt2))
	{
		if(nHandleDev1==nHandle_BelongEnt2)
		{
			return TRUE;
		}
	}
	//还有一种情况，同属于pmt中的某个设备
	if(m_mysys.GetXdata(nHandleDev1,_T("BELONG_ENT"),nHandle_BelongEnt1))
	{
		if(m_mysys.GetXdata(nHandleDev2,_T("BELONG_ENT"),nHandle_BelongEnt2))
		{
			if(nHandle_BelongEnt1==nHandle_BelongEnt2)
			{
				return TRUE;
			}
		}
	}
	if(m_mysys.GetXdata(nHandleDev1,_T("BELONG_ENT_PMT"),nHandle_BelongEnt1))
	{
		if(m_mysys.GetXdata(nHandleDev2,_T("BELONG_ENT_PMT"),nHandle_BelongEnt2))
		{
			if(nHandle_BelongEnt1==nHandle_BelongEnt2)
			{
				return TRUE;
			}
		}
	}
	return FALSE;

}

void CZnSxt::InitLsbDatabase()
{
	try
	{
// 		vector<int> i_vec;
// 		const int a=3;
// 		i_vec.push_back(a);

		CADORecordset cSet(&m_AdoDb) ;
		CString strSql ;
		strSql.Format(_T("select distinct sxt_line_handle from sxt_line where 1=1")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_line_handle"), nHandle) ;
				if(nHandle!=0)
				{
					CGuiDaoNode myGuiDao(nHandle);
					vec_GuiDaoInfo.push_back(myGuiDao);
					cSet.MoveNext() ;
				}
			}
					cSet.Close() ;
		}
		strSql.Format(_T("select * from sxt_ent where sxt_ent_block='DC_000' or sxt_ent_block='DC_00'")) ;
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				if(nHandle!=0)
				{
					CDaocha myDaocha(nHandle);
					vec_DaochaInfo.push_back(myDaocha);
					cSet.MoveNext() ;
				}
				//cSet.Close() ;
			}
			cSet.Close();
		}
		strSql.Format(_T("select * from sxt_ent where sxt_ent_type>=100 and sxt_ent_type<200"));
		if (cSet.Open(strSql))
		{
			while(!cSet.IsEOF())
			{
				long nHandle = 0 ;
				cSet.GetFieldValue(_T("sxt_ent_handle"), nHandle) ;
				if(nHandle!=0)
				{
					//CDaocha myDaocha(nHandle);
					//vec_DaochaInfo.push_back(myDaocha);
					CXhjDataNode myXhjData(nHandle);
					vec_XhjInfo.push_back(myXhjData);
					cSet.MoveNext() ;
				}
				//cSet.Close() ;
			}
			cSet.Close();
		}

// 		for(int i=0;i<vec_DaochaInfo.size();i++)
// 		{
// // 			CString str_ano_name;
// // 			CString str_jiaocha_min;
// // 			CString str_jiaocha_max;
// // 			this->GetEntName(vec_DaochaInfo[i].m_nHandle_Another_DC,str_ano_name);
// // 			this->GetEntName(vec_DaochaInfo[i].m_nHandle_Jiaocha_DCMin,str_jiaocha_min);
// // 			this->GetEntName(vec_DaochaInfo[i].m_nHandle_Jiaocha_DCMax,str_jiaocha_max);
// 			acutPrintf(_T("\n name=%s,ano_name=%s,name_min=%s,name_max=%s,jiaocha_name_min=%s,jiaocha_name_max=%s,daidong=%s"),vec_DaochaInfo[i].m_strDcName,vec_DaochaInfo[i].m_str_Another_DC,vec_DaochaInfo[i].m_strDC_Min,vec_DaochaInfo[i].m_strDC_Max,vec_DaochaInfo[i].m_str_Jiaocha_DCMin,vec_DaochaInfo[i].m_str_Jiaocha_DCMax,vec_DaochaInfo[i].m_strDaidong);
// 		}
// 		for(int i=0;i<vec_GuiDaoInfo.size();i++)
// 		{
// 			acutPrintf(_T("\n %d name=%s,handleLeft=%d,handleRight=%d,lineType=%d"),i,vec_GuiDaoInfo[i].m_strGuiDaoName,vec_GuiDaoInfo[i].m_nHandleNextLineLeft,vec_GuiDaoInfo[i].m_nHandleNextLineRight,vec_GuiDaoInfo[i].m_iLineType);
// 			vector<long> vec_dc=vec_GuiDaoInfo[i].m_vecThisDaocha_Asc;
// 			for(int k=0;k<vec_dc.size();k++)
// 			{
// 				CString str_DCName;
// 				this->GetEntName(vec_dc[k],str_DCName);
// 				acutPrintf(_T("\t DC=%s"),str_DCName);
// 			}
// 		}
		for(int i=0;i<vec_XhjInfo.size();i++)
		{
			acutPrintf(_T("\n xhj_name=%s,xhj_blk=%s,conline_left=%d,conline_right=%d"),vec_XhjInfo[i].m_strXhjName,vec_XhjInfo[i].m_strBlockName,vec_XhjInfo[i].m_nHandleXhjLeftLine,vec_XhjInfo[i].m_nHandleXhjRightLine);
		}
	}

	catch(_com_error& e)
	{
		CString sBuff = CBlkUtility::GetErrorDescription(e);
		AfxMessageBox(sBuff);        
	}
	catch(...)
	{	
		AfxMessageBox(_T("未知的错误!in InitLsbDatabase"));
	}

}

int CZnSxt::FindDaoChaHandleFromVector( long nHandle )
{
	int i=vec_DaochaInfo.size();
	if(i==0)
	{
		return -1;
	}
	for(int k=0;k<i;k++)
	{
		if(nHandle==vec_DaochaInfo[k].m_nHandleDC)
		{
			return k;
		}
	}
	return -1;


}
int CZnSxt::FindGuiDaoHandleFromVector( long nHandle )
{
	int i=this->vec_GuiDaoInfo.size();
	if(i==0)
	{
		return -1;
	}
	for(int k=0;k<i;k++)
	{
		if(nHandle==this->vec_GuiDaoInfo[k].m_nHandleLine)
		{
			return k;
		}
	}
	return -1;


}
int CZnSxt::FindXhjHandleFromVector( long nHandleXhj )
{
	int i=this->vec_XhjInfo.size();
	if(i==0)
	{
		return -1;
	}
	for(int k=0;k<i;k++)
	{
		if(nHandleXhj==this->vec_XhjInfo[k].m_nHandleXhj)
		{
			return k;
		}
	}
	return -1;


}




