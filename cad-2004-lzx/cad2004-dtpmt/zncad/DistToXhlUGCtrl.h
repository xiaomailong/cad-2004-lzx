#pragma once

#include "UGCtrl.h"

// CDistToXhlUGCtrl

//cell 'param' values, used as a style field
// #define DROPLIST_JGDRAG_LOAD		110
// #define DROPLIST_XGDRAG_LOAD		111

#define CELLTYPE_IS_EDITABLE		120
#define CELLTYPE_ISNOT_EDITABLE	0


class CDistToXhlUGCtrl : public CUGCtrl
{
	DECLARE_DYNAMIC(CDistToXhlUGCtrl)

public:
	CDistToXhlUGCtrl();
	virtual ~CDistToXhlUGCtrl();

protected:
	DECLARE_MESSAGE_MAP()
public:
		//***** Over-ridable Notify Functions *****
	virtual void OnSetup();
	//GetCellIndirect notification
	virtual void OnGetCell(int col,long row,CUGCell *cell);
	//cell type notifications
	virtual int OnCellTypeNotify(long ID,int col,long row,long msg,LONG_PTR param);
	virtual void OnDClicked(int col,long row,RECT *rect,POINT *point,BOOL processed);
	void SetupSection(long nRow, LPCTSTR lpcstrTitle);

	//fonts
	CFont	m_defFont;
	CFont	m_boldFont;
	CFont	m_medFont;
	CFont	m_medBoldFont;
	CFont	m_lrgFont;
	CFont	m_smFont;
	double QuickGetNumber(int nCol, int nRow);
	int	QuickGetNumber(int nCol,int nRow, int& number);
	int	QuickGetNumber(int nCol, int nRow, double& number);
	BOOL QuickGetBool(int nCol, int nRow);

	int SetCellParam(int nCol, int nRow, int nParam) ;

	// -----------------------------------------------------------------------------
	virtual int OnEditFinish(int  col, long  row, CWnd * edit, LPCTSTR  string, BOOL  cancelFlag);
	// -----------------------------------------------------------------------------
	virtual int OnEditStart(int  col, long  row, CWnd ** edit);
	// �ڵ�Ԫ��༭��ʼʱ��¼��ʱ���ı�
	CString m_strCellPreText;
	int m_iItemIsNull[30];	

	vector<vector<vector<CString> > > m_vecToXhl ;
	// �����ݴ�Grid����
	int OutData(vector<vector<vector<CString> > >& vecToXhl);
	// ����紫�����ݸ���Grid
	int InData(const vector<vector<vector<CString> > >& vecToXhl);
};

